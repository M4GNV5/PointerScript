#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "../parser/common.h"
#include "../parser/ast.h"
#include "include/conversion.h"

#define handle_binary(name, operator) \
	unsigned ptrs_handle_op_##name(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		unsigned left = expr->left->handler(expr->left, jit, scope); \
		assert(scope->usedRegCount == left); \
		scope->usedRegCount += 2; \
		\
		unsigned right = expr->right->handler(expr->right, jit, scope); \
		\
		/* TODO floats */ \
		jit_##operator##r(jit, R(left), R(left), R(right)); \
		scope->usedRegCount -= 2; \
		return left; \
	}

#define handle_binary_assign(name, operator) \
	unsigned ptrs_handle_op_##name(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		unsigned right = expr->right->handler(expr->right, jit, scope); \
		assert(scope->usedRegCount == right); \
		scope->usedRegCount += 2; \
		\
		unsigned left = expr->left->handler(expr->left, jit, scope); \
		\
		/* TODO floats */ \
		jit_##operator##r(jit, R(left), R(left), R(right)); \
		expr->left->setHandler(expr->left, jit, scope, R(right), R(right + 1)); \
		\
		scope->usedRegCount -= 2; \
		return left; \
	}

#define handle_binary_logic(name, comparer) \
	unsigned ptrs_handle_op_##name(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope) \
	{ \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		/*evaluate the left-side expression*/ \
		unsigned left = expr->left->handler(expr->left, jit, scope); \
		assert(scope->usedRegCount == left); \
		scope->usedRegCount = left + 2; \
		\
		/*convert the left-side expression to a boolean*/ \
		long tmpVal = R(scope->usedRegCount); \
		long tmpMeta = R(scope->usedRegCount + 1); \
		jit_movr(jit, tmpVal, R(left)); \
		jit_movr(jit, tmpMeta, R(left + 1)); \
		ptrs_jit_vartob(jit, tmpVal, tmpMeta); \
		\
		/*conditionally jump over the evaluation of the right-side expression*/ \
		jit_op *skip = jit_##comparer##i(jit, (uintptr_t)JIT_FORWARD, tmpVal, 0); \
		\
		/*overwrite the return value with the right-side expression*/ \
		unsigned right = expr->right->handler(expr->right, jit, scope); \
		jit_movr(jit, R(left), R(right)); \
		jit_movr(jit, R(left + 1), R(right + 1)); \
		\
		jit_patch(jit, skip); \
		scope->usedRegCount -= 2; \
		return left; \
	}

//handle_binary(typeequal) TODO
//handle_binary(typeinequal) TODO
handle_binary(equal, eq) //==
handle_binary(inequal, ne) //!=
handle_binary(lessequal, le) //<=
handle_binary(greaterequal, ge) //>=
handle_binary(less, lt) //<
handle_binary(greater, gt) //>
handle_binary(or, or) //|
handle_binary(xor, xor) //^
handle_binary(and, and) //&
handle_binary(shr, rsh) //>>
handle_binary(shl, lsh) //<<
handle_binary(add, add) //+
handle_binary(sub, sub) //-
handle_binary(mul, mul) //*
handle_binary(div, div) ///
handle_binary(mod, mod) //%
handle_binary_assign(addassign, add) //+=
handle_binary_assign(subassign, sub) //-=
handle_binary_assign(mulassign, mul) //*=
handle_binary_assign(divassign, div) ///=
handle_binary_assign(modassign, mod) //%=
handle_binary_assign(shrassign, rsh) //>>=
handle_binary_assign(shlassign, lsh) //<<=
handle_binary_assign(andassign, and) //&=
handle_binary_assign(xorassign, xor) //^=
handle_binary_assign(orassign, or) //|=
handle_binary_logic(logicor, bne) //||
handle_binary_logic(logicand, beq) //&&

unsigned ptrs_handle_op_logicxor(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	unsigned left = expr->left->handler(expr->left, jit, scope);
	assert(scope->usedRegCount == left);
	scope->usedRegCount += 2;

	unsigned right = expr->right->handler(expr->right, jit, scope);

	ptrs_jit_vartob(jit, R(left), R(left + 1));
	ptrs_jit_vartob(jit, R(right), R(right + 1));
	jit_xorr(jit, R(left), R(left), R(right));

	scope->usedRegCount -= 2;
	return left;
}

unsigned ptrs_handle_prefix_logicnot(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	unsigned left = expr->handler(expr, jit, scope);
	ptrs_jit_vartob(jit, R(left), R(left + 1));
	jit_xori(jit, R(left), R(left), 1);

	return left;
}

unsigned ptrs_handle_op_assign(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary *expr = &node->arg.binary;

	unsigned right = expr->right->handler(expr->right, jit, scope);
	assert(scope->usedRegCount == right); \
	scope->usedRegCount += 2;

	expr->left->setHandler(expr->left, jit, scope, right, right + 1);

	scope->usedRegCount -= 2;
	return right;
}

unsigned ptrs_handle_prefix_plus(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;
 	return expr->handler(expr, jit, scope);
}

#define jit_incr(jit, a, b) (jit_addi(jit, a, b, 1))
#define jit_decr(jit, a, b) (jit_subi(jit, a, b, 1))

#define handle_prefix(name, operator, isAssign) \
	unsigned ptrs_handle_prefix_##name(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		unsigned val = expr->handler(expr, jit, scope); \
		assert(scope->usedRegCount == val); \
		scope->usedRegCount += 2; \
		\
		/* TODO floats */ \
		jit_##operator##r(jit, R(val), R(val)); \
		if(isAssign) \
			expr->setHandler(expr, jit, scope, R(val), R(val + 1)); \
		\
		scope->usedRegCount -= 2; \
		return val; \
	}

handle_prefix(inc, inc, true)
handle_prefix(dec, dec, true)
handle_prefix(not, not, false)
handle_prefix(minus, neg, false)

#define handle_suffix(name, operator) \
	unsigned ptrs_handle_suffix_##name(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		unsigned val = expr->handler(expr, jit, scope); \
		assert(scope->usedRegCount == val); \
		scope->usedRegCount += 3; \
		\
		/* TODO floats */ \
		jit_##operator##r(jit, R(val + 2), R(val)); \
		expr->setHandler(expr, jit, scope, R(val + 2), R(val + 1)); \
		\
		scope->usedRegCount -= 3; \
		return val; \
	}

handle_suffix(inc, inc)
handle_suffix(dec, dec)
