#ifndef _PTRS_CALL
#define _PTRS_CALL

#include <callback.h>

int ptrs_astlist_length(struct ptrs_astlist *list);
void ptrs_astlist_handle(struct ptrs_astlist *list, ptrs_var_t *out, ptrs_scope_t *scope);
ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_var_t *func, ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope);
ptrs_var_t *ptrs_callfunc(ptrs_ast_t *callAst, ptrs_var_t *result, ptrs_scope_t *callScope, ptrs_var_t *funcvar, int argc, ptrs_var_t *argv);
void ptrs_callcallback(ptrs_function_t *func, va_alist alist);
intptr_t ptrs_callnative(void *func, int argc, ptrs_var_t *argv);

#endif
