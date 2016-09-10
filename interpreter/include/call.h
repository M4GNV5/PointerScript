#ifndef _PTRS_CALL
#define _PTRS_CALL

int ptrs_astlist_length(struct ptrs_astlist *list, ptrs_ast_t *node, ptrs_scope_t *scope);
void ptrs_astlist_handle(struct ptrs_astlist *list, ptrs_var_t *out, ptrs_scope_t *scope);
ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_vartype_t retType, ptrs_struct_t *thisArg, ptrs_var_t *func,
	ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope);
ptrs_var_t *ptrs_callfunc(ptrs_ast_t *callAst, ptrs_var_t *result, ptrs_scope_t *callScope,
	ptrs_struct_t *thisArg, ptrs_var_t *funcvar, int argc, ptrs_var_t *argv);
ptrs_val_t ptrs_callnative(ptrs_vartype_t retType, void *func, int argc, ptrs_var_t *argv);

#endif
