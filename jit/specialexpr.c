#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/call.h"
#include "include/struct.h"
#include "include/util.h"
#include "include/run.h"
#include "include/astlist.h"
#include "jit/jit-insn.h"
#include "jit/jit-type.h"
#include "jit/jit-value.h"

ptrs_jit_var_t ptrs_handle_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_call *expr = &node->arg.call;

	if(expr->value->vtable->call != NULL)
	{
		return expr->value->vtable->call(expr->value, func, scope, node, &expr->typing, expr->arguments);
	}
	else
	{
		ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);
		return ptrs_jit_call(node, func, scope,
			&expr->typing, jit_const_int(func, void_ptr, 0), val, expr->arguments);
	}

}

ptrs_jit_var_t ptrs_handle_stringformat(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_strformat *expr = &node->arg.strformat;

	size_t argCount = expr->insertionCount + 3;
	jit_type_t argDef[argCount];
	jit_value_t args[argCount];

	argDef[0] = jit_type_void_ptr;
	argDef[1] = jit_type_nuint;
	argDef[2] = jit_type_void_ptr;
	args[2] = jit_const_int(func, void_ptr, (uintptr_t)expr->str);

	struct ptrs_stringformat *curr = expr->insertions;
	for(int i = 3; i < argCount; i++)
	{
		ptrs_jit_var_t val = curr->entry->vtable->get(curr->entry, func, scope);

		if(curr->convert)
			val = ptrs_jit_vartoa(func, val);

		argDef[i] = jit_value_get_type(val.val);
		args[i] = val.val;
		curr = curr->next;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_sys_int, argDef, argCount, 0);

	args[0] = jit_const_int(func, void_ptr, 0);
	args[1] = jit_const_int(func, nuint, 0);
	jit_value_t len = jit_insn_call_native(func, "snprintf", snprintf, signature, args, argCount, 0);

	len = jit_insn_add(func, len, jit_const_int(func, sys_int, 1));
	jit_value_t buff = jit_insn_alloca(func, len);

	args[0] = buff;
	args[1] = len;
	jit_insn_call_native(func, "snprintf", snprintf, signature, args, argCount, 0);

	ptrs_jit_var_t ret = {
		.val = buff,
		.meta = ptrs_jit_arrayMetaKnownType(func, len, PTRS_NATIVETYPE_INDEX_CHAR),
		.constType = PTRS_TYPE_POINTER,
	};
	return ret;
}

ptrs_jit_var_t ptrs_handle_new(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_new *expr = &node->arg.newexpr;
	ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);

	return ptrs_struct_construct(node, func, scope,
		val, expr->arguments, expr->onStack);
}

ptrs_jit_var_t ptrs_handle_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_member *expr = &node->arg.member;
	ptrs_jit_var_t base = expr->base->vtable->get(expr->base, func, scope);
	jit_value_t key = jit_const_int(func, void_ptr, (uintptr_t)expr->name);
	jit_value_t keyLen = jit_const_int(func, int, expr->namelen);

	return ptrs_jit_struct_get(node, func, scope, base, key, keyLen);
}
void ptrs_assign_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t val)
{
	struct ptrs_ast_member *expr = &node->arg.member;
	ptrs_jit_var_t base = expr->base->vtable->get(expr->base, func, scope);
	jit_value_t key = jit_const_int(func, void_ptr, (uintptr_t)expr->name);
	jit_value_t keyLen = jit_const_int(func, int, expr->namelen);

	ptrs_jit_struct_set(node, func, scope, base, key, keyLen, val);
}
ptrs_jit_var_t ptrs_addressof_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_member *expr = &node->arg.member;
	ptrs_jit_var_t base = expr->base->vtable->get(expr->base, func, scope);
	jit_value_t key = jit_const_int(func, void_ptr, (uintptr_t)expr->name);
	jit_value_t keyLen = jit_const_int(func, int, expr->namelen);

	return ptrs_jit_struct_addressof(node, func, scope, base, key, keyLen);
}
ptrs_jit_var_t ptrs_call_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments)
{
	struct ptrs_ast_member *expr = &node->arg.member;
	ptrs_jit_var_t base = expr->base->vtable->get(expr->base, func, scope);
	jit_value_t key = jit_const_int(func, void_ptr, (uintptr_t)expr->name);
	jit_value_t keyLen = jit_const_int(func, int, expr->namelen);

	return ptrs_jit_struct_call(node, func, scope, base, key, keyLen, typing, arguments);
}

ptrs_jit_var_t ptrs_handle_prefix_sizeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;

	ptrs_jit_var_t val = node->vtable->get(node, func, scope);

	ptrs_jit_var_t ret = {
		.val = jit_value_create(func, jit_type_long),
		.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT),
		.constType = PTRS_TYPE_INT
	};

	ptrs_jit_typeSwitch(node, func, scope, val,
		(1, "Cannot get the size of a value of type %t", TYPESWITCH_TYPE),
		(PTRS_TYPE_POINTER, PTRS_TYPE_STRUCT),
		case PTRS_TYPE_POINTER:
			;
			jit_value_t arraySize = ptrs_jit_getArraySize(func, val.meta);
			jit_insn_store(func, ret.val, arraySize);
			break;

		case PTRS_TYPE_STRUCT:
			if(jit_value_is_constant(val.meta))
			{
				ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
				ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
				jit_insn_store(func, ret.val, jit_const_long(func, long, struc->size));
			}
			else
			{
				jit_value_t struc = ptrs_jit_getMetaPointer(func, val.meta);
				jit_insn_store(func, ret.val, jit_insn_load_relative(func, struc,
					offsetof(ptrs_struct_t, size), jit_type_uint));
			}
			break;
	);

	return ret;
}

