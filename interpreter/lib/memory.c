#include <stdlib.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/error.h"

//TODO rework this to more dynamic memory management
#define PTRS_VARSPACE_SIZE 1024 * 1024

void *ptrs_stack_start = NULL;
void *ptrs_stack = NULL;

void *ptrs_alloc(size_t size)
{
	if(ptrs_stack == NULL)
	{
		ptrs_stack_start = malloc(PTRS_VARSPACE_SIZE);
		ptrs_stack = ptrs_stack_start - size;
	}

	ptrs_stack += size;
	if(ptrs_stack - ptrs_stack_start >= PTRS_VARSPACE_SIZE)
	{
		ptrs_error(NULL, "Out of memory");
		return NULL;
	}

	return ptrs_stack;
}

ptrs_var_t *ptrs_vardup(ptrs_var_t *value)
{
	ptrs_var_t *dup = ptrs_alloc(sizeof(ptrs_var_t));
	memcpy(dup, value, sizeof(ptrs_var_t));
	return dup;
}
