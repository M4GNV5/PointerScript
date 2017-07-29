#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "../parser/common.h"
#include "../parser/ast.h"
#include "include/conversion.h"
#include "include/util.h"

#define handle_binary(name, operator) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope); \
		ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope); \
		\
		/* TODO floats & correct meta type & correct meta array size */ \
		right.val = jit_insn_##operator(func, left.val, right.val); \
		return right; \
	}

#define handle_binary_assign(name, operator) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope); \
		ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope); \
		\
		/* TODO floats & correct meta type & correct meta array size */ \
		left.val = jit_insn_##operator(func, left.val, right.val); \
		expr->left->setHandler(expr->left, func, scope, left); \
		\
		return left; \
	}

#define handle_binary_typecompare(comparer) \
	right.val = jit_insn_and(func, left.val, \
		jit_insn_##comparer(func, \
			ptrs_jit_getType(func, left.meta), \
			ptrs_jit_getType(func, right.meta)) \
	);

#define handle_binary_compare(name, comparer, extra) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope); \
		ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope); \
		\
		/* TODO floats */ \
		left.val = jit_insn_##comparer(func, left.val, right.val); \
		\
		extra \
		\
		left.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT); \
		return left; \
	}

#define handle_binary_logic(name, comparer, constComparer) \
	ptrs_jit_var_t ptrs_handle_op_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		/*evaluate the left-side expression*/ \
		ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope); \
		if(jit_value_is_constant(left.val) && jit_value_is_constant(left.meta)) \
		{ \
			jit_long constVal = jit_value_get_long_constant(ptrs_jit_vartob(func, left.val, left.meta)); \
			if(constVal constComparer true) \
				return expr->right->handler(expr->right, func, scope); \
			\
			ptrs_jit_var_t ret = { \
				.val = jit_const_long(func, long, 0), \
				.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT) \
			}; \
			return ret; \
		} \
		\
		ptrs_jit_var_t tmp = { \
			.val = jit_value_create(func, jit_type_long), \
			.meta = jit_value_create(func, jit_type_ulong), \
		}; \
		jit_insn_store(func, tmp.val, left.val); \
		jit_insn_store(func, tmp.meta, left.meta); \
		\
		/*conditionally jump over the evaluation of the right-side expression*/ \
		jit_label_t skip = jit_label_undefined; \
		ptrs_jit_branch_##comparer(func, &skip, left.val, left.meta); \
		\
		/*overwrite the return value with the right-side expression*/ \
		ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope); \
		jit_insn_store(func, tmp.val, right.val); \
		jit_insn_store(func, tmp.meta, right.meta); \
		\
		jit_insn_label(func, &skip); \
		return tmp; \
	}

handle_binary_compare(typeequal, eq, handle_binary_typecompare(eq)) //===
handle_binary_compare(typeinequal, ne, handle_binary_typecompare(ne)) //!==
handle_binary_compare(equal, eq, ) //==
handle_binary_compare(inequal, ne, ) //!=
handle_binary_compare(lessequal, le, ) //<=
handle_binary_compare(greaterequal, ge, ) //>=
handle_binary_compare(less, lt, ) //<
handle_binary_compare(greater, gt, ) //>
handle_binary(or, or) //|
handle_binary(xor, xor) //^
handle_binary(and, and) //&
handle_binary(shr, sshr) //>>
handle_binary(shl, shl) //<<
handle_binary(add, add) //+
handle_binary(sub, sub) //-
handle_binary(mul, mul) //*
handle_binary(div, div) ///
handle_binary(mod, rem) //%
handle_binary_assign(addassign, add) //+=
handle_binary_assign(subassign, sub) //-=
handle_binary_assign(mulassign, mul) //*=
handle_binary_assign(divassign, div) ///=
handle_binary_assign(modassign, rem) //%=
handle_binary_assign(shrassign, sshr) //>>=
handle_binary_assign(shlassign, shl) //<<=
handle_binary_assign(andassign, and) //&=
handle_binary_assign(xorassign, xor) //^=
handle_binary_assign(orassign, or) //|=
handle_binary_logic(logicor, if, !=) //||
handle_binary_logic(logicand, if_not, ==) //&&

ptrs_jit_var_t ptrs_handle_op_logicxor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	ptrs_jit_var_t left = expr->left->handler(expr->left, func, scope);
	ptrs_jit_var_t right = expr->right->handler(expr->right, func, scope);

	left.val = ptrs_jit_vartob(func, left.val, left.meta);
	right.val = ptrs_jit_vartob(func, left.val, left.meta);

	right.val = jit_insn_xor(func, left.val, right.val);
	right.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);

	return right;
}

ptrs_jit_var_t ptrs_handle_prefix_logicnot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	ptrs_jit_var_t val = expr->handler(expr, func, scope);
	val.val = ptrs_jit_vartob(func, val.val, val.meta);

	val.val = jit_insn_xor(func, val.val, jit_const_long(func, long, 1));
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);

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

#define jit_insn_inc(func, val) (jit_insn_add(func, val, jit_const_long(func, long, 1)))
#define jit_insn_dec(func, val) (jit_insn_sub(func, val, jit_const_long(func, long, 1)))

#define handle_prefix(name, operator, isAssign) \
	ptrs_jit_var_t ptrs_handle_prefix_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		ptrs_jit_var_t val = expr->handler(expr, func, scope); \
		\
		/* TODO floats */ \
		val.val = jit_insn_##operator(func, val.val); \
		if(isAssign) \
			expr->setHandler(expr, func, scope, val); \
		\
		return val; \
	}

handle_prefix(inc, inc, true)
handle_prefix(dec, dec, true)
handle_prefix(not, not, false)
handle_prefix(minus, neg, false)

#define handle_suffix(name, operator) \
	ptrs_jit_var_t ptrs_handle_suffix_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		ptrs_jit_var_t val = expr->handler(expr, func, scope); \
		ptrs_jit_var_t writeback = val; \
		\
		/* TODO floats */ \
		writeback.val = jit_insn_##operator(func, val.val); \
		expr->setHandler(expr, func, scope, writeback); \
		\
		return val; \
	}

handle_suffix(inc, inc)
handle_suffix(dec, dec)
