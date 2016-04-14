#ifndef _PTRS_STACK
#define _PTRS_STACK

#define PTRS_STACK_SIZE (1024 * 1024)

extern void *ptrs_stack;
void *ptrs_alloc(size_t size);

#endif
