#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/conversion.h"
#include "../include/error.h"
#include "../include/util.h"

#define const_typecomp(a, b) ((PTRS_TYPE_##a << 3) | PTRS_TYPE_##b)
#define typecomp(a, b) ((a << 3) | b)
#define jit_typecomp(func, a, b) (jit_insn_or(func, jit_insn_shl(func, a, jit_const_int(func, nuint, 3)), b))

static jit_type_t getIntrinsicSignature()
{
	static jit_type_t signature = NULL;
	if(signature == NULL)
	{
		jit_type_t argDef[5] = {
			jit_type_void_ptr,
			jit_type_long,
			jit_type_ulong,
			jit_type_long,
			jit_type_ulong
		};
		signature = jit_type_create_signature(jit_abi_cdecl, ptrs_jit_getVarType(), argDef, 5, 0);
	}

	return signature;
}

#define binary_add_cases \
	case const_typecomp(NATIVE, INT): \
		ret.meta.type = PTRS_TYPE_NATIVE; \
		ret.meta.array.size = leftMeta.array.size - right.intval; \
		ret.value.nativeval = left.nativeval + right.intval; \
		break; \
	case const_typecomp(INT, NATIVE): \
		ret.meta.type = PTRS_TYPE_NATIVE; \
		ret.meta.array.size = rightMeta.array.size - left.intval; \
		ret.value.nativeval = left.nativeval + right.intval; \
		break; \
	case const_typecomp(POINTER, INT): \
		ret.meta.type = PTRS_TYPE_POINTER; \
		ret.meta.array.size = leftMeta.array.size - right.intval; \
		ret.value.nativeval = left.ptrval + right.intval; \
		break; \
	case const_typecomp(INT, POINTER): \
		ret.meta.type = PTRS_TYPE_POINTER; \
		ret.meta.array.size = rightMeta.array.size - left.intval; \
		ret.value.nativeval = left.intval + right.ptrval; \
		break;

#define binary_sub_cases \
	case const_typecomp(NATIVE, INT): \
		ret.meta.type = PTRS_TYPE_NATIVE; \
		ret.meta.array.size = leftMeta.array.size + right.intval; \
		ret.value.nativeval = left.nativeval - right.intval; \
		break; \
	case const_typecomp(NATIVE, NATIVE): \
		ret.meta.type = PTRS_TYPE_INT; \
		ret.value.intval = left.nativeval - right.nativeval; \
		break; \
	case const_typecomp(POINTER, INT): \
		ret.meta.type = PTRS_TYPE_POINTER; \
		ret.meta.array.size = leftMeta.array.size - right.intval; \
		ret.value.ptrval = left.ptrval - right.intval; \
		break; \
	case const_typecomp(POINTER, POINTER): \
		ret.meta.type = PTRS_TYPE_INT; \
		ret.value.intval = left.ptrval - right.ptrval; \
		break;

#define binary_add_jit_cases \
	case const_typecomp(NATIVE, INT): \
		left.constType = PTRS_TYPE_NATIVE; \
		left.meta = ptrs_jit_setArraySize(func, left.meta, \
			jit_insn_sub(func, ptrs_jit_getArraySize(func, left.meta), right.val) \
		); \
		left.val = jit_insn_add(func, left.val, right.val); \
		break; \
	case const_typecomp(INT, NATIVE): \
		left.constType = PTRS_TYPE_NATIVE; \
		left.meta = ptrs_jit_setArraySize(func, right.meta, \
			jit_insn_sub(func, ptrs_jit_getArraySize(func, right.meta), left.val) \
		); \
		left.val = jit_insn_add(func, left.val, right.val); \
		break; \
	case const_typecomp(POINTER, INT): \
		left.constType = PTRS_TYPE_POINTER; \
		left.meta = ptrs_jit_setArraySize(func, left.meta, \
			jit_insn_sub(func, ptrs_jit_getArraySize(func, left.meta), right.val) \
		); \
		left.val = jit_insn_add(func, left.val, \
			jit_insn_shl(func, right.val, jit_const_int(func, int, 4))); \
		break; \
	case const_typecomp(INT, POINTER): \
		left.constType = PTRS_TYPE_POINTER; \
		left.meta = ptrs_jit_setArraySize(func, right.meta, \
			jit_insn_sub(func, ptrs_jit_getArraySize(func, right.meta), left.val) \
		); \
		left.val = jit_insn_add(func, \
			jit_insn_shl(func, left.val, jit_const_int(func, int, 4)), right.val); \
		break;

