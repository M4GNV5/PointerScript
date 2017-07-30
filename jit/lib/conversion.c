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

void ptrs_jit_branch_if(jit_function_t func, jit_label_t *target, jit_value_t val, jit_value_t meta)
{
	bool placeIsUndefined = true;
	bool placeIsZero = true;

	if(jit_value_is_constant(meta))
	{
		placeIsUndefined = false;

		if(ptrs_jit_value_getMetaConstant(meta).type == PTRS_TYPE_UNDEFINED)
			return;
	}
	if(jit_value_is_constant(val))
	{
		placeIsZero = false;

		if(ptrs_jit_value_getValConstant(val).intval == 0)
			return;
	}

	if(!placeIsUndefined && !placeIsZero)
	{
		jit_insn_branch(func, target);
		return;
	}

	if(placeIsUndefined)
	{
		jit_value_t isUndefined = ptrs_jit_hasType(func, meta, PTRS_TYPE_UNDEFINED);
		jit_insn_branch_if_not(func, isUndefined, target);
	}

	if(placeIsZero)
	{
		jit_value_t isZero = jit_insn_to_bool(func, val);
		jit_insn_branch_if(func, isZero, target);
	}
}

void ptrs_jit_branch_if_not(jit_function_t func, jit_label_t *target, jit_value_t val, jit_value_t meta)
{
	bool placeIsUndefined = true;
	bool placeIsZero = true;

	if(jit_value_is_constant(meta))
	{
		placeIsUndefined = false;

		if(ptrs_jit_value_getMetaConstant(meta).type == PTRS_TYPE_UNDEFINED)
		{
			jit_insn_branch(func, target);
			return;
		}
	}
	if(jit_value_is_constant(val))
	{
		placeIsZero = false;

		if(ptrs_jit_value_getValConstant(val).intval == 0)
		{
			jit_insn_branch(func, target);
			return;
		}
	}

	if(placeIsUndefined)
	{
		jit_value_t isUndefined = ptrs_jit_hasType(func, meta, PTRS_TYPE_UNDEFINED);
		jit_insn_branch_if(func, isUndefined, target);
	}

	if(placeIsZero)
	{
		jit_value_t isZero = jit_insn_to_not_bool(func, val);
		jit_insn_branch_if(func, isZero, target);
	}
}

jit_value_t ptrs_jit_vartob(jit_function_t func, jit_value_t val, jit_value_t meta)
{
	if(jit_value_is_constant(meta) && jit_value_is_constant(val))
	{
		if(ptrs_jit_value_getMetaConstant(meta).type == PTRS_TYPE_UNDEFINED
			|| ptrs_jit_value_getValConstant(val).intval == 0)
			return jit_const_long(func, long, 0);
		else
			return jit_const_long(func, long, 1);
	}
	else if(jit_value_is_constant(meta))
	{
		if(ptrs_jit_value_getMetaConstant(meta).type == PTRS_TYPE_UNDEFINED)
			return jit_const_long(func, long, 0);
		else
			return jit_insn_to_bool(func, val);
	}
	else if(jit_value_is_constant(val))
	{
		if(ptrs_jit_value_getValConstant(val).intval == 0)
			return jit_const_long(func, long, 0);
		else
			return ptrs_jit_doesntHaveType(func, meta, PTRS_TYPE_UNDEFINED);
	}
	else
	{
		jit_value_t definedType = ptrs_jit_doesntHaveType(func, meta, PTRS_TYPE_UNDEFINED);
		jit_value_t isTrue = jit_insn_to_bool(func, val);
		return jit_insn_and(func, definedType, isTrue);
	}
}

jit_value_t ptrs_jit_vartoi(jit_function_t func, jit_value_t val, jit_value_t meta)
{
	if(jit_value_is_constant(meta))
	{
		ptrs_meta_t constMeta = ptrs_jit_value_getMetaConstant(meta);
		switch(constMeta.type)
		{
			case PTRS_TYPE_UNDEFINED:
				return jit_const_long(func, long, 0);
			case PTRS_TYPE_FLOAT:
				return jit_insn_convert(func, ptrs_jit_reinterpretCast(func, val, jit_type_float64), jit_type_long, 0);
			case PTRS_TYPE_NATIVE:
				if(constMeta.array.size > 0)
					break; //use default conversion
				//fallthrough
			default:
				return val;
		}
	}

	jit_intrinsic_descr_t descr = {
		.return_type = jit_type_long,
		.ptr_result_type = NULL,
		.arg1_type = jit_type_long,
		.arg2_type = jit_type_ulong
	};

	return jit_insn_call_intrinsic(func, NULL, ptrs_vartoi, &descr, val, meta);
}

jit_value_t ptrs_jit_vartof(jit_function_t func, jit_value_t val, jit_value_t meta)
{
	if(jit_value_is_constant(meta))
	{
		ptrs_meta_t constMeta = ptrs_jit_value_getMetaConstant(meta);
		switch(constMeta.type)
		{
			case PTRS_TYPE_UNDEFINED:
				return jit_const_long(func, long, 0);
			case PTRS_TYPE_FLOAT:
				return val;
			case PTRS_TYPE_NATIVE:
				if(constMeta.array.size > 0)
					break; //use default conversion
				//fallthrough
			default:
				return jit_insn_convert(func, val, jit_type_float64, 0);
		}
	}

	jit_intrinsic_descr_t descr = {
		.return_type = jit_type_float64,
		.ptr_result_type = NULL,
		.arg1_type = jit_type_long,
		.arg2_type = jit_type_ulong
	};

	return jit_insn_call_intrinsic(func, NULL, ptrs_vartof, &descr, val, meta);
}

ptrs_jit_var_t ptrs_jit_vartoa(jit_function_t func, jit_value_t val, jit_value_t meta)
{
	jit_value_t buff = jit_value_create(func, jit_type_void_ptr);
	jit_value_t size = jit_value_create(func, jit_type_ulong);

	jit_label_t genericConversion = jit_label_undefined;
	jit_label_t done = jit_label_undefined;

	jit_value_t isNative = ptrs_jit_hasType(func, meta, PTRS_TYPE_NATIVE);
	jit_insn_branch_if_not(func, isNative, &genericConversion);

	jit_value_t _size = ptrs_jit_getArraySize(func, meta);
	jit_value_t zeroLength = jit_insn_eq(func, _size, jit_const_long(func, ulong, 0));
	jit_insn_branch_if(func, zeroLength, &genericConversion);

	//sized variable of type native (i.e. already a (sized) string)
	jit_insn_store(func, size, jit_insn_add(func, _size, jit_const_int(func, nuint, 1)));
	jit_insn_store(func, buff, jit_insn_alloca(func, size));

	jit_insn_memcpy(func, buff, val, _size);
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
	jit_value_t params[] = {val, meta, buff, size};

	jit_insn_call_native(func, NULL, ptrs_vartoa, signature, params, 4, JIT_CALL_NOTHROW);
	jit_type_free(signature);

	jit_insn_label(func, &done);

	ptrs_jit_var_t ret;
	ret.val = buff;
	ret.meta = ptrs_jit_arrayMeta(func,
		jit_const_long(func, ulong, PTRS_TYPE_NATIVE),
		jit_const_long(func, ulong, 0),
		size);
	return ret;
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
