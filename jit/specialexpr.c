#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/call.h"
#include "include/util.h"
#include "include/run.h"

ptrs_jit_var_t ptrs_handle_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_call *expr = &node->arg.call;

	if(expr->value->callHandler != NULL)
		return expr->value->callHandler(expr->value, func, scope, node, expr->arguments);

	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	ptrs_jit_var_t ret;
	ret.val = ptrs_jit_vcall(node, func, scope, jit_type_long, val.val, val.meta, expr->arguments);
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	ret.constType = PTRS_TYPE_INT;

	return ret;
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
		argDef[i] = jit_type_void_ptr;

		ptrs_jit_var_t val = curr->entry->handler(curr->entry, func, scope);

		if(curr->convert)
			val = ptrs_jit_vartoa(func, val);

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
	//TODO
}

ptrs_jit_var_t ptrs_handle_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_assign_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_addressof_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_call_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_prefix_sizeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;

	ptrs_jit_var_t val = node->handler(node, func, scope);

	if(val.constType == -1)
	{
		jit_value_t type = ptrs_jit_getType(func, val.meta);
		struct ptrs_assertion *assertion = ptrs_jit_assert(node, func, scope,
			jit_insn_ge(func, type, jit_const_int(func, int, PTRS_TYPE_NATIVE)),
			1, "Cannot get size of variable of type %t", type);
		ptrs_jit_appendAssert(func, assertion, jit_insn_le(func, type, jit_const_int(func, int, PTRS_TYPE_POINTER)));
	}
	else if(val.constType != PTRS_TYPE_NATIVE && val.constType != PTRS_TYPE_POINTER)
	{
		ptrs_error(node, "Cannot get size of variable of type %t", val.constType);
	}

	val.val = ptrs_jit_getArraySize(func, val.meta);
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	val.constType = PTRS_TYPE_INT;

	return val;
}

ptrs_jit_var_t ptrs_handle_prefix_address(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;

	if(node->addressHandler == NULL)
		ptrs_error(node, "Cannot get address of temporary or constant value");

	return node->addressHandler(node, func, scope);
}

ptrs_jit_var_t ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t val = node->handler(node, func, scope);

	ptrs_jit_var_t ret = {
		jit_value_create(func, jit_type_long),
		jit_value_create(func, jit_type_ulong),
	};

	jit_label_t isNative = jit_label_undefined;
	jit_label_t end = jit_label_undefined;

	jit_value_t type = ptrs_jit_getType(func, val.meta);
	jit_insn_branch_if(func, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_NATIVE)), &isNative);

	ptrs_jit_assert(node, func, scope, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_POINTER)),
		1, "Cannot dereference variable of type %t", type);

	//PTRS_TYPE_POINTER
	jit_insn_store(func, ret.val, jit_insn_load_relative(func, val.val, 0, jit_type_long));
	jit_insn_store(func, ret.meta, jit_insn_load_relative(func, val.val, sizeof(ptrs_val_t), jit_type_ulong));
	jit_insn_branch(func, &end);

	//PTRS_TYPE_NATIVE
	jit_insn_label(func, &isNative);
	jit_insn_store(func, ret.val, jit_insn_load_relative(func, val.val, 0, jit_type_ubyte));
	jit_insn_store(func, ret.meta, ptrs_jit_const_meta(func, PTRS_TYPE_INT));

	jit_insn_label(func, &end);

	return ret;
}
void ptrs_handle_assign_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	node = node->arg.astval;
	ptrs_jit_var_t base = node->handler(node, func, scope);

	jit_label_t isNative = jit_label_undefined;
	jit_label_t end = jit_label_undefined;

	jit_value_t type = ptrs_jit_getType(func, base.meta);
	jit_insn_branch_if(func, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_NATIVE)), &isNative);

	ptrs_jit_assert(node, func, scope, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_POINTER)),
		1, "Cannot dereference variable of type %t", type);

	//PTRS_TYPE_POINTER
	jit_insn_store_relative(func, base.val, 0, val.val);
	jit_insn_store_relative(func, base.val, sizeof(ptrs_val_t), val.meta);
	jit_insn_branch(func, &end);

	//PTRS_TYPE_NATIVE
	jit_insn_label(func, &isNative);
	jit_value_t byteVal = jit_insn_convert(func, ptrs_jit_vartoi(func, val), jit_type_ubyte, 0);
	jit_insn_store_relative(func, base.val, 0, byteVal);

	jit_insn_label(func, &end);
}

ptrs_jit_var_t ptrs_handle_indexlength(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t ret;
	ret.val = scope->indexSize;
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	ret.constType = PTRS_TYPE_INT;

	return ret;
}