ptrs_jit_var_t ptrs_handle_prefix_address(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;

	if(node->vtable->addressof == NULL)
		ptrs_error(node, "Cannot get address of temporary or constant value");

	return node->vtable->addressof(node, func, scope);
}

ptrs_var_t ptrs_intrinsic_prefix_dereference(ptrs_ast_t *node, ptrs_val_t value, ptrs_meta_t meta)
{
	if(meta.array.size <= 0)
		ptrs_error(node, "Attempting to dereference an array of size 0");

	ptrs_var_t result;
	ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, meta);
	type->getHandler(value.ptrval, type->size, &result);

	return result;
}
ptrs_jit_var_t ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t val = node->vtable->get(node, func, scope);
	ptrs_jit_typeCheck(node, func, scope, val, PTRS_TYPE_POINTER, "Cannot dereference value of type %t");

	ptrs_jit_var_t ret;
	ret.addressable = false;

	if(jit_value_is_constant(val.meta))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
		ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, meta);

		if(meta.array.size <= 0)
			ptrs_error(node, "Attempting to dereference an array of size 0");

		if(type->varType == (uint8_t)-1)
		{
			ret.val = jit_insn_load_relative(func, val.val, 0, jit_type_long);
			ret.meta = jit_insn_load_relative(func, val.val, sizeof(ptrs_val_t), jit_type_ulong);
			ret.constType = -1;
		}
		else if(type->varType == PTRS_TYPE_FLOAT)
		{
			jit_value_t result = jit_insn_load_relative(func, val.val, 0, type->jitType);
			ret.val = jit_insn_convert(func, result, jit_type_float64, 0);
			ret.meta = ptrs_jit_const_meta(func, type->varType);
			ret.constType = type->varType;
		}
		else
		{
			jit_value_t result = jit_insn_load_relative(func, val.val, 0, type->jitType);
			ret.val = jit_insn_convert(func, result, jit_type_long, 0);
			ret.meta = ptrs_jit_const_meta(func, type->varType);
			ret.constType = type->varType;
		}

	}
	else
	{
		jit_value_t result;
		ptrs_jit_reusableCall(func, ptrs_intrinsic_prefix_dereference, result, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_void_ptr, jit_type_ulong),
			(jit_const_int(func, void_ptr, (uintptr_t)node), val.val, val.meta)
		);

		ret = ptrs_jit_valToVar(func, result);
	}

	return ret;
}

void ptrs_intrinsic_assign_prefix_dereference(ptrs_ast_t *node, ptrs_val_t addr, ptrs_meta_t meta, ptrs_val_t value, ptrs_meta_t valMeta)
{
	ptrs_var_t valueVar;
	valueVar.value = value;
	valueVar.meta = valMeta;

	if(meta.array.size <= 0)
		ptrs_error(node, "Attempting to dereference an array of size 0");

	ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, meta);
	type->setHandler(addr.ptrval, type->size, &valueVar);
}
void ptrs_assign_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	node = node->arg.astval;
	ptrs_jit_var_t base = node->vtable->get(node, func, scope);

	ptrs_jit_typeCheck(node, func, scope, base, PTRS_TYPE_POINTER, "Cannot dereference value of type %t");

	if(jit_value_is_constant(base.meta))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, meta);

		if(meta.array.size <= 0)
			ptrs_error(node, "Attempting to dereference an array of size 0");

		jit_value_t value;
		if(type->varType == (uint8_t)-1)
		{
			jit_insn_store_relative(func, base.val, 0, val.val);
			jit_insn_store_relative(func, base.val, sizeof(ptrs_val_t), val.meta);
		}
		else if(type->varType == PTRS_TYPE_FLOAT)
		{
			value = ptrs_jit_vartof(func, val);
			value = jit_insn_convert(func, value, type->jitType, 0);
			jit_insn_store_relative(func, base.val, 0, value);
		}
		else
		{
			value = ptrs_jit_vartoi(func, val);
			value = jit_insn_convert(func, value, type->jitType, 0);
			jit_insn_store_relative(func, base.val, 0, value);
		}
	}
	else
	{
		jit_value_t result;
		ptrs_jit_reusableCallVoid(func, ptrs_intrinsic_assign_prefix_dereference,
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_long, jit_type_ulong),
			(jit_const_int(func, void_ptr, (uintptr_t)node), base.val, base.meta, val.val, val.meta)
		);
	}
}

ptrs_jit_var_t ptrs_handle_indexlength(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t ret;
	ret.val = scope->indexSize;
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	ret.constType = PTRS_TYPE_INT;

	return ret;
}

