#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <alloca.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include <jit/jit.h>

#include "../../parser/common.h"
#include "../include/conversion.h"
#include "../include/util.h"

void ptrs_jit_branch_if(jit_function_t func, jit_label_t *target, ptrs_jit_var_t val)
{
	bool placeIsUndefined = true;
	bool placeIsZero = true;

	if(val.constType != -1)
	{
		placeIsUndefined = false;

		if(val.constType == PTRS_TYPE_UNDEFINED)
			return;
	}
	if(jit_value_is_constant(val.val))
	{
		placeIsZero = false;

		if(ptrs_jit_value_getValConstant(val.val).intval == 0)
			return;
	}

	if(!placeIsUndefined && !placeIsZero)
	{
		jit_insn_branch(func, target);
		return;
	}

	if(placeIsUndefined)
	{
		jit_value_t isUndefined = ptrs_jit_hasType(func, val.meta, PTRS_TYPE_UNDEFINED);
		jit_insn_branch_if_not(func, isUndefined, target);
	}

	if(placeIsZero)
	{
		jit_value_t isZero = jit_insn_to_bool(func, val.val);
		jit_insn_branch_if(func, isZero, target);
	}
}

void ptrs_jit_branch_if_not(jit_function_t func, jit_label_t *target, ptrs_jit_var_t val)
{
	bool placeIsUndefined = true;
	bool placeIsZero = true;

	if(val.constType != -1)
	{
		placeIsUndefined = false;

		if(val.constType == PTRS_TYPE_UNDEFINED)
		{
			jit_insn_branch(func, target);
			return;
		}
	}
	if(jit_value_is_constant(val.val))
	{
		placeIsZero = false;

		if(ptrs_jit_value_getValConstant(val.val).intval == 0)
		{
			jit_insn_branch(func, target);
			return;
		}
	}

	if(placeIsUndefined)
	{
		jit_value_t isUndefined = ptrs_jit_hasType(func, val.meta, PTRS_TYPE_UNDEFINED);
		jit_insn_branch_if(func, isUndefined, target);
	}

	if(placeIsZero)
	{
		jit_value_t isZero = jit_insn_to_not_bool(func, val.val);
		jit_insn_branch_if(func, isZero, target);
	}
}

jit_value_t ptrs_jit_vartob(jit_function_t func, ptrs_jit_var_t val)
{
	if(val.constType != -1 && jit_value_is_constant(val.val))
	{
		if(val.constType == PTRS_TYPE_UNDEFINED
			|| ptrs_jit_value_getValConstant(val.val).intval == 0)
			return jit_const_long(func, long, 0);
		else
			return jit_const_long(func, long, 1);
	}
	else if(val.constType != -1)
	{
		if(val.constType == PTRS_TYPE_UNDEFINED)
			return jit_const_long(func, long, 0);
		else
			return jit_insn_to_bool(func, val.val);
	}
	else if(jit_value_is_constant(val.val))
	{
		if(ptrs_jit_value_getValConstant(val.val).intval == 0)
			return jit_const_long(func, long, 0);
		else
			return ptrs_jit_doesntHaveType(func, val.meta, PTRS_TYPE_UNDEFINED);
	}
	else
	{
		jit_value_t definedType = ptrs_jit_doesntHaveType(func, val.meta, PTRS_TYPE_UNDEFINED);
		jit_value_t isTrue = jit_insn_to_bool(func, val.val);
		return jit_insn_and(func, definedType, isTrue);
	}
}

jit_value_t ptrs_jit_vartoi(jit_function_t func, ptrs_jit_var_t val)
{
	switch(val.constType)
	{
		case -1:
			break; //use intrinsic
		case PTRS_TYPE_UNDEFINED:
			return jit_const_long(func, long, 0);
		case PTRS_TYPE_INT:
			return val.val;
		case PTRS_TYPE_FLOAT:
			return jit_insn_convert(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64), jit_type_long, 0);
	}

	jit_intrinsic_descr_t descr = {
		.return_type = jit_type_long,
		.ptr_result_type = NULL,
		.arg1_type = jit_type_long,
		.arg2_type = jit_type_ulong
	};

	return jit_insn_call_intrinsic(func, NULL, ptrs_vartoi, &descr, val.val, val.meta);
}

jit_value_t ptrs_jit_vartof(jit_function_t func, ptrs_jit_var_t val)
{
	switch(val.constType)
	{
		case -1:
			break; //use instrinsic
		case PTRS_TYPE_UNDEFINED:
			return jit_const_long(func, long, 0);
		case PTRS_TYPE_INT:
			return jit_insn_convert(func, val.val, jit_type_float64, 0);
		case PTRS_TYPE_FLOAT:
			return ptrs_jit_reinterpretCast(func, val.val, jit_type_float64);
	}

	jit_intrinsic_descr_t descr = {
		.return_type = jit_type_float64,
		.ptr_result_type = NULL,
		.arg1_type = jit_type_long,
		.arg2_type = jit_type_ulong
	};

	return jit_insn_call_intrinsic(func, NULL, ptrs_vartof, &descr, val.val, val.meta);
}