static void ptrs_handle_index_common(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t *base, jit_value_t *type, jit_value_t *size, jit_value_t *index)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	*base = expr->left->handler(expr->left, func, scope);

	*type = ptrs_jit_getType(func, base->meta);
	ptrs_jit_assert(node, func, scope, jit_insn_ge(func, *type, jit_const_int(func, nuint, PTRS_TYPE_NATIVE)),
		1, "Cannot dereference variable of type %t", *type);

	jit_value_t oldSize = scope->indexSize;
	scope->indexSize = ptrs_jit_getArraySize(func, base->meta);

	ptrs_jit_var_t _index = expr->right->handler(expr->right, func, scope);
	*index = ptrs_jit_vartoi(func, _index);

	struct ptrs_assertion *assertion = ptrs_jit_assert(node, func, scope, jit_insn_lt(func, *index, scope->indexSize),
		2, "Index %d is out of range of array of size %d", *index, scope->indexSize);
	ptrs_jit_appendAssert(func, assertion, jit_insn_ge(func, *index, jit_const_long(func, long, 0)));

	if(size != NULL)
		*size = scope->indexSize;
	scope->indexSize = oldSize;
}

ptrs_jit_var_t ptrs_handle_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t base;
	jit_value_t index;
	jit_value_t type;
	ptrs_handle_index_common(node, func, scope, &base, &type, NULL, &index);

	ptrs_jit_var_t result = {
		.val = jit_value_create(func, jit_type_long),
		.meta = jit_value_create(func, jit_type_ulong),
		.constType = -1,
	};

	jit_label_t isPointer = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	jit_insn_branch_if_not(func, jit_insn_eq(func, type, jit_const_int(func, ulong, PTRS_TYPE_NATIVE)), &isPointer);

	//native
	jit_insn_store(func, result.val, jit_insn_load_elem(func, base.val, index, jit_type_ubyte));
	jit_insn_store(func, result.meta, ptrs_jit_const_meta(func, PTRS_TYPE_INT));
	jit_insn_branch(func, &done);

	//pointer
	jit_insn_label(func, &isPointer);

	jit_value_t valIndex = jit_insn_shl(func, index, jit_const_int(func, nint, 1));
	jit_insn_store(func, result.val, jit_insn_load_elem(func, base.val, valIndex, jit_type_long));
	jit_value_t metaIndex = jit_insn_add(func, valIndex, jit_const_int(func, nint, 1));
	jit_insn_store(func, result.meta, jit_insn_load_elem(func, base.val, metaIndex, jit_type_ulong));

	//TODO struct

	jit_insn_label(func, &done);

	return result;
}
void ptrs_handle_assign_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	ptrs_jit_var_t base;
	jit_value_t index;
	jit_value_t type;
	ptrs_handle_index_common(node, func, scope, &base, &type, NULL, &index);

	jit_label_t isPointer = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	jit_insn_branch_if_not(func, jit_insn_eq(func, type, jit_const_int(func, ulong, PTRS_TYPE_NATIVE)), &isPointer);

	//native
	jit_value_t intVal = ptrs_jit_vartoi(func, val);
	jit_value_t uByteVal = jit_insn_convert(func, intVal, jit_type_ubyte, 1);
	jit_insn_store_elem(func, base.val, index, uByteVal);
	jit_insn_branch(func, &done);

	//pointer
	jit_insn_label(func, &isPointer);

	jit_value_t valIndex = jit_insn_shl(func, index, jit_const_int(func, nint, 1));
	jit_insn_store_elem(func, base.val, valIndex, val.val);
	jit_value_t metaIndex = jit_insn_add(func, valIndex, jit_const_int(func, nint, 1));
	jit_insn_store_elem(func, base.val, metaIndex, val.meta);

	//TODO struct

	jit_insn_label(func, &done);
}
ptrs_jit_var_t ptrs_handle_addressof_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t base;
	jit_value_t index;
	jit_value_t type;
	jit_value_t size;
	ptrs_handle_index_common(node, func, scope, &base, &type, &size, &index);

	ptrs_jit_var_t result = {
		.val = jit_value_create(func, jit_type_long),
		.meta = ptrs_jit_setArraySize(func, base.meta, jit_insn_sub(func, size, index)),
		.constType = base.constType,
	};

	jit_label_t isPointer = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	jit_insn_branch_if_not(func, jit_insn_eq(func, type, jit_const_int(func, ulong, PTRS_TYPE_NATIVE)), &isPointer);

	//native
	jit_insn_store(func, result.val, jit_insn_load_elem_address(func, base.val, index, jit_type_ubyte));
	jit_insn_branch(func, &done);

	//pointer
	jit_insn_label(func, &isPointer);

	jit_value_t valIndex = jit_insn_shl(func, index, jit_const_int(func, nint, 1));
	jit_insn_store(func, result.val, jit_insn_load_elem_address(func, base.val, valIndex, jit_type_long));

	//TODO struct

	jit_insn_label(func, &done);

	return result;
}
ptrs_jit_var_t ptrs_handle_call_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_slice(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_slice *expr = &node->arg.slice;

	ptrs_jit_var_t val = expr->base->handler(expr->base, func, scope);
	jit_value_t type = ptrs_jit_getType(func, val.meta);

	jit_value_t oldSize = scope->indexSize;
	scope->indexSize = ptrs_jit_getArraySize(func, val.meta);

	ptrs_jit_var_t _start = expr->start->handler(expr->start, func, scope);
	jit_value_t start = ptrs_jit_vartoi(func, _start);

	ptrs_jit_var_t _end = expr->end->handler(expr->end, func, scope);
	jit_value_t end = ptrs_jit_vartoi(func, _end);

	jit_value_t newSize = jit_insn_sub(func, end, start);
	jit_value_t newPtr = jit_value_create(func, jit_type_void_ptr);

	jit_label_t done = jit_label_undefined;
	jit_insn_store(func, newPtr, jit_insn_load_elem_address(func, val.val, start, jit_type_ubyte));
	jit_insn_branch_if(func, jit_insn_eq(func, type, jit_const_int(func, ulong, PTRS_TYPE_NATIVE)), &done);

	ptrs_jit_assert(node, func, scope, jit_insn_eq(func, type, jit_const_int(func, ulong, PTRS_TYPE_POINTER)),
		1, "Cannot slice variable of type %t", type);
	jit_insn_store(func, newPtr, jit_insn_load_elem_address(func, val.val, start, jit_type_ubyte));

	jit_insn_label(func, &done);

	/*
	disabled so we can slice arrays received from native functions.
	TODO: should there be a seperate syntax for that?

	ptrs_jit_assert(node, func, scope, jit_insn_le(func, end, scope->indexSize),
		2, "Canot end a slice at %d for an array of size %d", end, scope->indexSize);
	ptrs_jit_assert(node, func, scope, jit_insn_ge(func, start, jit_const_long(func, ulong, 0)),
		1, "Canot start a slice at %d", start);
	ptrs_jit_assert(node, func, scope, jit_insn_lt(func, start, end),
		2, "Slice start (%d) is bigger than slice end (%d)", start, end);
	*/

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

	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	val.meta = ptrs_jit_const_meta(func, expr->builtinType);
	val.constType = expr->builtinType;
	return val;
}