ptrs_var_t ptrs_intrinsic_index(ptrs_ast_t *node, ptrs_val_t base, ptrs_meta_t baseMeta, ptrs_val_t index, ptrs_meta_t indexMeta)
{
	ptrs_var_t result;

	if(baseMeta.type == PTRS_TYPE_POINTER)
	{
		if(indexMeta.type != PTRS_TYPE_INT)
			ptrs_error(node, "Array index needs to be of type int not %t", indexMeta.type);
		if(index.intval < 0 || index.intval >= baseMeta.array.size)
			ptrs_error(node, "Cannot get index %d of array of length %d", index.intval, baseMeta.array.size);

		ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, baseMeta);
		type->getHandler((uint8_t *)base.ptrval + index.intval * type->size, type->size, &result);
	}
	else if(baseMeta.type == PTRS_TYPE_STRUCT)
	{
		char buff[32];
		ptrs_var_t key = ptrs_vartoa(index, indexMeta, buff, 32);
		result = ptrs_struct_get(node, base.ptrval, baseMeta, key.value.ptrval, key.meta.array.size);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", baseMeta.type);
	}

	return result;
}
ptrs_jit_var_t ptrs_handle_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t base = expr->left->vtable->get(expr->left, func, scope);

	jit_value_t oldArraySize = scope->indexSize;
	scope->indexSize = ptrs_jit_getArraySize(func, base.meta);
	ptrs_jit_var_t index = expr->right->vtable->get(expr->right, func, scope);
	scope->indexSize = oldArraySize;

	if(base.constType == PTRS_TYPE_POINTER && jit_value_is_constant(base.meta))
	{
		ptrs_jit_typeCheck(node, func, scope, index, PTRS_TYPE_INT, "Array index needs to be of type int not %t");
		ptrs_meta_t baseMeta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_nativetype_info_t *arrayType = ptrs_getNativeTypeForArray(node, baseMeta);

		jit_value_t arraySize = jit_const_long(func, ulong, baseMeta.array.size);
		struct ptrs_assertion *sizeCheck = ptrs_jit_assert(node, func, scope,
			jit_insn_lt(func, index.val, arraySize),
			2, "Attempting to access index %d of an array of size %d", index.val, arraySize);
		ptrs_jit_appendAssert(func, sizeCheck, jit_insn_ge(func, index.val, jit_const_long(func, ulong, 0)));

		ptrs_jit_var_t result;
		result.addressable = false;

		if(arrayType->varType == (uint8_t)-1)
		{
			jit_value_t varIndex = jit_insn_shl(func, index.val, jit_const_long(func, ulong, 1));
			result.val = jit_insn_load_elem(func, base.val, varIndex, jit_type_long);
			varIndex = jit_insn_add(func, varIndex, jit_const_long(func, ulong, 1));
			result.meta = jit_insn_load_elem(func, base.val, varIndex, jit_type_ulong);
			result.constType = -1;
		}
		else if(arrayType->varType == PTRS_TYPE_FLOAT)
		{
			jit_value_t loadedValue = jit_insn_load_elem(func, base.val, index.val, arrayType->jitType);
			result.val = jit_insn_convert(func, loadedValue, jit_type_float64, 0);
			result.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
			result.constType = PTRS_TYPE_FLOAT;
		}
		else
		{
			jit_value_t loadedValue = jit_insn_load_elem(func, base.val, index.val, arrayType->jitType);
			result.val = jit_insn_convert(func, loadedValue, jit_type_long, 0);
			result.meta = ptrs_jit_const_meta(func, arrayType->varType);
			result.constType = arrayType->varType;
		}

		return result;
	}
	else if(base.constType == PTRS_TYPE_STRUCT)
	{
		return ptrs_jit_struct_get(node, func, scope, base, index.val, index.meta);
	}
	else if(base.constType == -1 || base.constType == PTRS_TYPE_POINTER)
	{
		jit_value_t ret;
		ptrs_jit_reusableCall(func, ptrs_intrinsic_index, ret, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_long, jit_type_ulong),
			(jit_const_int(func, void_ptr, (uintptr_t)node), base.val, base.meta, index.val, index.meta)
		);

		return ptrs_jit_valToVar(func, ret);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", base.constType);

		// dummy
		ptrs_jit_var_t result;
		return result;
	}
}

