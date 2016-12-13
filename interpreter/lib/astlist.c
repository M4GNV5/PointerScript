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

			len += val->meta.array.size;
		}
		else
		{
			len++;
		}
		list = list->next;
	}
	return len;
}

void ptrs_astlist_handle(struct ptrs_astlist *list, int len, ptrs_var_t *out, ptrs_scope_t *scope)
{
	int i;
	for(i = 0; list != NULL; i++)
	{
		if(list->entry == NULL)
		{
			out[i].type = PTRS_TYPE_UNDEFINED;
			list = list->next;
			continue;
		}

		ptrs_var_t *curr = list->entry->handler(list->entry, &out[i], scope);

		if(list->expand)
		{
			for(int j = 0; j < curr->meta.array.size; j++)
			{
				memcpy(&out[i], &curr->value.ptrval[j], sizeof(ptrs_var_t));
				i++;
			}
			i--;
		}
		else if(curr != &out[i])
		{
			memcpy(&out[i], curr, sizeof(ptrs_var_t));
		}

		list = list->next;
	}

	ptrs_var_t *last = &out[i - 1];
	for(; i < len; i++)
	{
		memcpy(&out[i], last, sizeof(ptrs_var_t));
	}
}

void ptrs_astlist_handleByte(struct ptrs_astlist *list, int len, uint8_t *out, ptrs_scope_t *scope)
{
	int i;
	for(i = 0; list != NULL; i++)
	{
		if(list->entry == NULL)
		{
			out[i] = 0;
			list = list->next;
			continue;
		}

		ptrs_var_t currv;
		ptrs_var_t *curr = list->entry->handler(list->entry, &currv, scope);

		if(list->expand)
		{
			for(int j = 0; j < curr->meta.array.size; j++)
			{
				out[i] = ptrs_vartoi(&curr->value.ptrval[j]);
				i++;
			}
			i--;
		}
		else
		{
			out[i] = ptrs_vartoi(curr);
		}

		list = list->next;
	}

	uint8_t last = out[i - 1];
	for(; i < len; i++)
	{
		out[i] = last;
	}
}
