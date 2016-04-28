#include <stdlib.h>
#include "../include/stack.h"
#include "../include/scope.h"
#include "../include/error.h"

void *ptrs_alloc(ptrs_scope_t *scope, size_t size)
{
	if(scope->sp == NULL)
	{
		scope->sp = malloc(PTRS_STACK_SIZE);
		scope->stackstart = scope->sp;
	}

	if(scope->sp + size >= scope->stackstart + PTRS_STACK_SIZE)
		ptrs_error(NULL, NULL, "Out of memory");

	void *ptr = scope->sp;
	scope->sp += size;
	return ptr;
}
