#ifndef _PTRS_MEMORY
#define _PTRS_MEMORY

#include <stdlib.h>
#include "../../parser/common.h"

ptrs_var_t *ptrs_alloc();
ptrs_var_t *ptrs_vardup(ptrs_var_t *value);
extern void *ptrs_stack;

#endif
