#ifndef _PTRS_CALL
#define _PTRS_CALL

ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_nativetype_info_t *retType, ptrs_struct_t *thisArg, ptrs_var_t *func,
	ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope);
ptrs_var_t *ptrs_callfunc(ptrs_ast_t *callAst, ptrs_var_t *result, ptrs_scope_t *callScope,
	ptrs_struct_t *thisArg, ptrs_var_t *funcvar, int argc, ptrs_var_t *argv);
int64_t ptrs_callnative(ptrs_nativetype_info_t *retType, ptrs_var_t *result, void *func, int argc, ptrs_var_t *argv);

#ifndef _PTRS_NOCALLBACK
#include <ffcb.h>
void ptrs_callcallback(ffcb_return_t ret, ptrs_function_t *func, va_list ap);
#endif

#endif
