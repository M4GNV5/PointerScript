#ifndef _PTRS_CALL
#define _PTRS_CALL

ptrs_var_t *ptrs_callfunc(ptrs_function_t *func, ptrs_var_t *result, int argc, ptrs_var_t *argv);
intptr_t ptrs_callnative(ptrs_ast_t *ast, void *func, int argc, ptrs_var_t *argv);

#endif
