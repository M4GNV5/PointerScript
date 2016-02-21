#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/object.h"
#include "../include/scope.h"

void ptrs_scope_set(ptrs_scope_t *scope, char *key, ptrs_var_t *value)
{
	scope->current = ptrs_object_set(scope->current, key, value);
}

ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, char *key)
{
	ptrs_var_t *val;
	while(scope != NULL)
	{
		val = ptrs_object_get(scope->current, key);
		if(val != NULL)
			return val;
		scope = scope->outer;
	}
	return NULL;
}
