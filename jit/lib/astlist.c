#include <string.h>

#include "../include/error.h"
#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

void ptrs_astlist_handle(struct ptrs_astlist *list, unsigned fpOffset, jit_state_t *jit, ptrs_scope_t *scope)
{
	const ptrs_meta_t defaultMeta = {.type = PTRS_TYPE_UNDEFINED};

	int i;
	for(i = 0; list != NULL; i += 16)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			jit_movi(jit, R(0), 0);
			jit_movi(jit, R(1), *(uintptr_t *)&defaultMeta);
		}
		else
		{
			list->entry->handler(list->entry, jit, scope);
		}

		ptrs_jit_load_val(R(2), fpOffset);
		jit_stxi(jit, R(2), i, R(0), 8);
		jit_stxi(jit, R(2), i + 8, R(1), 8);

		list = list->next;
	}

	//R(0) = array.ptr
	//R(1) = array.size
	//R(2) = i
	//R(3) = default.value
	//R(4) = default.meta
	ptrs_jit_load_val(R(0), fpOffset);
	ptrs_jit_load_arraysize(R(1), fpOffset);
	jit_movi(jit, R(2), i);
	jit_movi(jit, R(3), 0);
	jit_movi(jit, R(4), *(uintptr_t *)&defaultMeta);

	jit_label *check = jit_get_label(jit);
	jit_op *done = jit_bger_u(jit, JIT_FORWARD, R(2), R(1));

	jit_stxr(jit, R(0), R(2), R(3), 8);
	jit_addi(jit, R(2), R(2), 8);
	jit_stxr(jit, R(0), R(2), R(4), 8);
	jit_addi(jit, R(2), R(2), 8);
	jit_jmpi(jit, check);
	jit_patch(jit, done);
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
			list->entry->handler(list->entry, jit, scope);
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
