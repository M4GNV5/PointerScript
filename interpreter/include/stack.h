#ifndef _PTRS_STACK
#define _PTRS_STACK

#include "../../parser/common.h"

#define PTRS_STACK_SIZE (1024 * 1024)
extern size_t ptrs_stacksize;

void *ptrs_alloc(ptrs_scope_t *scope, size_t size);

#endif
