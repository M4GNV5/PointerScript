#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../include/stack.h"
#include "../include/scope.h"
#include "../include/error.h"

size_t ptrs_stacksize = PTRS_STACK_SIZE;
bool ptrs_zeroMemory = false;

void *ptrs_alloc(ptrs_scope_t *scope, size_t size)
{
	if(scope->sp == NULL)
	{
		scope->sp = malloc(ptrs_stacksize);
		scope->bp = scope->sp;
		scope->stackstart = scope->sp;
	}

	if(scope->sp + size >= scope->stackstart + ptrs_stacksize)
		ptrs_error(NULL, scope, "Out of memory");

	void *ptr = scope->sp;
	scope->sp += size;

	if(ptrs_zeroMemory)
		memset(ptr, 0, size);

	return ptr;
}