void ptrs_intrinsic_assign_index(ptrs_ast_t *node, ptrs_val_t base, ptrs_meta_t baseMeta, ptrs_val_t index, ptrs_meta_t indexMeta, ptrs_val_t val, ptrs_meta_t valMeta)
{
	if(baseMeta.type == PTRS_TYPE_POINTER)
	{
		if(indexMeta.type != PTRS_TYPE_INT)
			ptrs_error(node, "Array index needs to be of type int not %t", indexMeta.type);
		if(index.intval < 0 || index.intval >= baseMeta.array.size)
			ptrs_error(node, "Cannot get index %d of array of length %d", index.intval, baseMeta.array.size);

		ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, baseMeta);
		if(type->varType != (uint8_t)-1 && type->varType != valMeta.type)
			ptrs_error(node, "Cannot assign an array element to value of type %t", valMeta.type);

		ptrs_var_t value = {
			.value = val,
			.meta = valMeta,
		};
		type->setHandler((uint8_t *)base.ptrval + index.intval * type->size, type->size, &value);
	}
	else if(baseMeta.type == PTRS_TYPE_STRUCT)
	{
		char buff[32];
		ptrs_var_t key = ptrs_vartoa(index, indexMeta, buff, 32);
		ptrs_struct_set(node, base.ptrval, baseMeta, key.value.ptrval, key.meta.array.size, val, valMeta);
	}
	else
	{
		ptrs_error(node, "Cannot assign index of value of type %t", baseMeta.type);
	}
}
void ptrs_assign_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t base = expr->left->vtable->get(expr->left, func, scope);

	jit_value_t oldArraySize = scope->indexSize;
	jit_value_t baseArraySize = ptrs_jit_getArraySize(func, base.meta);
	scope->indexSize = baseArraySize;
	ptrs_jit_var_t index = expr->right->vtable->get(expr->right, func, scope);
	scope->indexSize = oldArraySize;

	if(base.constType == PTRS_TYPE_POINTER && jit_value_is_constant(base.meta))
	{
		ptrs_jit_typeCheck(node, func, scope, index, PTRS_TYPE_INT, "Array index needs to be of type int not %t");
		ptrs_meta_t baseMeta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_nativetype_info_t *arrayType = ptrs_getNativeTypeForArray(node, baseMeta);

		struct ptrs_assertion *sizeCheck = ptrs_jit_assert(node, func, scope,
			jit_insn_lt(func, index.val, baseArraySize),
			2, "Attempting to access index %d of an array of size %d", index.val, baseArraySize);
		ptrs_jit_appendAssert(func, sizeCheck, jit_insn_ge(func, index.val, jit_const_long(func, ulong, 0)));

		if(arrayType->varType == (uint8_t)-1)
		{
			jit_value_t indexPos = jit_insn_shl(func, index.val, jit_const_long(func, ulong, 1));
			jit_insn_store_elem(func, base.val, indexPos, val.val);
			indexPos = jit_insn_add(func, indexPos, jit_const_long(func, ulong, 1));
			jit_insn_store_elem(func, base.val, indexPos, val.meta);
		}
		else if(arrayType->varType == PTRS_TYPE_FLOAT)
		{
			jit_value_t value = ptrs_jit_vartof(func, val);
			value = jit_insn_convert(func, value, arrayType->jitType, 0);
			jit_insn_store_elem(func, base.val, index.val, value);
		}
		else
		{
			jit_value_t value = ptrs_jit_vartoi(func, val);
			value = jit_insn_convert(func, value, arrayType->jitType, 0);
			jit_insn_store_elem(func, base.val, index.val, value);
		}
	}
	else if(base.constType == PTRS_TYPE_STRUCT)
	{
		ptrs_jit_struct_set(node, func, scope, base, index.val, index.meta, val);
	}
	else if(base.constType == -1 || base.constType == PTRS_TYPE_POINTER)
	{
		jit_value_t ret;
		ptrs_jit_reusableCallVoid(func, ptrs_intrinsic_assign_index,
			(
				jit_type_void_ptr,
				jit_type_long, jit_type_ulong,
				jit_type_long, jit_type_ulong,
				jit_type_long, jit_type_ulong
			),
			(
				jit_const_int(func, void_ptr, (uintptr_t)node),
				base.val, base.meta,
				index.val, index.meta,
				val.val, val.meta
			)
		);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", base.constType);
	}
}

ptrs_var_t ptrs_intrinsic_addressof_index(ptrs_ast_t *node, ptrs_val_t base, ptrs_meta_t baseMeta, ptrs_val_t index, ptrs_meta_t indexMeta)
{
	ptrs_var_t result;

	if(baseMeta.type == PTRS_TYPE_POINTER)
	{
		if(indexMeta.type != PTRS_TYPE_INT)
			ptrs_error(node, "Array index needs to be of type int not %t", indexMeta.type);
		if(index.intval < 0 || index.intval >= baseMeta.array.size)
			ptrs_error(node, "Cannot get index %d of array of length %d", index.intval, baseMeta.array.size);

		ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, baseMeta);
		result.value.ptrval = (uint8_t *)base.ptrval + index.intval * type->size;
		result.meta.type = PTRS_TYPE_POINTER;
		result.meta.array.typeIndex = baseMeta.array.typeIndex;
		result.meta.array.size = baseMeta.array.size - index.intval;
	}
	else if(baseMeta.type == PTRS_TYPE_STRUCT)
	{
		char buff[32];
		ptrs_var_t key = ptrs_vartoa(index, indexMeta, buff, 32);
		result = ptrs_struct_addressOf(node, base.ptrval, baseMeta, key.value.ptrval, key.meta.array.size);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", baseMeta.type);
	}

	return result;
}
ptrs_jit_var_t ptrs_addressof_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t base = expr->left->vtable->get(expr->left, func, scope);

	jit_value_t oldArraySize = scope->indexSize;
	jit_value_t baseArraySize = ptrs_jit_getArraySize(func, base.meta);
	scope->indexSize = baseArraySize;
	ptrs_jit_var_t index = expr->right->vtable->get(expr->right, func, scope);
	scope->indexSize = oldArraySize;

	if(base.constType == PTRS_TYPE_POINTER)
	{
		ptrs_jit_typeCheck(node, func, scope, index, PTRS_TYPE_INT, "Array index needs to be of type int not %t");

		struct ptrs_assertion *sizeCheck = ptrs_jit_assert(node, func, scope,
			jit_insn_lt(func, index.val, baseArraySize),
			2, "Attempting to access index %d of an array of size %d", index.val, baseArraySize);
		ptrs_jit_appendAssert(func, sizeCheck, jit_insn_ge(func, index.val, jit_const_long(func, ulong, 0)));

		jit_value_t newLen = jit_insn_sub(func, baseArraySize, index.val);

		jit_value_t typeIndex = ptrs_jit_getArrayTypeIndex(func, base.meta);
		jit_value_t typeSize = ptrs_jit_getArrayTypeSize(node, func, base.meta, typeIndex);

		ptrs_jit_var_t result;
		result.val = jit_insn_add(func, base.val, jit_insn_mul(func, index.val, typeSize));
		result.meta = ptrs_jit_arrayMeta(func, newLen, typeIndex);
		result.constType = PTRS_TYPE_POINTER;
		return result;
	}
	else if(base.constType == PTRS_TYPE_STRUCT)
	{
		return ptrs_jit_struct_addressof(node, func, scope, base, index.val, index.meta);
	}
	else if(base.constType == -1)
	{
		jit_value_t ret;
		ptrs_jit_reusableCall(func, ptrs_intrinsic_addressof_index, ret, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_long, jit_type_ulong),
			(jit_const_int(func, void_ptr, (uintptr_t)node), base.val, base.meta, index.val, index.meta)
		);

		return ptrs_jit_valToVar(func, ret);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", base.constType);

		// dummy
		ptrs_jit_var_t result;
		return result;
	}
}

