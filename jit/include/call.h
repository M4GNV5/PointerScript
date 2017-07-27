#ifndef _PTRS_CALL
#define _PTRS_CALL

jit_value_t ptrs_jit_call(jit_function_t func,
	jit_type_t retType, jit_value_t val, size_t narg, ptrs_jit_var_t *args);
jit_value_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_type_t retType, jit_value_t val, jit_value_t meta, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, jit_function_t callee, size_t narg, ptrs_jit_var_t *args);
ptrs_jit_var_t ptrs_jit_vcallnested(jit_function_t func, ptrs_scope_t *scope,
	jit_function_t callee, struct ptrs_astlist *args);

jit_function_t ptrs_jit_createTrampoline(ptrs_function_t *funcAst, jit_function_t func);

#endif