void ptrs_itoa(char *buff, ptrs_val_t val)
{
	sprintf(buff, "%"PRId64, val.intval);
}
void ptrs_ftona(char *buff, int maxlen, ptrs_val_t val)
{
	snprintf(buff, maxlen, "%.8f", val.floatval);

	int i = 0;
	while(buff[i] != '.')
		i++;

	int last = i;
	i++;

	while(buff[i] != 0)
	{
		if(buff[i] != '0')
			last = i + 1;
		i++;
	}
	buff[last] = 0;
}
void ptrs_ftoa(char *buff, ptrs_val_t val)
{
	ptrs_ftona(buff, 32, val);
}
void ptrs_ptoa(char *buff, ptrs_val_t val)
{
	sprintf(buff, "pointer:%p", val.ptrval);
}
void ptrs_stoa(char *buff, ptrs_val_t val)
{
	sprintf(buff, "%s:%p", val.structval->name, val.structval);
}
ptrs_jit_var_t ptrs_jit_vartoa(jit_function_t func, ptrs_jit_var_t val)
{
	jit_value_t buff;

	if(val.constType == -1)
	{
		buff = jit_value_create(func, jit_type_void_ptr);
		jit_value_t size = jit_value_create(func, jit_type_ulong);

		jit_label_t genericConversion = jit_label_undefined;
		jit_label_t done = jit_label_undefined;

		jit_value_t isNative = ptrs_jit_hasType(func, val.meta, PTRS_TYPE_NATIVE);
		jit_insn_branch_if_not(func, isNative, &genericConversion);

		jit_value_t _size = ptrs_jit_getArraySize(func, val.meta);
		jit_value_t zeroLength = jit_insn_eq(func, _size, jit_const_long(func, ulong, 0));
		jit_insn_branch_if(func, zeroLength, &genericConversion);

		//sized variable of type native (i.e. already a (sized) string)
		jit_insn_store(func, size, jit_insn_add(func, _size, jit_const_int(func, nuint, 1)));
		jit_insn_store(func, buff, jit_insn_alloca(func, size));

		jit_insn_memcpy(func, buff, val.val, _size);
		jit_insn_store_elem(func, buff, _size, jit_const_int(func, ubyte, 0));

		jit_insn_branch(func, &done);

		//other type that needs to be converted
		jit_insn_label(func, &genericConversion);
		jit_insn_store(func, size, jit_const_int(func, nuint, 32));
		jit_insn_store(func, buff, jit_insn_alloca(func, size));

		jit_type_t paramDef[] = {
			jit_type_long, //ptrs_val_t
			jit_type_ulong, //ptrs_meta_t
			jit_type_void_ptr,
			jit_type_ulong
		};

		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, paramDef, 4, 1);
		jit_value_t params[] = {val.val, val.meta, buff, size};

		jit_insn_call_native(func, NULL, ptrs_vartoa, signature, params, 4, JIT_CALL_NOTHROW);
		jit_type_free(signature);

		jit_insn_label(func, &done);

		ptrs_jit_var_t ret;
		ret.val = buff;
		ret.meta = ptrs_jit_arrayMeta(func,
			jit_const_long(func, ulong, PTRS_TYPE_NATIVE),
			jit_const_long(func, ulong, 0),
			size);
		ret.constType = PTRS_TYPE_NATIVE;
		return ret;
	}
	else
	{
		if(val.constType == PTRS_TYPE_NATIVE)
		{
			jit_value_t size = ptrs_jit_getArraySize(func, val.meta);

			jit_value_t buffSize = jit_insn_add(func, size, jit_const_int(func, nint, 1));
			val.meta = ptrs_jit_arrayMeta(func,
				jit_const_int(func, ulong, PTRS_TYPE_NATIVE),
				jit_const_int(func, ulong, 0),
				buffSize
			);

			if(jit_value_is_constant(size))
				buff = jit_insn_array(func, jit_value_get_long_constant(buffSize));
			else
				buff = jit_insn_alloca(func, buffSize);

			jit_insn_memcpy(func, buff, val.val, size);
			jit_insn_store_elem(func, buff, size, jit_const_int(func, ubyte, 0));

			val.constType = PTRS_TYPE_NATIVE;
			val.val = buff;
			return val;
		}

		buff = jit_insn_array(func, 32);
		ptrs_jit_var_t ret;
		ret.constType = PTRS_TYPE_NATIVE;
		ret.val = buff;
		ret.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, 0, 32);

		if(val.constType == PTRS_TYPE_UNDEFINED)
		{
			jit_insn_memcpy(func, buff, jit_const_int(func, void_ptr, (uintptr_t)"undefined"),
				jit_const_int(func, nuint, strlen("undefined")));
			return ret;
		}

		static jit_type_t conversionSignature = NULL;
		if(conversionSignature == NULL)
		{
			jit_type_t argDef[] = {
				jit_type_void_ptr,
				jit_type_long,
			};
			conversionSignature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, argDef, 2, 0);
		}

		void *converters[] = {
			[PTRS_TYPE_INT] = ptrs_itoa,
			[PTRS_TYPE_FLOAT] = ptrs_ftoa,
			[PTRS_TYPE_POINTER] = ptrs_ptoa,
			[PTRS_TYPE_STRUCT] = ptrs_stoa,
		};

		jit_value_t args[] = {buff, val.val};
		jit_insn_call_native(func, "ptrs_vartoa", converters[val.constType], conversionSignature, args, 2, 0);
		return ret;
	}
}

