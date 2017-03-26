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
			ptrs_var_t valv;
			ptrs_var_t *val = list->entry->handler(list->entry, &valv, scope);
			if(val->type != PTRS_TYPE_POINTER && val->type != PTRS_TYPE_NATIVE)
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
		if(list->lazy)
		{
			out[i].type = (uint8_t)-1;
			out[i].value.nativeval = list->entry;
			list = list->next;
			continue;
		}

		ptrs_var_t *curr = list->entry->handler(list->entry, &out[i], scope);

		if(list->expand)
		{
			ptrs_var_t currv;
			memcpy(&currv, curr, sizeof(ptrs_var_t));

			if(curr->type == PTRS_TYPE_POINTER)
			{
				for(int j = 0; j < currv.meta.array.size; j++)
				{
					memcpy(&out[i], &currv.value.ptrval[j], sizeof(ptrs_var_t));
					i++;
				}
			}
			else //PTRS_TYPE_NATIVE
			{
				for(int j = 0; j < currv.meta.array.size; j++)
				{
					out[i].type = PTRS_TYPE_INT;
					out[i].value.intval = ((uint8_t *)currv.value.strval)[j];
					i++;
				}
			}
			i--;
		}
		else if(curr != &out[i])
		{
			ptrs_var_t *curr = list->entry->handler(list->entry, &out[i], scope);
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
			if(curr->type == PTRS_TYPE_POINTER)
			{
				for(int j = 0; j < curr->meta.array.size; j++)
				{
					out[i] = ptrs_vartoi(&curr->value.ptrval[j]);
					i++;
				}
			}
			else //PTRS_TYPE_NATIVE
			{
				memcpy(out + i, curr->value.strval, curr->meta.array.size);
				i += curr->meta.array.size;
			}
			i--;
		}
		else
		{
			out[i] = ptrs_vartoi(curr);
		}

		list = list->next;
	}

	if(i < len)
		memset(out + i, out[i - 1], len - i);
}