ptrs_jit_var_t ptrs_call_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments)
{
	ptrs_jit_var_t callee;

	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t base = expr->left->vtable->get(expr->left, func, scope);

	jit_value_t oldArraySize = scope->indexSize;
	jit_value_t baseArraySize = ptrs_jit_getArraySize(func, base.meta);
	scope->indexSize = baseArraySize;
	ptrs_jit_var_t index = expr->right->vtable->get(expr->right, func, scope);
	scope->indexSize = oldArraySize;

	if(base.constType == PTRS_TYPE_POINTER && jit_value_is_constant(base.meta))
	{
		ptrs_jit_typeCheck(node, func, scope, index, PTRS_TYPE_INT, "Array index needs to be of type int not %t");
		ptrs_meta_t baseMeta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_nativetype_info_t *arrayType = ptrs_getNativeTypeForArray(node, baseMeta);

		if(arrayType->varType != PTRS_TYPE_STRUCT
			&& arrayType->varType != PTRS_TYPE_POINTER
			&& arrayType->varType != PTRS_TYPE_FUNCTION
			&& arrayType->varType != (uint8_t)-1)
			ptrs_error(node, "Cannot call value of type %t", arrayType->varType);

		struct ptrs_assertion *sizeCheck = ptrs_jit_assert(node, func, scope,
			jit_insn_lt(func, index.val, baseArraySize),
			2, "Attempting to access index %d of an array of size %d", index.val, baseArraySize);
		ptrs_jit_appendAssert(func, sizeCheck, jit_insn_ge(func, index.val, jit_const_long(func, ulong, 0)));

		if(arrayType->varType == (uint8_t)-1)
		{
			jit_value_t varIndex = jit_insn_shl(func, index.val, jit_const_long(func, ulong, 1));
			callee.val = jit_insn_load_elem(func, base.val, varIndex, jit_type_long);
			varIndex = jit_insn_add(func, varIndex, jit_const_long(func, ulong, 1));
			callee.meta = jit_insn_load_elem(func, base.val, varIndex, jit_type_ulong);
			callee.constType = -1;
			callee.addressable = false;
		}
		else
		{
			jit_value_t loadedValue = jit_insn_load_elem(func, base.val, index.val, arrayType->jitType);
			callee.val = jit_insn_convert(func, loadedValue, jit_type_long, 0);
			callee.meta = ptrs_jit_const_meta(func, arrayType->varType);
			callee.constType = arrayType->varType;
			callee.addressable = false;
		}
	}
	else if(base.constType == PTRS_TYPE_STRUCT)
	{
		return ptrs_jit_struct_call(node, func, scope, base, index.val, index.meta, typing, arguments);
	}
	else if(base.constType == -1 || base.constType == PTRS_TYPE_POINTER)
	{
		jit_value_t ret;
		ptrs_jit_reusableCall(func, ptrs_intrinsic_index, ret, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_long, jit_type_ulong),
			(jit_const_int(func, void_ptr, (uintptr_t)node), base.val, base.meta, index.val, index.meta)
		);

		callee = ptrs_jit_valToVar(func, ret);
	}
	else
	{
		ptrs_error(node, "Cannot get index of value of type %t", base.constType);

		// dummy
		ptrs_jit_var_t result;
		return result;
	}

	return ptrs_jit_call(node, func, scope, typing, base.val, callee, arguments);
}