#define binary_sub_jit_cases \
	case const_typecomp(NATIVE, INT): \
		left.constType = PTRS_TYPE_NATIVE; \
		left.meta = ptrs_jit_setArraySize(func, left.meta, \
			jit_insn_add(func, ptrs_jit_getArraySize(func, left.meta), right.val) \
		); \
		left.val = jit_insn_sub(func, left.val, right.val); \
		break; \
	case const_typecomp(NATIVE, NATIVE): \
		left.constType = PTRS_TYPE_INT; \
		left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT); \
		left.val = jit_insn_sub(func, left.val, right.val); \
		break; \
	case const_typecomp(POINTER, INT): \
		left.constType = PTRS_TYPE_POINTER; \
		left.meta = ptrs_jit_setArraySize(func, left.meta, \
			jit_insn_add(func, ptrs_jit_getArraySize(func, left.meta), right.val) \
		); \
		left.val = jit_insn_sub(func, left.val, right.val); \
		break; \
	case const_typecomp(POINTER, POINTER): \
		left.constType = PTRS_TYPE_INT; \
		left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT); \
		left.val = jit_insn_sub(func, left.val, right.val); \
		break;

#define handle_binary_intrinsic(name, operator, jitOp, cases, jitCases) \
	ptrs_var_t ptrs_intrinsic_##name(ptrs_ast_t *node, ptrs_val_t left, ptrs_meta_t leftMeta, \
		ptrs_val_t right, ptrs_meta_t rightMeta) \
	{ \
		ptrs_var_t ret; \
		switch(typecomp(leftMeta.type, rightMeta.type)) \
		{ \
			case const_typecomp(INT, INT): \
				ret.meta.type = PTRS_TYPE_INT; \
				ret.value.intval = left.intval operator right.intval; \
				break; \
			case const_typecomp(FLOAT, INT): \
				ret.meta.type = PTRS_TYPE_FLOAT; \
				ret.value.floatval = left.floatval operator right.intval; \
				break; \
			case const_typecomp(INT, FLOAT): \
				ret.meta.type = PTRS_TYPE_FLOAT; \
				ret.value.floatval = left.intval operator right.floatval; \
				break; \
			case const_typecomp(FLOAT, FLOAT): \
				ret.meta.type = PTRS_TYPE_FLOAT; \
				ret.value.floatval = left.floatval operator right.floatval; \
				break; \
			cases \
			default: \
				ptrs_error(node, "Cannot use operator " #operator " on variables of type %t and %t", \
					leftMeta.type, rightMeta.type); \
				break; \
		} \
		return ret; \
	} \
	\
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope); \
		ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope); \
		\
		if(left.constType != -1 && right.constType != -1) \
		{ \
			if(jit_value_is_constant(left.val) && jit_value_is_constant(right.val)) \
			{ \
				ptrs_var_t val = ptrs_intrinsic_##name(node, \
					ptrs_jit_value_getValConstant(left.val), \
					ptrs_jit_value_getMetaConstant(left.meta), \
					ptrs_jit_value_getValConstant(right.val), \
					ptrs_jit_value_getMetaConstant(right.meta) \
				); \
				\
				ptrs_jit_var_t ret = { \
					.meta = jit_const_long(func, ulong, *(uint64_t *)&val.meta), \
					.constType = val.meta.type, \
				}; \
				\
				if(val.meta.type == PTRS_TYPE_FLOAT) \
					ret.val = jit_const_float(func, val.value.floatval); \
				else \
					ret.val = jit_const_long(func, long, val.value.intval); \
				\
				return ret; \
			} \
			else \
			{ \
				switch(typecomp(left.constType, right.constType)) \
				{ \
					case const_typecomp(INT, INT): \
						left.constType = PTRS_TYPE_INT; \
						left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT); \
						left.val = jit_insn_##jitOp(func, left.val, right.val); \
						break; \
					case const_typecomp(FLOAT, INT): \
						left.constType = PTRS_TYPE_FLOAT; \
						left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT); \
						left.val = jit_insn_##jitOp(func, \
							ptrs_jit_reinterpretCast(func, left.val, jit_type_float64), \
							jit_insn_convert(func, right.val, jit_type_float64, 0) \
						); \
						break; \
					case const_typecomp(INT, FLOAT): \
						left.constType = PTRS_TYPE_FLOAT; \
						left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT); \
						left.val = jit_insn_##jitOp(func, \
							jit_insn_convert(func, left.val, jit_type_float64, 0), \
							ptrs_jit_reinterpretCast(func, right.val, jit_type_float64) \
						); \
						break; \
					case const_typecomp(FLOAT, FLOAT): \
						left.constType = PTRS_TYPE_FLOAT; \
						left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT); \
						left.val = jit_insn_##jitOp(func, \
							ptrs_jit_reinterpretCast(func, left.val, jit_type_float64), \
							ptrs_jit_reinterpretCast(func, right.val, jit_type_float64) \
						); \
						break; \
					jitCases \
					default: \
						ptrs_error(node, "Cannot use operator " #operator " on variables of type %t and %t", \
							left.constType, right.constType); \
						break; \
				} \
				return left; \
			} \
		} \
		\
		jit_value_t args[5] = { \
			jit_const_int(func, void_ptr, (uintptr_t)node), \
			ptrs_jit_reinterpretCast(func, left.val, jit_type_long), \
			left.meta, \
			ptrs_jit_reinterpretCast(func, right.val, jit_type_long), \
			right.meta \
		}; \
		\
		jit_value_t retVal = jit_insn_call_native(func, "(op " #operator ")", \
			ptrs_intrinsic_##name, getIntrinsicSignature(), args, 5, 0); \
		\
		ptrs_jit_var_t ret = ptrs_jit_valToVar(func, retVal); \
		if(left.constType == PTRS_TYPE_FLOAT || right.constType == PTRS_TYPE_FLOAT) \
			ret.constType = PTRS_TYPE_FLOAT; \
		\
		return ret; \
	}

