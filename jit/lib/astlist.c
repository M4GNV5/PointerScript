#include <string.h>

#include "../include/error.h"
#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

void ptrs_astlist_handle(struct ptrs_astlist *list, int len, ptrs_var_t *out, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_astlist_handleByte(struct ptrs_astlist *list, unsigned fpOffset, jit_state_t *jit, ptrs_scope_t *scope)
{
	int i;
	for(i = 0; list != NULL; i++)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			jit_movi(jit, R(0), 0);
		}
		else
		{
			list->entry->handler(list->entry, &currv, scope);
			ptrs_jit_convert(jit, ptrs_vartoi, R(0), R(0), R(1));
		}

		ptrs_jit_load_val(R(1), fpOffset);
		jit_stxi(jit, R(1), i, R(0), 1);

		list = list->next;
	}

	ptrs_jit_load_val(R(0), fpOffset);
	ptrs_jit_load_arraysize(R(1), fpOffset);
	jit_subi(jit, R(1), R(1), i);

	jit_prepare(jit);
	jit_putargr(jit, R(0));
	jit_putargi(jit, 0);
	jit_putargr(jit, R(1));
	jit_calli(memset);
}