ptrs_jit_var_t ptrs_handle_slice(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_slice *expr = &node->arg.slice;

	ptrs_jit_var_t val = expr->base->vtable->get(expr->base, func, scope);

	jit_value_t oldSize = scope->indexSize;
	scope->indexSize = ptrs_jit_getArraySize(func, val.meta);
	ptrs_jit_var_t start = expr->start->vtable->get(expr->start, func, scope);
	ptrs_jit_var_t end = expr->end->vtable->get(expr->end, func, scope);

	ptrs_jit_typeCheck(node, func, scope, val, PTRS_TYPE_POINTER, "Cannot slice a value of type %t");
	ptrs_jit_typeCheck(node, func, scope, start, PTRS_TYPE_INT, "Slice start value needs to be of type int not %t");
	ptrs_jit_typeCheck(node, func, scope, end, PTRS_TYPE_INT, "Slice end value needs to be of type int not %t");

	ptrs_jit_assert(node, func, scope, jit_insn_le(func, end.val, scope->indexSize),
		2, "Cannot end a slice at %d for an array of size %d", end, scope->indexSize);
	ptrs_jit_assert(node, func, scope, jit_insn_ge(func, start.val, jit_const_long(func, ulong, 0)),
		1, "Cannot start a slice at %d", start);

	ptrs_jit_assert(node, func, scope, jit_insn_le(func, start.val, end.val),
		2, "Slice start (%d) is after slice end (%d)", start, end);

	scope->indexSize = oldSize;

	jit_value_t typeSize = ptrs_jit_getArrayTypeSize(node, func, val.meta, NULL);
	jit_value_t newPtr = jit_insn_add(func, val.val, jit_insn_mul(func, start.val, typeSize));
	jit_value_t newSize = jit_insn_sub(func, end.val, start.val);

	ptrs_jit_var_t ret;
	ret.val = newPtr;
	ret.meta = ptrs_jit_setArraySize(func, val.meta, newSize);
	ret.constType = PTRS_TYPE_POINTER;
	return ret;
}

ptrs_jit_var_t ptrs_handle_as(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;

	ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);

	if(expr->builtinType != PTRS_TYPE_FLOAT)
		val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);

	val.meta = ptrs_jit_const_meta(func, expr->builtinType);
	val.constType = expr->builtinType;
	return val;
}

ptrs_jit_var_t ptrs_handle_cast_builtin(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;
	ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);

	switch(expr->builtinType)
	{
		case PTRS_TYPE_INT:
			val.val = ptrs_jit_vartoi(func, val);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
			val.constType = PTRS_TYPE_INT;
			break;
		case PTRS_TYPE_FLOAT:
			val.val = ptrs_jit_vartof(func, val);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
			val.constType = PTRS_TYPE_FLOAT;
			break;
		default:
			ptrs_error(node, "Cannot convert to type %s", ptrs_typetoa(expr->builtinType));
	}

	return val;
}

ptrs_jit_var_t ptrs_handle_tostring(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;
	ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);

	return ptrs_jit_vartoa(func, val);
}

ptrs_jit_var_t ptrs_handle_cast(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;

	ptrs_jit_var_t type = expr->type->vtable->get(expr->type, func, scope);
	ptrs_jit_typeCheck(node, func, scope, type, PTRS_TYPE_STRUCT,
		"Cast target is of type %t not struct");

	ptrs_jit_var_t val = expr->value->vtable->get(expr->value, func, scope);

	val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
	val.meta = type.meta;
	val.constType = PTRS_TYPE_STRUCT;

	return val;
}

ptrs_jit_var_t ptrs_handle_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;
	struct ptrs_ast_import *stmt = &expr->import->arg.import;

	if(stmt->isScriptImport)
	{
		ptrs_ast_t *ast = stmt->expressions[expr->index];
		return ast->vtable->get(ast, func, scope);
	}
	else if(expr->type == NULL)
	{
		return ptrs_jit_varFromConstant(func, stmt->symbols[expr->index]);
	}
	else
	{
		ptrs_jit_var_t ret;
		jit_value_t addr = jit_const_int(func, void_ptr,
			(uintptr_t)stmt->symbols[expr->index].value.ptrval);

		ret.val = jit_insn_load_relative(func, addr, 0, expr->type->jitType);
		ret.meta = ptrs_jit_const_meta(func, expr->type->varType);
		ret.constType = expr->type->varType;

		return ret;
	}
}
void ptrs_assign_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t val)
{
	struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;
	struct ptrs_ast_import *stmt = &expr->import->arg.import;

	if(stmt->isScriptImport)
	{
		ptrs_ast_t *ast = stmt->expressions[expr->index];
		if(ast->vtable->set == NULL)
			ptrs_error(node, "Invalid assign expression, left side is not a valid lvalue");

		ast->vtable->set(ast, func, scope, val);
		return;
	}
	else
	{
		if(expr->type == NULL)
		{
			ptrs_error(node, "Cannot re-assign an imported function");
		}

		jit_value_t addr = jit_const_int(func, void_ptr,
			(uintptr_t)stmt->symbols[expr->index].value.ptrval);
		ptrs_jit_assignTypedFromVar(func, addr, expr->type->jitType, val);
	}
}
ptrs_jit_var_t ptrs_call_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments)
{
	struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;
	struct ptrs_ast_import *stmt = &expr->import->arg.import;
	ptrs_jit_var_t val;

	if(stmt->isScriptImport)
	{
		ptrs_ast_t *ast = stmt->expressions[expr->index];
		if(ast->vtable->call != NULL)
			return ast->vtable->call(ast, func, scope, caller, typing, arguments);
		else
			val = ast->vtable->get(ast, func, scope);
	}
	else if(expr->type == NULL)
	{
		val = ptrs_jit_varFromConstant(func, stmt->symbols[expr->index]);
	}
	else
	{
		ptrs_jit_var_t ret;
		jit_value_t addr = jit_const_int(func, void_ptr,
			(uintptr_t)stmt->symbols[expr->index].value.ptrval);

		val.val = jit_insn_load_relative(func, addr, 0, expr->type->jitType);
		val.meta = ptrs_jit_const_meta(func, expr->type->varType);
		val.constType = expr->type->varType;
	}

	return ptrs_jit_call(node, func, scope,
		typing, jit_const_int(func, void_ptr, 0), val, arguments);
}
ptrs_jit_var_t ptrs_addressof_importedsymbol(ptrs_ast_t *node,
	jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;
	struct ptrs_ast_import *stmt = &expr->import->arg.import;

	if(stmt->isScriptImport)
	{
		ptrs_ast_t *ast = stmt->expressions[expr->index];
		if(ast->vtable->addressof == NULL)
			ptrs_error(node, "Cannot get address of temporary or constant value");

		return ast->vtable->addressof(ast, func, scope);
	}
	else if(expr->type == NULL)
	{
		ptrs_error(node, "Cannot get address of imported native function");
	}
	else
	{
		ptrs_jit_var_t ret;
		ret.val = jit_const_int(func, void_ptr,
			(uintptr_t)stmt->symbols[expr->index].value.ptrval);
		size_t size = expr->type->size;
		ret.meta = ptrs_jit_const_arrayMeta(func, size, expr->type - ptrs_nativeTypes);
		ret.constType = PTRS_TYPE_POINTER;

		return ret;
	}
}

