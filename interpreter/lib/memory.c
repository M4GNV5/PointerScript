#include <stdlib.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/error.h"

#define PTRS_VARSPACE_SIZE 1024 * 1024

ptrs_var_t *ptrs_heap_start;
ptrs_var_t *ptrs_heap;

ptrs_var_t *ptrs_alloc()
{
	if(ptrs_heap == NULL)
	{
		ptrs_heap_start = malloc(PTRS_VARSPACE_SIZE);
		ptrs_heap = ptrs_heap_start - sizeof(ptrs_var_t);
	}

	ptrs_heap += sizeof(ptrs_var_t);
	if(ptrs_heap - ptrs_heap_start >= PTRS_VARSPACE_SIZE)
	{
		// TODO garbage collection
		ptrs_error(NULL, "Out of memory");
		return NULL;
	}

	return ptrs_heap;
}

ptrs_var_t *ptrs_vardup(ptrs_var_t *value)
{
	ptrs_var_t *dup = ptrs_alloc();
	memcpy(dup, value, sizeof(ptrs_var_t));
	return dup;
}
