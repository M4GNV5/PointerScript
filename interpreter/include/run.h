#ifndef _PTRS_RUN
#define _PTRS_RUN

#include "../../parser/common.h"

void ptrs_eval(char *src, const char *filename, ptrs_var_t *result, ptrs_scope_t *scope, ptrs_symboltable_t **symbols);
void ptrs_dofile(const char *file, ptrs_var_t *result, ptrs_scope_t *scope, ptrs_symboltable_t **symbols);

#endif