ptrs_jit_var_t ptrs_handle_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_identifier *expr = &node->arg.identifier;

	ptrs_jit_var_t target = *expr->location;
	ptrs_jit_var_t ret;
	ret.addressable = 0;

	if(expr->valuePredicted)
		ret.val = jit_const_long(func, long, expr->valuePrediction.intval);

	if(expr->metaPredicted)
		ret.meta = jit_const_long(func, ulong, *(jit_long *)&expr->metaPrediction);

	if(expr->typePredicted)
		ret.constType = expr->metaPrediction.type;
	else
		ret.constType = -1;

	if(expr->valuePredicted && expr->metaPredicted && expr->typePredicted)
		return ret;

	if(target.addressable
		&& (!jit_value_is_constant(target.val) || !jit_value_is_constant(target.meta)))
	{
		jit_value_t ptr = ptrs_jit_import(node, func, target.val, true);
		if(!expr->valuePredicted)
			ret.val = jit_insn_load_relative(func, ptr, 0, jit_type_long);
		if(!expr->metaPredicted)
			ret.meta = jit_insn_load_relative(func, ptr, sizeof(ptrs_val_t), jit_type_ulong);
	}
	else
	{
		if(!expr->valuePredicted)
			ret.val = ptrs_jit_import(node, func, target.val, false);

		if(!expr->metaPredicted)
			ret.meta = ptrs_jit_import(node, func, target.meta, false);
	}

	return ret;
}
void ptrs_assign_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	ptrs_jit_var_t target = *node->arg.identifier.location;
	jit_function_t targetFunc = jit_value_get_function(target.val);

	if(target.addressable)
	{
		jit_value_t ptr = ptrs_jit_import(node, func, target.val, true);
		jit_insn_store_relative(func, ptr, 0, val.val);
		jit_insn_store_relative(func, ptr, sizeof(ptrs_val_t), val.meta);
		return;
	}

	if(jit_value_is_constant(target.val))
	{
		ptrs_error(node, "Cannot assign a value to a constant");
	}

	if(func == targetFunc)
	{
		if(target.constType != PTRS_TYPE_FLOAT)
			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
		jit_insn_store(func, target.val, val.val);
	}
	else
	{
		target.val = jit_insn_import(func, target.val);
		if(target.val == NULL)
			ptrs_error(node, "Cannot access that variable from here");

		jit_insn_store_relative(func, target.val, 0, val.val);
	}

	if(target.constType != -1)
	{
		if(val.constType == -1)
		{
			jit_value_t type = ptrs_jit_getType(func, val.meta);
			ptrs_jit_assert(node, func, scope,
				jit_insn_eq(func, type, jit_const_long(func, ulong, target.constType)),
				2, "Cannot assign value of type %m to a variable of type %t",
				val.meta, jit_const_int(func, sbyte, target.constType)
			);
		}
		else if(val.constType != target.constType)
		{
			ptrs_error(node, "Cannot assign value of type %t to variable of type %t",
				val.constType, target.constType);
		}
	}

	if(jit_value_is_constant(target.meta))
	{
		if(target.constType == PTRS_TYPE_INT || target.constType == PTRS_TYPE_FLOAT)
		{
			//ignore
		}
		else if(jit_value_is_constant(val.meta))
		{
			if(jit_value_get_long_constant(target.meta) != jit_value_get_long_constant(val.meta))
			{
				ptrs_error(node, "The right side's meta value does not match the defined meta"
					" of the variable. Are you trying to assign a different struct"
					" to a struct variable?");
			}
		}
		else
		{
			target.meta = ptrs_jit_import(node, func, target.meta, false);

			ptrs_jit_assert(node, func, scope, jit_insn_eq(func, target.meta, val.meta),
				0, "The right side's meta value does not match the defined meta"
				" of the variable. Are you trying to assign a different struct"
				" to a struct variable?");
		}
	}
	else if(func == targetFunc)
	{
		jit_insn_store(func, target.meta, val.meta);
	}
	else
	{
		target.meta = jit_insn_import(func, target.meta);
		jit_insn_store_relative(func, target.meta, 0, val.meta);
	}
}
ptrs_jit_var_t ptrs_addressof_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t target = *node->arg.identifier.location;
	if(!target.addressable)
		ptrs_error(node, "Variable is not marked as addressable. This is probably an internal error");

	target.val = ptrs_jit_import(node, func, target.val, true);
	target.meta = ptrs_jit_const_arrayMeta(func, 1, PTRS_NATIVETYPE_INDEX_VAR);
	target.constType = PTRS_TYPE_POINTER;

	return target;
}

