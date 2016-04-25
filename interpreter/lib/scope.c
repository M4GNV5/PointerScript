#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/stack.h"
#include "../include/scope.h"

ptrs_object_t *ptrs_object_set(ptrs_object_t *obj, const char *key, ptrs_var_t *value)
{
	ptrs_object_t *startObj = obj;
	if(obj != NULL)
	{
		for(;;)
		{
			if(strcmp(obj->key, key) == 0)
			{
				memcpy(obj->value, value, sizeof(ptrs_var_t));
				return startObj;
			}

			if(obj->next == NULL)
				break;

			obj = obj->next;
		}
	}

	ptrs_object_t *new = ptrs_alloc(sizeof(ptrs_object_t));
	if(obj == NULL)
		startObj = new;
	else
		obj->next = new;

	new->next = NULL;
	new->key = key;

	new->value = ptrs_alloc(sizeof(ptrs_var_t));
	memcpy(new->value, value, sizeof(ptrs_var_t));

	return startObj;
}

ptrs_var_t *ptrs_object_get(ptrs_object_t *obj, const char *key)
{
	while(obj != NULL)
	{
		if(strcmp(obj->key, key) == 0)
			return obj->value;
		obj = obj->next;
	}
	return NULL;
}

void ptrs_scope_set(ptrs_scope_t *scope, const char *key, ptrs_var_t *value)
{
	scope->current = ptrs_object_set(scope->current, key, value);
}

ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, const char *key)
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
