#include <bits/stdint-uintn.h>
#include <string.h>
#include <jit/jit.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/conversion.h"
#include "../include/util.h"
#include "jit/jit-insn.h"
#include "jit/jit-value.h"

int ptrs_astlist_length(struct ptrs_astlist *curr)
{
	int len = 0;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	return len;
}

void ptrs_fillArray(void *array, size_t len, ptrs_val_t val, ptrs_meta_t meta, ptrs_nativetype_info_t *type)
{
	if(type->varType == (uint8_t)-1)
	{
		ptrs_var_t *vararray = array;
		for(int i = 0; i < len; i++)
		{
			vararray[i].value = val;
			vararray[i].meta = meta;
		}
	}
	else
	{
		ptrs_var_t filler;
		filler.value = val;
		filler.meta = meta;
		for(int i = 0; i < len; i++)
		{
			type->setHandler(array + i * type->size, type->size, &filler);
		}
	}
}
void ptrs_astlist_handle(ptrs_ast_t *node, struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t val, jit_value_t size, ptrs_nativetype_info_t *type)
{
	int i = 0;
	jit_value_t zero = jit_const_long(func, ulong, 0);
	ptrs_jit_var_t result = {zero, zero, PTRS_TYPE_UNDEFINED};
	jit_value_t convertedResult = zero;

	jit_value_t initializerLength = jit_const_long(func, ulong, ptrs_astlist_length(list));
	ptrs_jit_assert(node, func, scope, jit_insn_le(func, initializerLength, size),
		2, "Array initializer of length %d is larger than array size of length %d", initializerLength, size);

	if(list == NULL)
	{
		result.val = zero;
		result.meta = zero;
		result.constType = PTRS_TYPE_UNDEFINED;
	}
	else if(list->next == NULL)
	{
		result = list->entry->vtable->get(list->entry, func, scope);
	}
	else
	{
		for(; list != NULL; i++)
		{
			if(list->entry == NULL)
			{
				result.val = zero;
				result.meta = zero;
			}
			else
			{
				result = list->entry->vtable->get(list->entry, func, scope);
			}

			if(type->varType == PTRS_TYPE_FLOAT)
			{
				if(result.constType == PTRS_TYPE_INT && jit_value_is_constant(result.val))
				{
					// allow int constants to act as initializers for float arrays
					convertedResult = jit_insn_convert(func, result.val, type->jitType, 0);
				}
				else
				{
					ptrs_jit_typeCheck(list->entry, func, scope, result, PTRS_TYPE_FLOAT, "Initializer of float array needs to be of type float not %t");
					convertedResult = ptrs_jit_reinterpretCast(func, result.val, jit_type_float64);
					convertedResult = jit_insn_convert(func, convertedResult, type->jitType, 0);
				}
			}
			else if(type->varType == PTRS_TYPE_INT)
			{
				ptrs_jit_typeCheck(list->entry, func, scope, result, PTRS_TYPE_INT, "Initializer of int array needs to be of type int not %t");
				convertedResult = jit_insn_convert(func, result.val, type->jitType, 0);
			}
			else if(type->varType == PTRS_TYPE_POINTER)
			{
				ptrs_jit_typeCheck(list->entry, func, scope, result, PTRS_TYPE_POINTER, "Initializer of pointer array needs to be of type pointer not %t");
				convertedResult = jit_insn_convert(func, result.val, type->jitType, 0);
			}
			else if(type->varType == (uint8_t)-1)
			{
				// no conversion
			}
			else
			{
				ptrs_error(list->entry, "Unknown list type, initialization is not implemented.");
			}

			if(type->varType == (uint8_t)-1)
			{
				jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t), result.val);
				jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t) + sizeof(ptrs_val_t), result.meta);
			}
			else
			{
				jit_insn_store_relative(func, val, i * type->size, convertedResult);
			}
			list = list->next;
		}
	}

	if(jit_value_is_constant(result.val) && jit_value_is_constant(result.meta)
		&& !jit_value_is_true(result.val) && !jit_value_is_true(result.meta))
	{
		val = jit_insn_add(func, val, jit_const_int(func, nuint, i * type->size));
		size = jit_insn_sub(func, size, jit_const_int(func, nuint, i)); //size = size - i
		size = jit_insn_mul(func, size, jit_const_int(func, long, type->size)); //size = size * type->size
		jit_insn_memset(func, val, zero, size);
	}
	else if(type->size == 1)
	{
		val = jit_insn_add(func, val, jit_const_int(func, nuint, i));
		size = jit_insn_sub(func, size, jit_const_int(func, nuint, i));
		jit_insn_memset(func, val, convertedResult, size);
	}
	else if(jit_value_is_constant(size) && jit_value_get_nint_constant(size) <= 8)
	{
		size_t len = jit_value_get_nint_constant(size);
		for(; i < len; i++)
		{
			if(type->varType == (uint8_t)-1)
			{
				jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t), result.val);
				jit_insn_store_relative(func, val, i * sizeof(ptrs_var_t) + sizeof(ptrs_val_t), result.meta);
			}
			else
			{
				jit_insn_store_relative(func, val, i * type->size, convertedResult);
			}
		}
	}
	else
	{
		ptrs_jit_reusableCallVoid(func, ptrs_fillArray, (
				jit_type_void_ptr,
				jit_type_nuint,
				jit_type_long,
				jit_type_ulong,
				jit_type_void_ptr
			), (
				jit_insn_add(func, val, jit_const_int(func, nuint, i * 16)),
				jit_insn_sub(func, size, jit_const_int(func, nuint, i)),
				result.val,
				result.meta,
				jit_const_int(func, void_ptr, (uintptr_t)type)
			)
		);
	}
}
