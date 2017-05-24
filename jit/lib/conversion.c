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

jit_value_t ptrs_jit_vartob(jit_function_t func, jit_value_t val, jit_value_t meta)
{
	jit_value_t tmp1 = ptrs_jit_get_type(func, meta);
	tmp1 = jit_insn_eq(func, tmp1, ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED));
	jit_value_t tmp2 = jit_insn_eq(func, val, ptrs_jit_const_val(func, 0));
	return jit_insn_and(func, tmp1, tmp2);
}

jit_value_t ptrs_jit_vartoi(jit_function_t func, jit_value_t val, jit_value_t meta)
{
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
	jit_intrinsic_descr_t descr = {
		.return_type = jit_type_float64,
		.ptr_result_type = NULL,
		.arg1_type = jit_type_long,
		.arg2_type = jit_type_ulong
	};

	return jit_insn_call_intrinsic(func, NULL, ptrs_vartof, &descr, val, meta);
}

jit_value_t ptrs_jit_vartoa(jit_function_t func, jit_value_t val, jit_value_t meta, jit_value_t buff, jit_value_t maxlen)
{
	jit_type_t paramDef[] = {
		jit_type_long, //ptrs_val_t
		jit_type_ulong, //ptrs_meta_t
		jit_type_void_ptr,
		jit_type_ulong
	};

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, paramDef, 4, 0);
	jit_value_t params[] = {val, meta, buff, maxlen};

	jit_value_t ret = jit_insn_call_native(func, NULL, ptrs_vartoa, signature, params, 4, JIT_CALL_NOTHROW);
	jit_type_free(signature);
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
			return "undefined";
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
		case PTRS_TYPE_FUNCTION:
			snprintf(buff, maxlen, "function:%p", val.funcval);
			break;
		case PTRS_TYPE_STRUCT:
			snprintf(buff, maxlen, "%s:%p", val.structval->name, val.structval);
			break;
	}
	buff[maxlen - 1] = 0;
	return buff;
}

void ptrs_typetoa(const char **result, ptrs_vartype_t type)
{
	switch(type)
	{
		case PTRS_TYPE_UNDEFINED:
			*result = "undefined";
			break;
		case PTRS_TYPE_INT:
			*result = "int";
			break;
		case PTRS_TYPE_FLOAT:
			*result = "float";
			break;
		case PTRS_TYPE_NATIVE:
			*result = "native";
			break;
		case PTRS_TYPE_POINTER:
			*result = "pointer";
			break;
		case PTRS_TYPE_FUNCTION:
			*result = "function";
			break;
		case PTRS_TYPE_STRUCT:
			*result = "struct";
			break;
		default:
			*result = "unknown";
			break;
	}
}
