#include <string.h>

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, const char *op)
{
	struct ptrs_opoverload *curr = struc->value.structval->overloads;
	while(curr != NULL)
	{
		if(strcmp(curr->op, op) == 0)
			return curr->handler;
		curr = curr->next;
	}
	return NULL;
}

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key)
{
	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(strcmp(curr->name, key) == 0)
		{
			switch(curr->type)
			{
				case PTRS_STRUCTMEMBER_VAR:
					return struc->data + curr->offset;
				case PTRS_STRUCTMEMBER_FUNCTION:
					result->type = PTRS_TYPE_FUNCTION;
					result->value.funcval = curr->value.function;
					result->meta.this = struc;
					return result;
				case PTRS_STRUCTMEMBER_ARRAY:
					result->type = PTRS_TYPE_NATIVE;
					result->value.nativeval = struc->data + curr->offset;
					return result;
				case PTRS_STRUCTMEMBER_VARARRAY:
					result->type = PTRS_TYPE_POINTER;
					result->value.ptrval = struc->data + curr->offset;
					return result;
			}
		}
		curr = curr->next;
	}

	return NULL;
}
