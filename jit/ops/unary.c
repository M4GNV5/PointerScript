#include "../../parser/common.h"
#include "../../parser/ast.h"

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
