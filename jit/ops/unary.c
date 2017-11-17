#include "../../parser/common.h"
#include "../../parser/ast.h"

#include "../include/util.h"

#define jit_insn_inc(func, val) (jit_insn_add(func, val, jit_const_long(func, long, 1)))
#define jit_insn_dec(func, val) (jit_insn_sub(func, val, jit_const_long(func, long, 1)))

#define handle_prefix(name, operator, isAssign) \
	ptrs_jit_var_t ptrs_handle_prefix_##name(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		ptrs_jit_var_t val = expr->handler(expr, func, scope); \
		\
		if(val.constType != -1) \
		{ \
			if(val.constType == PTRS_TYPE_FLOAT) \
				val.val = jit_insn_##operator(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64)); \
			else \
				val.val = jit_insn_##operator(func, val.val); \
		} \
		else \
		{ \
			jit_value_t ret = jit_value_create(func, jit_type_long); \
			jit_label_t isFloat = jit_label_undefined; \
			jit_label_t done = jit_label_undefined; \
			\
			jit_insn_branch_if(func, ptrs_jit_hasType(func, val.meta, PTRS_TYPE_FLOAT), &isFloat); \
			jit_insn_store(func, ret, jit_insn_##operator(func, val.val)); \
			jit_insn_branch(func, &done); \
			\
			jit_insn_label(func, &isFloat); \
			val.val = jit_insn_##operator(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64)); \
			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long); \
			jit_insn_store(func, ret, val.val); \
			\
			jit_insn_label(func, &done); \
			val.val = ret; \
		} \
		\
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
		if(val.constType != -1) \
		{ \
			if(val.constType == PTRS_TYPE_FLOAT) \
				writeback.val = jit_insn_##operator(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64)); \
			else \
				writeback.val = jit_insn_##operator(func, val.val); \
		} \
		else \
		{ \
			jit_value_t ret = jit_value_create(func, jit_type_long); \
			jit_label_t isFloat = jit_label_undefined; \
			jit_label_t done = jit_label_undefined; \
			\
			jit_insn_branch_if(func, ptrs_jit_hasType(func, val.meta, PTRS_TYPE_FLOAT), &isFloat); \
			jit_insn_store(func, ret, jit_insn_##operator(func, val.val)); \
			jit_insn_branch(func, &done); \
			\
			jit_insn_label(func, &isFloat); \
			writeback.val = jit_insn_##operator(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64)); \
			writeback.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long); \
			jit_insn_store(func, ret, val.val); \
			\
			jit_insn_label(func, &done); \
			writeback.val = ret; \
		} \
		\
		expr->setHandler(expr, func, scope, writeback); \
		\
		return val; \
	}

handle_suffix(inc, inc)
handle_suffix(dec, dec)
