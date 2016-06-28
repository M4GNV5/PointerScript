#include <string.h>

#include "../include/error.h"
#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

int ptrs_astlist_length(struct ptrs_astlist *list, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	int len = 0;
	while(list != NULL)
	{
		if(list->expand)
		{
			ptrs_var_t *val = ptrs_scope_get(scope, list->entry->arg.varval);
			if(val->type != PTRS_TYPE_POINTER)
				ptrs_error(node, scope, "Cannot expand variable of type %s", ptrs_typetoa(val->type));

			val = val->value.ptrval;
			for(int i = 0; val[i].type != PTRS_TYPE_NATIVE || val[i].value.nativeval != NULL; i++)
				len++;
		}
		else
		{
			len++;
		}
		list = list->next;
	}
	return len;
}
void ptrs_astlist_handle(struct ptrs_astlist *list, ptrs_var_t *out, ptrs_scope_t *scope)
{
	for(int i = 0; list != NULL; i++)
	{
		ptrs_var_t *curr = list->entry->handler(list->entry, &out[i], scope);

		if(list->expand)
		{
			curr = curr->value.ptrval;
			for(int j = 0; curr[j].type != PTRS_TYPE_NATIVE || curr[j].value.nativeval != NULL; j++)
			{
				memcpy(&out[i], &curr[j], sizeof(ptrs_var_t));
				i++;
			}
		}
		else if(curr != &out[i])
		{
			memcpy(&out[i], curr, sizeof(ptrs_var_t));
		}

		list = list->next;
	}
}