#define handle_binary_intonly(name, opName, operator) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope); \
		ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope); \
		\
		if(left.constType != -1 && right.constType != -1) \
		{ \
			if(left.constType != PTRS_TYPE_INT || right.constType != PTRS_TYPE_INT) \
			{ \
				ptrs_error(node, "Cannot use operator " #operator " on variables of type %t and %t", \
					left.constType, right.constType); \
			} \
		} \
		else \
		{ \
			jit_value_t leftType = ptrs_jit_getType(func, left.meta); \
			jit_value_t rightType = ptrs_jit_getType(func, right.meta); \
			\
			struct ptrs_assertion *assertion = ptrs_jit_assert(node, func, scope, \
				jit_insn_eq(func, leftType, jit_const_int(func, nuint, PTRS_TYPE_INT)), \
				2, "Cannot use operator " #operator " on variables of type %t and %t", leftType, rightType \
			); \
			ptrs_jit_appendAssert(func, assertion, \
				jit_insn_eq(func, rightType, jit_const_int(func, nuint, PTRS_TYPE_INT))); \
		} \
		\
		right.val = jit_insn_##opName(func, left.val, right.val); \
		return right; \
	}

#define handle_binary_typecompare(comparer, constOp) \
	if(left.constType != -1 && right.constType != -1) \
	{ \
		if(!(left.constType constOp right.constType)) \
			left.val = jit_const_int(func, long, 0); \
	} \
	else \
	{ \
		left.val = jit_insn_and(func, left.val, \
			jit_insn_##comparer(func, \
				ptrs_jit_getType(func, left.meta), \
				ptrs_jit_getType(func, right.meta)) \
		); \
	}


