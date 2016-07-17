#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, ptrs_asthandler_t handler, bool isLeftSide)
{
	struct ptrs_opoverload *curr = struc->value.structval->overloads;
	while(curr != NULL)
	{
		if(curr->op == handler && curr->isLeftSide == isLeftSide)
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
			if(struc->data == NULL && curr->type != PTRS_STRUCTMEMBER_FUNCTION)
				return NULL;

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
					result->meta.array.readOnly = false;
					result->meta.array.size = curr->value.size;
					return result;
				case PTRS_STRUCTMEMBER_VARARRAY:
					result->type = PTRS_TYPE_POINTER;
					result->value.ptrval = struc->data + curr->offset;
					result->meta.array.size = curr->value.size;
					return result;
			}
		}
		curr = curr->next;
	}

	return NULL;
}
