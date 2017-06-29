#ifndef _PTRS_CALL
#define _PTRS_CALL

jit_value_t ptrs_jit_call(jit_function_t func,
	jit_type_t retType, jit_value_t val, size_t narg, ptrs_jit_var_t *args);
jit_value_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_type_t retType, jit_value_t val, jit_value_t meta, struct ptrs_astlist *args);

#endif
