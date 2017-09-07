#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/conversion.h"

ptrs_jit_var_t ptrs_handle_op_logicxor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope);
	ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope);

	left.val = ptrs_jit_vartob(func, left);
	right.val = ptrs_jit_vartob(func, right);

	right.val = jit_insn_xor(func, left.val, right.val);
	right.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	right.constType = PTRS_TYPE_INT;

	return right;
}

ptrs_jit_var_t ptrs_handle_prefix_logicnot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	ptrs_jit_var_t val = expr->handler(expr, func, scope);
	val.val = ptrs_jit_vartob(func, val);

	val.val = jit_insn_xor(func, val.val, jit_const_long(func, long, 1));
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	val.constType = PTRS_TYPE_INT;

	return val;
}

ptrs_jit_var_t ptrs_handle_op_assign(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope);
	expr->left->setHandler(expr->left, func, scope, right);

	return right;
}

ptrs_jit_var_t ptrs_handle_prefix_plus(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;
 	return expr->handler(expr, func, scope);
}
