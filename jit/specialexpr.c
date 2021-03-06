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
		.meta = ptrs_jit_arrayMeta(func,
			jit_const_long(func, ulong, PTRS_TYPE_NATIVE),
			jit_const_long(func, ulong, 0),
			len),
		.constType = PTRS_TYPE_NATIVE,
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
		(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER, PTRS_TYPE_STRUCT),
		case PTRS_TYPE_NATIVE:
		case PTRS_TYPE_POINTER:
			jit_insn_store(func, ret.val, ptrs_jit_getArraySize(func, val.meta));
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

ptrs_jit_var_t ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t val = node->vtable->get(node, func, scope);

	ptrs_jit_var_t ret = {
		jit_value_create(func, jit_type_long),
		jit_value_create(func, jit_type_ulong),
		-1,
	};

	ptrs_jit_typeSwitch(node, func, scope, val,
		(1, "Cannot dereference variable of type %t", TYPESWITCH_TYPE),
		(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER),
		case PTRS_TYPE_POINTER:
			jit_insn_store(func, ret.val,
				jit_insn_load_relative(func, val.val, 0, jit_type_long));
			jit_insn_store(func, ret.meta,
				jit_insn_load_relative(func, val.val, sizeof(ptrs_val_t), jit_type_ulong));
			break;

		case PTRS_TYPE_NATIVE:
			jit_insn_store(func, ret.val, jit_insn_load_relative(func, val.val, 0, jit_type_ubyte));
			jit_insn_store(func, ret.meta, ptrs_jit_const_meta(func, PTRS_TYPE_INT));
			ret.constType = PTRS_TYPE_INT;
			break;
	);

	return ret;
}
void ptrs_assign_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	node = node->arg.astval;
	ptrs_jit_var_t base = node->vtable->get(node, func, scope);

	ptrs_jit_typeSwitch(node, func, scope, base,
		(1, "Cannot dereference variable of type %t", TYPESWITCH_TYPE),
		(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER),

		case PTRS_TYPE_NATIVE:
			;
			jit_value_t byteVal = jit_insn_convert(func, ptrs_jit_vartoi(func, val), jit_type_ubyte, 0);
			jit_insn_store_relative(func, base.val, 0, byteVal);
			break;

		case PTRS_TYPE_POINTER:
			jit_insn_store_relative(func, base.val, 0, val.val);
			jit_insn_store_relative(func, base.val, sizeof(ptrs_val_t), val.meta);
			break;
	);
}

ptrs_jit_var_t ptrs_handle_indexlength(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t ret;
	ret.val = scope->indexSize;
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	ret.constType = PTRS_TYPE_INT;

	return ret;
}

#define ptrs_handle_index_common(structOp, arraySetup, nativeOp, pointerOp) \
	do { \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		ptrs_jit_var_t base = expr->left->vtable->get(expr->left, func, scope); \
		\
		jit_value_t oldSize = scope->indexSize; \
		jit_value_t arraySize = ptrs_jit_getArraySize(func, base.meta); \
		scope->indexSize = arraySize; \
		ptrs_jit_var_t index = expr->right->vtable->get(expr->right, func, scope); \
		scope->indexSize = oldSize; \
		\
		jit_label_t isArray = jit_label_undefined; \
		jit_label_t done = jit_label_undefined; \
		if(base.constType == -1) \
		{ \
			jit_value_t type = ptrs_jit_getType(func, base.meta); \
			\
			jit_insn_branch_if(func, \
				jit_insn_ne(func, type, jit_const_int(func, sbyte, PTRS_TYPE_STRUCT)), \
				&isArray \
			); \
		} \
		\
		if(base.constType == -1 || base.constType == PTRS_TYPE_STRUCT) \
		{ \
			ptrs_jit_var_t strIndex; \
			if(index.constType != PTRS_TYPE_NATIVE) \
				strIndex = ptrs_jit_vartoa(func, index); \
			else \
				strIndex = index; \
			\
			structOp \
			jit_insn_branch(func, &done); \
		} \
		\
		if(base.constType == -1 \
			|| base.constType == PTRS_TYPE_NATIVE \
			|| base.constType == PTRS_TYPE_POINTER) \
		{ \
			jit_insn_label(func, &isArray); \
			jit_value_t intIndex = ptrs_jit_vartoi(func, index); \
			\
			struct ptrs_assertion *assert = ptrs_jit_assert(node, func, scope, \
				jit_insn_ge(func, intIndex, jit_const_int(func, nuint, 0)), \
				3, "Cannot get index %v of array of size %d", index.val, index.meta, arraySize); \
			\
			ptrs_jit_appendAssert(func, assert, \
				jit_insn_le(func, intIndex, arraySize)); \
			\
			arraySetup \
			\
			ptrs_jit_typeSwitch(node, func, scope, base, \
				(1, "Cannot dereference variable of type %t", TYPESWITCH_TYPE), \
				(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER), \
				\
				case PTRS_TYPE_NATIVE: \
					; \
					nativeOp \
					break; \
					\
				case PTRS_TYPE_POINTER: \
					; \
					pointerOp \
					break; \
			); \
		} \
		\
		jit_insn_label(func, &done); \
	} while(0)

