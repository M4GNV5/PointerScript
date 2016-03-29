#include <stdlib.h>
#include <string.h>

#include "../../parser/common.h"

ptrs_object_t *ptrs_object_set(ptrs_object_t *obj, char *key, ptrs_var_t *value)
{
	ptrs_object_t *startObj = obj;
	if(obj != NULL)
	{
		for(;;)
		{
			if(strcmp(obj->key, key) == 0)
			{
				// TODO gc old value
				obj->value = value;
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
	new->key = strdup(key);
	new->value = value;

	return startObj;
}

ptrs_var_t *ptrs_object_get(ptrs_object_t *obj, char *key)
{
	while(obj != NULL)
	{
		if(strcmp(obj->key, key) == 0)
			return obj->value;
		obj = obj->next;
	}
	return NULL;
}
