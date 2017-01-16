#ifndef _PTRS_RUN
#define _PTRS_RUN

#include "../../parser/common.h"

ptrs_ast_t *ptrs_eval(char *src, const char *filename, ptrs_var_t *result, ptrs_scope_t *scope, ptrs_symboltable_t **symbols);
ptrs_ast_t *ptrs_dofile(const char *file, ptrs_var_t *result, ptrs_scope_t *scope, ptrs_symboltable_t **symbols);

#endif
