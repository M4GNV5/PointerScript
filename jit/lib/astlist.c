#include <string.h>
#include <jit/jit.h>

#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

void ptrs_astlist_handle(struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope, jit_value_t val, jit_value_t size)
{
	int i;
	jit_value_t zero = jit_const_long(func, ulong, 0);
	ptrs_jit_var_t result;

	for(i = 0; list != NULL; i++)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			result.val = zero;
			result.meta = zero;
		}
		else
		{
			result = list->entry->handler(list->entry, func, scope);
		}

		jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t), result.val);
		jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t) + sizeof(ptrs_val_t), result.meta);

		list = list->next;
	}

	jit_value_t index = jit_value_create(func, jit_type_nuint);
	jit_insn_store(func, index, jit_const_int(func, nuint, i * 2));

	jit_value_t endIndex = jit_insn_shl(func, size, jit_const_int(func, nuint, 1)); //mul 2

	jit_label_t check = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	jit_insn_label(func, &check);
	jit_insn_branch_if_not(func, jit_insn_lt(func, index, endIndex), &done); //while(i < array.size)

	jit_insn_store_elem(func, val, index, result.val); //array[index] = lastElement.value
	jit_value_t _index = jit_insn_add(func, index, jit_const_int(func, nuint, 1)); //i++
	jit_insn_store_elem(func, val, _index, result.meta); //array[index] = lastElement.meta
	_index = jit_insn_add(func, _index, jit_const_int(func, nuint, 1)); //i++

	jit_insn_store(func, index, _index);
	jit_insn_branch(func, &check);
	jit_insn_label(func, &done);
}

void ptrs_astlist_handleByte(struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope, jit_value_t val, jit_value_t size)
{
	int i;
	jit_value_t zero = jit_const_long(func, ubyte, 0);
	jit_value_t result;

	for(i = 0; list != NULL; i++)
	{
		//if(list->expand) //TODO

		if(list->entry == NULL)
		{
			result = zero;
		}
		else
		{
			ptrs_jit_var_t _result = list->entry->handler(list->entry, func, scope);
			result = ptrs_jit_vartoi(func, _result.val, _result.meta);
		}

		jit_insn_store_relative(func, val, i, result);
		list = list->next;
	}

	jit_value_t index = jit_const_int(func, nuint, i);
	jit_insn_memset(func, jit_insn_add(func, val, index), zero, jit_insn_sub(func, size, index));
}