ptrs_jit_var_t ptrs_handle_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	jit_function_t target = node->arg.funcval->symbol;

	ptrs_jit_var_t ret;
	ret.val = jit_const_long(func, long, (uintptr_t)ptrs_jit_function_to_closure(node, target));
	ret.meta = ptrs_jit_pointerMeta(func,
		jit_const_long(func, ulong, PTRS_TYPE_FUNCTION),
		jit_insn_get_parent_frame_pointer_of(func, target)
	);
	ret.constType = PTRS_TYPE_FUNCTION;

	return ret;
}
ptrs_jit_var_t ptrs_call_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments)
{
	return ptrs_jit_callnested(node, func, scope,
		jit_const_int(func, void_ptr, 0), node->arg.funcval->symbol, arguments);
}

ptrs_jit_var_t ptrs_handle_constant(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	return ptrs_jit_varFromConstant(func, node->arg.constval);
}

ptrs_jit_var_t ptrs_handle_prefix_typeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t val = node->vtable->get(node, func, scope);

	if(val.constType == -1)
	{
		val.val = ptrs_jit_getType(func, val.meta);
		val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
		val.constType = PTRS_TYPE_INT;
	}
	else
	{
		val.val = jit_const_long(func, long, val.constType);
		val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
		val.constType = PTRS_TYPE_INT;
	}
	return val;
}

ptrs_jit_var_t ptrs_handle_op_instanceof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;
	ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope);
	ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope);

	if(right.constType != PTRS_TYPE_STRUCT)
	{
		ptrs_jit_typeCheck(node, func, scope, right, PTRS_TYPE_STRUCT,
			"A variable of type %t cannot be a constructor, it is not a struct");
	}

	left.val = jit_insn_eq(func, left.meta, right.meta);
	left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	left.constType = PTRS_TYPE_INT;
	return left;
}

ptrs_jit_var_t ptrs_handle_op_in(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;
	ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope);
	ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope);

	if(right.constType != PTRS_TYPE_STRUCT)
	{
		ptrs_jit_typeCheck(node, func, scope, right, PTRS_TYPE_STRUCT,
			"Cannot check if a value of type %t has a field, it is not a struct");
	}

	ptrs_jit_var_t ret = {
		.val = NULL,
		.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT),
		.constType = PTRS_TYPE_INT
	};

	if(jit_value_is_constant(left.val) && jit_value_is_constant(left.meta) && jit_value_is_constant(right.meta))
	{
		ptrs_val_t nameVal = ptrs_jit_value_getValConstant(left.val);
		ptrs_meta_t nameMeta = ptrs_jit_value_getMetaConstant(left.meta);
		ptrs_meta_t strucMeta = ptrs_jit_value_getMetaConstant(right.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(strucMeta);

		char buff[32];
		const char *name = ptrs_vartoa(nameVal, nameMeta, buff, 32).value.ptrval;
		int32_t nameLen = strlen(name);

		if(ptrs_struct_find(struc, name, nameLen, -1, node) != NULL)
		{
			ret.val = jit_const_long(func, long, true);
			return ret;
		}
		else if(ptrs_struct_getOverload(struc, ptrs_handle_op_in, true) == NULL)
		{
			ret.val = jit_const_long(func, long, false);
			return ret;
		}
	}

	ptrs_jit_var_t name = ptrs_jit_vartoa(func, left);
	jit_value_t struc = ptrs_jit_getMetaPointer(func, right.meta);
	jit_value_t nodeVal = jit_const_int(func, void_ptr, (uintptr_t)node);

	ptrs_jit_reusableCall(func, ptrs_struct_hasKey, ret.val, jit_type_sbyte,
		(jit_type_void_ptr, jit_type_void_ptr, jit_type_void_ptr, jit_type_ulong, jit_type_void_ptr),
		(right.val, struc, name.val, name.meta, nodeVal)
	);

	ret.val = jit_insn_convert(func, ret.val, jit_type_long, 0);
	return ret;
}
