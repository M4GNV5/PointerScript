#include <string.h>

#include "../include/error.h"
#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

void ptrs_astlist_handle(struct ptrs_astlist *list, long valReg, long sizeReg, jit_state_t *jit, ptrs_scope_t *scope)
{
	int i;
	unsigned result;

	for(i = 0; list != NULL; i += 16)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			result = scope->usedRegCount;
			jit_movi(jit, R(result), 0);
			jit_movi(jit, R(result + 1), 0);
		}
		else
		{
			result = list->entry->handler(list->entry, jit, scope);
		}

		jit_stxi(jit, i, valReg, R(result), sizeof(ptrs_val_t));
		jit_stxi(jit, i + 8, valReg, R(result + 1), sizeof(ptrs_meta_t));

		list = list->next;
	}

	assert(result == scope->usedRegCount);
	scope->usedRegCount += 2;

	long iReg = R(scope->usedRegCount++);
	jit_movi(jit, iReg, i);

	jit_label *check = jit_get_label(jit);
	jit_op *done = jit_bger_u(jit, JIT_FORWARD, iReg, sizeReg); //while(i < array.size)

	jit_stxr(jit, valReg, iReg, R(result), 8); //array[i] = default.value
	jit_addi(jit, iReg, iReg, 8);         //i += 8
	jit_stxr(jit, valReg, iReg, R(result + 1), 8); //array[i] = default.meta
	jit_addi(jit, iReg, iReg, 8);         //i += 8

	jit_jmpi(jit, check);
	jit_patch(jit, done);

	scope->usedRegCount -= 3;
}

void ptrs_astlist_handleByte(struct ptrs_astlist *list, long valReg, long sizeReg, jit_state_t *jit, ptrs_scope_t *scope)
{
	int i;
	unsigned result;

	for(i = 0; list != NULL; i++)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			result = scope->usedRegCount;
			jit_movi(jit, R(result), 0);
		}
		else
		{
			result = list->entry->handler(list->entry, jit, scope);
			ptrs_jit_convert(jit, ptrs_vartoi, R(result), R(result), R(result + 1));
		}

		jit_stxi(jit, i, valReg, R(result), 1);

		list = list->next;
	}

	long tmp = R(scope->usedRegCount);
	jit_subi(jit, tmp, sizeReg, i);

	jit_prepare(jit);
	jit_putargr(jit, valReg);
	jit_putargr(jit, R(result));
	jit_putargr(jit, tmp);
	jit_call(jit, memset);
}
