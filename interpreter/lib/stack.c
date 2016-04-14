#include <stdlib.h>
#include "../include/stack.h"
#include "../include/error.h"

void *ptrs_stack = NULL;
void *ptrs_stackstart = NULL;

void *ptrs_alloc(size_t size)
{
	if(ptrs_stackstart == NULL)
	{
		ptrs_stackstart = malloc(PTRS_STACK_SIZE);
		ptrs_stack = ptrs_stackstart;
	}

	if(ptrs_stack + size >= ptrs_stackstart + PTRS_STACK_SIZE)
		ptrs_error(NULL, "Out of memory");

	void *ptr = ptrs_stack;
	ptrs_stack += size;
	return ptr;
}