#define handle_binary_compare(name, comparer, extra) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope); \
		ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope); \
		\
		if(left.constType == PTRS_TYPE_FLOAT && right.constType == PTRS_TYPE_FLOAT) \
		{ \
			left.val = jit_insn_##comparer(func, \
				ptrs_jit_reinterpretCast(func, left.val, jit_type_float64), \
				ptrs_jit_reinterpretCast(func, right.val, jit_type_float64) \
			); \
		} \
		else if(left.constType == PTRS_TYPE_FLOAT) \
		{ \
			left.val = jit_insn_##comparer(func, \
				ptrs_jit_reinterpretCast(func, left.val, jit_type_float64), \
				ptrs_jit_vartof(func, right) \
			); \
		} \
		else if(right.constType == PTRS_TYPE_FLOAT) \
		{ \
			left.val = jit_insn_##comparer(func, \
				ptrs_jit_vartof(func, left), \
				ptrs_jit_reinterpretCast(func, right.val, jit_type_float64) \
			); \
		} \
		else \
		{ \
			left.val = jit_insn_##comparer(func, left.val, right.val); \
		} \
		\
		extra \
		\
		left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT); \
		left.constType = PTRS_TYPE_INT; \
		return left; \
	}

#define handle_binary_logic(name, comparer, constComparer) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		/*evaluate the left-side expression*/ \
		ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope); \
		if(jit_value_is_constant(left.val) && jit_value_is_constant(left.meta)) \
		{ \
			jit_long constVal = jit_value_get_long_constant(ptrs_jit_vartob(func, left)); \
			if(constVal constComparer true) \
				return expr->right->vtable->get(expr->right, func, scope); \
			\
			ptrs_jit_var_t ret = { \
				.val = jit_const_long(func, long, 0), \
				.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT), \
				.constType = PTRS_TYPE_INT, \
			}; \
			return ret; \
		} \
		\
		ptrs_jit_var_t tmp = { \
			.val = jit_value_create(func, jit_type_long), \
			.meta = jit_value_create(func, jit_type_ulong), \
			.constType = -1, \
		}; \
		jit_insn_store(func, tmp.val, left.val); \
		jit_insn_store(func, tmp.meta, left.meta); \
		\
		/*conditionally jump over the evaluation of the right-side expression*/ \
		jit_label_t skip = jit_label_undefined; \
		ptrs_jit_branch_##comparer(func, &skip, left); \
		\
		/*overwrite the return value with the right-side expression*/ \
		ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope); \
		jit_insn_store(func, tmp.val, right.val); \
		jit_insn_store(func, tmp.meta, right.meta); \
		\
		jit_insn_label(func, &skip); \
		return tmp; \
	}

handle_binary_compare(typeequal, eq, handle_binary_typecompare(eq, ==)) //===
handle_binary_compare(typeinequal, ne, handle_binary_typecompare(ne, !=)) //!==
handle_binary_compare(equal, eq, ) //==
handle_binary_compare(inequal, ne, ) //!=
handle_binary_compare(lessequal, le, ) //<=
handle_binary_compare(greaterequal, ge, ) //>=
handle_binary_compare(less, lt, ) //<
handle_binary_compare(greater, gt, ) //>
handle_binary_intonly(or, or, |) //|
handle_binary_intonly(xor, xor, ^) //^
handle_binary_intonly(and, and, &) //&
handle_binary_intonly(shr, sshr, >>) //>>
handle_binary_intonly(shl, shl, <<) //<<
handle_binary_intrinsic(add, +, add, binary_add_cases, binary_add_jit_cases) //+
handle_binary_intrinsic(sub, -, sub, binary_sub_cases, binary_sub_jit_cases) //-
handle_binary_intrinsic(mul, *, mul, , ) //*
handle_binary_intrinsic(div, /, div, , ) ///
handle_binary_intonly(mod, rem, %) //%
handle_binary_logic(logicor, if, !=) //||
handle_binary_logic(logicand, if_not, ==) //&&
