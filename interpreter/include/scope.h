#ifndef _PTRS_SCOPE
#define _PTRS_SCOPE

#include <stdbool.h>
#include "../../parser/common.h"

typedef struct ptrs_scope
{
	struct ptrs_scope *outer;
	ptrs_object_t *current;
} ptrs_scope_t;

ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, char *name);
void ptrs_scope_set(ptrs_scope_t *scope, char *name, ptrs_var_t *value);

#endif
