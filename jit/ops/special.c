#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/conversion.h"
#include "../include/util.h"

ptrs_jit_var_t ptrs_handle_op_logicxor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t left = expr->left->vtable->get(expr->left, func, scope);
	ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope);

	left.val = ptrs_jit_vartob(func, left);
	right.val = ptrs_jit_vartob(func, right);

	right.val = jit_insn_xor(func, left.val, right.val);
	right.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	right.constType = PTRS_TYPE_INT;

	return right;
}

ptrs_jit_var_t ptrs_handle_op_ternary(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_ternary *expr = &node->arg.ternary;
	jit_label_t isFalse = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	ptrs_jit_var_t condition = expr->condition->vtable->get(expr->condition, func, scope);

	ptrs_jit_var_t ret = {
		.val = jit_value_create(func, jit_type_long),
		.meta = jit_value_create(func, jit_type_ulong),
		.constType = -1,
	};

	ptrs_jit_branch_if_not(func, &isFalse, condition);

	//is true
	ptrs_jit_var_t trueVal = expr->trueVal->vtable->get(expr->trueVal, func, scope);
	trueVal.val = ptrs_jit_reinterpretCast(func, trueVal.val, jit_type_long);

	jit_insn_store(func, ret.val, trueVal.val);
	jit_insn_store(func, ret.meta, trueVal.meta);
	jit_insn_branch(func, &done);

	//is false
	jit_insn_label(func, &isFalse);

	ptrs_jit_var_t falseVal = expr->falseVal->vtable->get(expr->falseVal, func, scope);
	falseVal.val = ptrs_jit_reinterpretCast(func, falseVal.val, jit_type_long);

	jit_insn_store(func, ret.val, falseVal.val);
	jit_insn_store(func, ret.meta, falseVal.meta);

	jit_insn_label(func, &done);
	ret.constType = trueVal.constType == falseVal.constType ? trueVal.constType : -1;
	return ret;
}

ptrs_jit_var_t ptrs_handle_op_assign(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t right = expr->right->vtable->get(expr->right, func, scope);
	expr->left->vtable->set(expr->left, func, scope, right);

	return right;
}

ptrs_jit_var_t ptrs_handle_prefix_logicnot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	ptrs_jit_var_t val = expr->vtable->get(expr, func, scope);
	val.val = ptrs_jit_vartob(func, val);

	val.val = jit_insn_xor(func, val.val, jit_const_long(func, long, 1));
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	val.constType = PTRS_TYPE_INT;

	return val;
}

ptrs_jit_var_t ptrs_handle_prefix_plus(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;
 	return expr->vtable->get(expr, func, scope);
}
