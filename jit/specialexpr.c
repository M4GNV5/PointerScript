#include <stdint.h>
#include <assert.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"

ptrs_jit_var_t ptrs_handle_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_stringformat(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
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

ptrs_jit_var_t ptrs_handle_thismember(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_thismember(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_addressof_thismember(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_call_thismember(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_prefix_length(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_prefix_address(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_indexlength(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_addressof_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_jit_var_t ptrs_handle_call_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_slice(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_as(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;

	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	val.meta = ptrs_jit_const_meta(func, expr->builtinType);
	return val;
}

ptrs_jit_var_t ptrs_handle_cast_builtin(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast *expr = &node->arg.cast;
	ptrs_jit_var_t val = expr->value->handler(expr->value, func, scope);

	switch(expr->builtinType)
	{
		case PTRS_TYPE_INT:
			val.val = ptrs_jit_vartoi(func, val.val, val.meta);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
			break;
		case PTRS_TYPE_FLOAT:
			val.val = ptrs_jit_vartof(func, val.val, val.meta);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
			break;
		default:
			ptrs_error(node, "Cannot convert to type %s", ptrs_typetoa(expr->builtinType));
	}

	return val;
}

ptrs_jit_var_t ptrs_handle_tostring(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
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
	jit_insn_store(func, target->meta, val.meta);
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

	val.val = ptrs_jit_get_type(func, val.meta);
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	return val;
}

ptrs_jit_var_t ptrs_handle_op_ternary(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
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
