#include <string.h>
#include <jit/jit.h>

#include "../include/conversion.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"

void ptrs_initArray(ptrs_var_t *array, size_t len, ptrs_val_t val, ptrs_meta_t meta)
{
	for(int i = 0; i < len; i++)
	{
		array[i].value = val;
		array[i].meta = meta;
	}
}
void ptrs_astlist_handle(struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope, jit_value_t val, jit_value_t size)
{
	int i;
	jit_value_t zero = jit_const_long(func, ulong, 0);
	ptrs_jit_var_t result = {zero, zero, PTRS_TYPE_UNDEFINED};

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

	if(jit_value_is_constant(result.val) && jit_value_is_constant(result.meta)
		&& !jit_value_is_true(result.val) && !jit_value_is_true(result.meta))
	{
		val = jit_insn_add(func, val, jit_const_int(func, nuint, i * 16));
		size = jit_insn_sub(func, size, jit_const_int(func, nuint, i)); //size = size - i
		size = jit_insn_shl(func, size, jit_const_int(func, long, 4)); //size = size * 16
		jit_insn_memset(func, val, zero, size);
	}
	else if(jit_value_is_constant(size) && jit_value_get_nint_constant(size) < 16)
	{
		size_t len = jit_value_get_nint_constant(size);
		for(; i < len; i++)
		{
			jit_insn_store_relative(func, val, i * 16, result.val);
			jit_insn_store_relative(func, val, i * 16 + 8, result.meta);
		}
	}
	else
	{
		static jit_type_t signature = NULL;
		if(signature == NULL)
		{
			jit_type_t argDef[] = {
				jit_type_void_ptr,
				jit_type_nuint,
				jit_type_long,
				jit_type_ulong
			};

			signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, argDef, 4, 0);
		}

		jit_value_t args[] = {
			jit_insn_add(func, val, jit_const_int(func, nuint, i * 16)),
			jit_insn_sub(func, size, jit_const_int(func, nuint, i)),
			result.val,
			result.meta
		};
		jit_insn_call_native(func, "ptrs_initArray", ptrs_initArray, signature, args, 4, 0);
	}
}

void ptrs_astlist_handleByte(struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope, jit_value_t val, jit_value_t size)
{
	int i = 0;
	jit_value_t zero = jit_const_long(func, ubyte, 0);
	jit_value_t result;

	if(list == NULL)
	{
		result = zero;
	}
	else if(list->next == NULL)
	{
		ptrs_jit_var_t _result = list->entry->handler(list->entry, func, scope);
		result = ptrs_jit_vartoi(func, _result);
	}
	else
	{
		for(; list != NULL; i++)
		{
			//if(list->expand) //TODO

			if(list->entry == NULL)
			{
				result = zero;
			}
			else
			{
				ptrs_jit_var_t _result = list->entry->handler(list->entry, func, scope);
				result = ptrs_jit_vartoi(func, _result);
			}

			jit_insn_store_relative(func, val, i, result);
			list = list->next;
		}

		jit_value_t index = jit_const_int(func, nuint, i);
		size = jit_insn_sub(func, size, index);
		val = jit_insn_add(func, val, index);
	}

	jit_insn_memset(func, val, result, size);
}