bool ptrs_vartob(ptrs_val_t val, ptrs_meta_t meta)
{
	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			return false;
		case PTRS_TYPE_INT:
			return val.intval != 0;
		case PTRS_TYPE_FLOAT:
			return val.floatval != 0;
		default: //pointer type
			return val.strval != NULL;
	}
}

int64_t strntol(const char *str, uint32_t len)
{
	char *str2;
	if(strnlen(str, len) == len)
	{
		str2 = alloca(len) + 1;
		strncpy(str2, str, len);
		str2[len] = 0;
	}
	else
	{
		str2 = (char *)str;
	}

	return strtoimax(str2, NULL, 0);
}
int64_t ptrs_vartoi(ptrs_val_t val, ptrs_meta_t meta)
{
	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			return 0;
		case PTRS_TYPE_INT:
			return val.intval;
		case PTRS_TYPE_FLOAT:
			return val.floatval;
		case PTRS_TYPE_NATIVE:
			if(meta.array.size > 0)
				return strntol(val.strval, meta.array.size);
		default: //pointer type
			return (intptr_t)val.nativeval;
	}
}

double strntod(const char *str, uint32_t len)
{
	char *str2;
	if(strnlen(str, len) == len)
	{
		str2 = alloca(len) + 1;
		strncpy(str2, str, len);
		str2[len] = 0;
	}
	else
	{
		str2 = (char *)str;
	}

	return atof(str2);
}
double ptrs_vartof(ptrs_val_t val, ptrs_meta_t meta)
{
	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			return NAN;
		case PTRS_TYPE_INT:
			return val.intval;
		case PTRS_TYPE_FLOAT:
			return val.floatval;
		case PTRS_TYPE_NATIVE:
			if(meta.array.size > 0)
				return strntod(val.strval, meta.array.size);
		default: //pointer type
			return (intptr_t)val.nativeval;
	}
}

const char *ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen)
{
	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			strcpy(buff, "undefined");
			break;
		case PTRS_TYPE_INT:
			snprintf(buff, maxlen, "%"PRId64, val.intval);
			break;
		case PTRS_TYPE_FLOAT:
			ptrs_ftona(buff, maxlen, val);
			break;
		case PTRS_TYPE_NATIVE:
			;
			if(meta.array.size == 0)
			{
				snprintf(buff, maxlen, "native:%p", val.strval);
				break;
			}

			int len = strnlen(val.strval, meta.array.size);
			if(len < meta.array.size)
				return val.strval;
			else
				snprintf(buff, maxlen, "%.*s", len, val.strval); //wat do when maxlen < len? :(
			break;
		case PTRS_TYPE_POINTER:
			snprintf(buff, maxlen, "pointer:%p", val.ptrval);
			break;
		case PTRS_TYPE_STRUCT:
			snprintf(buff, maxlen, "%s:%p", val.structval->name, val.structval);
			break;
	}
	buff[maxlen - 1] = 0;
	return buff;
}

const char * const ptrs_typeStrings[] = {
	[PTRS_TYPE_UNDEFINED] = "undefined",
	[PTRS_TYPE_INT] = "int",
	[PTRS_TYPE_FLOAT] = "float",
	[PTRS_TYPE_NATIVE] = "native",
	[PTRS_TYPE_POINTER] = "pointer",
	[PTRS_TYPE_STRUCT] = "struct",
};
const char *ptrs_typetoa(ptrs_vartype_t type)
{
	return ptrs_typeStrings[type];
}
