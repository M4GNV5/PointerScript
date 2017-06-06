#ifndef _PTRS_CALL
#define _PTRS_CALL

ptrs_jit_var_t ptrs_jit_callfunc(jit_function_t func, jit_value_t val, size_t narg, jit_value_t *args);
ptrs_jit_var_t ptrs_jit_callnative(jit_function_t func, jit_value_t val, size_t narg, ptrs_jit_var_t *args);
ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func,
	jit_value_t val, jit_value_t meta, size_t narg, ptrs_jit_var_t *args);
ptrs_jit_var_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t val, jit_value_t meta, struct ptrs_astlist *args);

#endif