ptrs_jit_var_t ptrs_handle_cast_builtin(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;
	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	switch(expr->builtinType)
	{
		case PTRS_TYPE_INT:
			val.val = ptrs_jit_vartoi(func, val);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
			val.constType = PTRS_TYPE_INT;
			break;
		case PTRS_TYPE_FLOAT:
			val.val = ptrs_jit_reinterpretCast(func, ptrs_jit_vartof(func, val), jit_type_long);
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
	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	return ptrs_jit_vartoa(func, val);
}

ptrs_jit_var_t ptrs_handle_cast(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_wildcardsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	return *node->arg.varval;
}
void ptrs_handle_assign_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	ptrs_jit_var_t *target = node->arg.varval;
	jit_insn_store(func, target->val, val.val);

	if(val.constType != -1 && val.constType != target->constType)
		ptrs_error(node, "Cannot assign value of type %t to variable of type %t",
			val.constType, target->constType);
	//TODO add a runtime check when val is dynamically typed?

	if(!jit_value_is_constant(target->meta))
		jit_insn_store(func, target->meta, val.meta);
}

ptrs_jit_var_t ptrs_handle_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	jit_function_t target = *node->arg.funcval;
	jit_function_t closure = jit_function_get_meta(target, PTRS_JIT_FUNCTIONMETA_CLOSURE);

	if(closure == NULL)
	{
		ptrs_function_t *funcAst = jit_function_get_meta(target, PTRS_JIT_FUNCTIONMETA_AST);

		closure = ptrs_jit_createTrampoline(funcAst, target);
		jit_function_set_meta(target, PTRS_JIT_FUNCTIONMETA_CLOSURE, closure, NULL, 0);

		if(ptrs_compileAot && jit_function_compile(closure) == 0)
			ptrs_error(node, "Failed compiling closure of function %s", funcAst->name);
	}

	void *closurePtr = jit_function_to_closure(closure);

	ptrs_jit_var_t ret = {
		.val = jit_const_int(func, void_ptr, (uintptr_t)closurePtr),
		.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, true, 0),
		.constType = PTRS_TYPE_NATIVE,
	};
	return ret;
}
ptrs_jit_var_t ptrs_handle_call_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	return ptrs_jit_vcallnested(func, scope, *node->arg.funcval, arguments);
}

ptrs_jit_var_t ptrs_handle_typed(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_typed(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_constant(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_jit_var_t val;
	val.val = jit_const_long(func, long, node->arg.constval.value.intval);
	val.meta = jit_const_long(func, ulong, *(uint64_t *)&node->arg.constval.meta);
	val.constType = node->arg.constval.meta.type;
	return val;
}

ptrs_jit_var_t ptrs_handle_lazy(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_prefix_typeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t val = node->handler(node, func, scope);

	val.val = ptrs_jit_getType(func, val.meta);
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	val.constType = PTRS_TYPE_INT;
	return val;
}

ptrs_jit_var_t ptrs_handle_op_instanceof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_op_in(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_yield(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
