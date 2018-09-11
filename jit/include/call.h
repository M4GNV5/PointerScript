#ifndef _PTRS_CALL
#define _PTRS_CALL

void ptrs_arglist_handle(jit_function_t func, ptrs_scope_t *scope,
	struct ptrs_astlist *curr, ptrs_jit_var_t *buff);

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, struct ptrs_astlist *args);
ptrs_jit_var_t ptrs_jit_ncallnested(jit_function_t func,
	jit_value_t thisPtr, jit_function_t callee, size_t narg, ptrs_jit_var_t *args);

jit_function_t ptrs_jit_createFunction(ptrs_ast_t *node, jit_function_t parent,
	jit_type_t signature, const char *name);
jit_function_t ptrs_jit_createFunctionFromAst(ptrs_ast_t *node, jit_function_t parent,
	ptrs_function_t *ast);
void ptrs_jit_buildFunction(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType);
jit_function_t ptrs_jit_compileFunction(ptrs_ast_t *node, jit_function_t parent, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType);

#endif
