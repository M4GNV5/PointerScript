#ifndef _PTRS_RUN
#define _PTRS_RUN

#include "../../parser/common.h"

extern const char *ptrs_file;

void ptrs_eval(char *src, ptrs_var_t *result, ptrs_scope_t *scope);
void ptrs_dofile(const char *file, ptrs_var_t *result, ptrs_scope_t *scope);

#endif
