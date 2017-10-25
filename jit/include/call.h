#ifndef _PTRS_CALL
#define _PTRS_CALL

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, struct ptrs_astlist *args);

jit_function_t ptrs_jit_createFunction(ptrs_ast_t *node, jit_function_t parent,
	jit_type_t signature, const char *name);
jit_function_t ptrs_jit_compileFunction(ptrs_ast_t *node, jit_function_t parent, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType);

#endif
