#ifndef _PTRS_CALL
#define _PTRS_CALL

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_typing_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_jit_ncallnested(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, size_t narg, ptrs_jit_var_t *args);
ptrs_jit_var_t ptrs_jit_callnested(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, struct ptrs_astlist *args);

void *ptrs_jit_function_to_closure(ptrs_ast_t *node, jit_function_t func);

void ptrs_jit_returnFromFunction(ptrs_ast_t *node, jit_function_t func, ptrs_jit_var_t val);

jit_function_t ptrs_jit_createFunction(ptrs_ast_t *node, jit_function_t parent,
	jit_type_t signature, const char *name);
jit_function_t ptrs_jit_createFunctionFromAst(ptrs_ast_t *node, jit_function_t parent,
	ptrs_function_t *ast);
void ptrs_jit_buildFunction(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType);

#endif