ptrs_jit_var_t ptrs_handle_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t result = {
		.val = jit_value_create(func, jit_type_long),
		.meta = jit_value_create(func, jit_type_ulong),
		.constType = -1,
	};

	ptrs_handle_index_common(
		{
			jit_value_t keyLen = ptrs_jit_getArraySize(func, strIndex.meta);
			ptrs_jit_var_t _result = ptrs_jit_struct_get(node, func, scope,
				base, strIndex.val, keyLen);
			jit_insn_store(func, result.val, _result.val);
			jit_insn_store(func, result.meta, _result.meta);

			if(base.constType == PTRS_TYPE_STRUCT)
				result.constType = _result.constType;
		},
		/* nothing */,
		{
			jit_insn_store(func, result.val, jit_insn_load_elem(func, base.val, intIndex, jit_type_ubyte));
			jit_insn_store(func, result.meta, ptrs_jit_const_meta(func, PTRS_TYPE_INT));

			if(base.constType == PTRS_TYPE_NATIVE)
				result.constType = PTRS_TYPE_INT;
		},
		{
			intIndex = jit_insn_shl(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store(func, result.val, jit_insn_load_elem(func, base.val, intIndex, jit_type_long));
			intIndex = jit_insn_add(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store(func, result.meta, jit_insn_load_elem(func, base.val, intIndex, jit_type_ulong));
		}
	);

	return result;
}
void ptrs_assign_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	ptrs_handle_index_common(
		{
			jit_value_t keyLen = ptrs_jit_getArraySize(func, strIndex.meta);
			ptrs_jit_struct_set(node, func, scope,
				base, strIndex.val, keyLen, val);
		},
		/* nothing */,
		{
			jit_value_t intVal = ptrs_jit_vartoi(func, val);
			jit_value_t uByteVal = jit_insn_convert(func, intVal, jit_type_ubyte, 0);
			jit_insn_store_elem(func, base.val, intIndex, uByteVal);
		},
		{
			intIndex = jit_insn_shl(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store_elem(func, base.val, intIndex, val.val);
			intIndex = jit_insn_add(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store_elem(func, base.val, intIndex, val.meta);
		}
	);
}
ptrs_jit_var_t ptrs_addressof_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t result = {
		.val = jit_value_create(func, jit_type_long),
		.meta = jit_value_create(func, jit_type_ulong),
		.constType = -1,
	};

	ptrs_handle_index_common(
		{
			jit_value_t keyLen = ptrs_jit_getArraySize(func, strIndex.meta);
			ptrs_jit_var_t _result = ptrs_jit_struct_addressof(node, func, scope,
				base, strIndex.val, keyLen);
			jit_insn_store(func, result.val, _result.val);
			jit_insn_store(func, result.meta, _result.meta);

			if(base.constType == PTRS_TYPE_STRUCT)
				result.constType = _result.constType;
		},
		{
			if(base.constType != PTRS_TYPE_STRUCT)
				result.constType = base.constType;

			jit_value_t newSize = jit_insn_sub(func, ptrs_jit_getArraySize(func, base.meta), intIndex);
			jit_insn_store(func, result.meta, ptrs_jit_setArraySize(func, base.meta, newSize));
		},
		{
			jit_insn_store(func, result.val,
				jit_insn_load_elem_address(func, base.val, intIndex, jit_type_ubyte));
		},
		{
			intIndex = jit_insn_shl(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store(func, result.val,
				jit_insn_load_elem_address(func, base.val, intIndex, jit_type_long));
		}
	);

	return result;
}
ptrs_jit_var_t ptrs_call_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments)
{
	ptrs_jit_var_t _base;
	ptrs_jit_var_t callee = {
		.val = jit_value_create(func, jit_type_long),
		.meta = jit_value_create(func, jit_type_ulong),
		.constType = -1,
	};

	ptrs_handle_index_common(
		{
			jit_value_t keyLen = ptrs_jit_getArraySize(func, strIndex.meta);
			_base = base;
			ptrs_jit_var_t _result = ptrs_jit_struct_get(node, func, scope,
				base, strIndex.val, keyLen);
			jit_insn_store(func, callee.val, _result.val);
			jit_insn_store(func, callee.meta, _result.meta);
		},
		{
			_base = base;
		},
		{
			if(base.constType == PTRS_TYPE_NATIVE)
			{
				ptrs_error(node, "Cannot call an index of a byte array");
			}
			else
			{
				jit_value_t msg = jit_const_int(func, void_ptr, (uintptr_t)"Cannot call index %d of a byte array");
				({ptrs_jit_reusableCallVoid(func, ptrs_error,
					(jit_type_void_ptr, jit_type_void_ptr, jit_type_nint),
					(jit_const_int(func, void_ptr, (uintptr_t)node), msg, intIndex)
				);});
			}
		},
		{
			intIndex = jit_insn_shl(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store(func, callee.val, jit_insn_load_elem(func, base.val, intIndex, jit_type_long));
			intIndex = jit_insn_add(func, intIndex, jit_const_int(func, nint, 1));
			jit_insn_store(func, callee.meta, jit_insn_load_elem(func, base.val, intIndex, jit_type_ulong));
		}
	);

	return ptrs_jit_call(node, func, scope, typing, _base.val, callee, arguments);
}

ptrs_jit_var_t ptrs_handle_slice(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_slice *expr = &node->arg.slice;

	ptrs_jit_var_t val = expr->base->vtable->get(expr->base, func, scope);
	jit_value_t type = ptrs_jit_getType(func, val.meta);

	jit_value_t oldSize = scope->indexSize;
	scope->indexSize = ptrs_jit_getArraySize(func, val.meta);

	jit_value_t start = ptrs_jit_vartoi(func,
		expr->start->vtable->get(expr->start, func, scope));
	jit_value_t end = ptrs_jit_vartoi(func,
		expr->end->vtable->get(expr->end, func, scope));

	jit_value_t newSize = jit_insn_sub(func, end, start);
	jit_value_t newPtr = jit_value_create(func, jit_type_void_ptr);

	ptrs_jit_typeSwitch(node, func, scope, val,
		(1, "Cannot slice variable of type %t", TYPESWITCH_TYPE),
		(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER),

		case PTRS_TYPE_NATIVE:
			jit_insn_store(func, newPtr,
				jit_insn_load_elem_address(func, val.val, start, jit_type_ubyte));
			break;

		case PTRS_TYPE_POINTER:
			jit_insn_store(func, newPtr,
				jit_insn_load_elem_address(func, val.val, start, ptrs_jit_getVarType()));
			break;
	);

	/*
	disabled so we can slice arrays received from native functions.
	TODO: should there be a seperate syntax for that?

	ptrs_jit_assert(node, func, scope, jit_insn_le(func, end, scope->indexSize),
		2, "Cannot end a slice at %d for an array of size %d", end, scope->indexSize);
	ptrs_jit_assert(node, func, scope, jit_insn_ge(func, start, jit_const_long(func, ulong, 0)),
		1, "Cannot start a slice at %d", start);
	*/

	ptrs_jit_assert(node, func, scope, jit_insn_le(func, start, end),
		2, "Slice start (%d) is after slice end (%d)", start, end);

	scope->indexSize = oldSize;

	ptrs_jit_var_t ret;
	ret.val = newPtr;
	ret.meta = ptrs_jit_setArraySize(func, val.meta, newSize);
	ret.constType = val.constType;

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
			(uintptr_t)stmt->symbols[expr->index].value.nativeval);

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
			(uintptr_t)stmt->symbols[expr->index].value.nativeval);
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
			(uintptr_t)stmt->symbols[expr->index].value.nativeval);

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
			(uintptr_t)stmt->symbols[expr->index].value.nativeval);
		ret.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, false, expr->type->size);
		ret.constType = PTRS_TYPE_NATIVE;

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
				2, "Cannot assign value of type %mt to variable of type %t",
				type, jit_const_int(func, sbyte, target.constType)
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
				2, "The right side's meta value does not match the defined meta"
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
	target.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_POINTER, false, 1);
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
		const char *name = ptrs_vartoa(nameVal, nameMeta, buff, 32).value.strval;
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
