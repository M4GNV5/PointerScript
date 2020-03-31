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
#include "../include/struct.h"
#include "../jit.h"

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
	if(jit_value_is_constant(val.val) && jit_value_is_constant(val.meta))
	{
		ptrs_val_t constVal = ptrs_jit_value_getValConstant(val.val);
		ptrs_meta_t constMeta = ptrs_jit_value_getMetaConstant(val.meta);

		if(constMeta.type != PTRS_TYPE_STRUCT)
			return jit_const_long(func, long, ptrs_vartoi(constVal, constMeta));
	}

	switch(val.constType)
	{
		case -1:
		case PTRS_TYPE_NATIVE:
			break; //use intrinsic
		case PTRS_TYPE_STRUCT:
			if(jit_value_is_constant(val.meta))
			{
				ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
				ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
				if(ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, true) == NULL)
					return val.val;
			}
			break; //use intrinsic
		case PTRS_TYPE_UNDEFINED:
			return jit_const_long(func, long, 0);
		case PTRS_TYPE_FLOAT:
			return jit_insn_convert(func, ptrs_jit_reinterpretCast(func, val.val, jit_type_float64), jit_type_long, 0);
		default:
			return val.val;
	}

	jit_value_t ret;
	val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
	ptrs_jit_reusableCall(func, ptrs_vartoi, ret, jit_type_long,
		(jit_type_long, jit_type_ulong), (val.val, val.meta));

	return ret;
}

jit_value_t ptrs_jit_vartof(jit_function_t func, ptrs_jit_var_t val)
{
	if(jit_value_is_constant(val.val) && jit_value_is_constant(val.meta))
	{
		ptrs_val_t constVal = ptrs_jit_value_getValConstant(val.val);
		ptrs_meta_t constMeta = ptrs_jit_value_getMetaConstant(val.meta);

		if(constMeta.type != PTRS_TYPE_STRUCT)
			return jit_const_float(func, ptrs_vartof(constVal, constMeta));
	}

	switch(val.constType)
	{
		case -1:
		case PTRS_TYPE_NATIVE:
			break; //use instrinsic
		case PTRS_TYPE_STRUCT:
			if(jit_value_is_constant(val.meta))
			{
				ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
				ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
				if(ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, true) == NULL)
					return jit_insn_convert(func, val.val, jit_type_float64, 0);
			}
			break; //use intrinsic
		case PTRS_TYPE_UNDEFINED:
			return jit_const_long(func, long, 0);
		case PTRS_TYPE_FLOAT:
			return ptrs_jit_reinterpretCast(func, val.val, jit_type_float64);
		default:
			return jit_insn_convert(func, val.val, jit_type_float64, 0);
	}

	jit_value_t ret;
	val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
	ptrs_jit_reusableCall(func, ptrs_vartof, ret, jit_type_float64,
		(jit_type_long, jit_type_ulong), (val.val, val.meta));

	return ret;
}

void ptrs_itoa(char *buff, ptrs_val_t val)
{
	sprintf(buff, "%"PRId64, val.intval);
}
void ptrs_ftona(char *buff, size_t maxlen, ptrs_val_t val)
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
const char *ptrs_stoa(ptrs_val_t val, ptrs_meta_t meta, char *buff)
{
	ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
	jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_tostring, val.nativeval != NULL);
	if(overload != NULL)
	{
		ptrs_var_t ret;
		ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.nativeval, ());

		return ptrs_vartoa(ret.value, ret.meta, buff, 32).value.strval;
	}

	sprintf(buff, "%s:%p", struc->name, val.structval);
	return buff;
}
void ptrs_functoa(char *buff, ptrs_val_t val)
{
	sprintf(buff, "function:%p", val.nativeval);
}
ptrs_jit_var_t ptrs_jit_vartoa(jit_function_t func, ptrs_jit_var_t val)
{
	jit_value_t buff;
	ptrs_jit_var_t ret;
	ret.constType = PTRS_TYPE_NATIVE;

	if(val.constType == -1)
	{
		jit_value_t size = jit_const_int(func, nuint, 32);
		buff = jit_insn_alloca(func, size);

		val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
		jit_value_t retVal;
		ptrs_jit_reusableCall(func, ptrs_vartoa, retVal, ptrs_jit_getVarType(),
			(
				jit_type_long, //ptrs_val_t
				jit_type_ulong, //ptrs_meta_t
				jit_type_void_ptr,
				jit_type_ulong
			),
			(val.val, val.meta, buff, size)
		);

		ret = ptrs_jit_valToVar(func, retVal);
		ret.constType = PTRS_TYPE_NATIVE;
		return ret;
	}
	else
	{
		if(val.constType == PTRS_TYPE_NATIVE)
		{
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
				jit_const_int(func, nuint, strlen("undefined") + 1));
			return ret;
		}
		else if(val.constType == PTRS_TYPE_STRUCT)
		{
			ptrs_jit_reusableCall(func, ptrs_stoa, ret.val, jit_type_void_ptr,
				(jit_type_long, jit_type_long, jit_type_void_ptr),
				(val.val, val.meta, buff)
			);
			return ret;
		}
		else
		{
			void *converters[] = {
				[PTRS_TYPE_INT] = ptrs_itoa,
				[PTRS_TYPE_FLOAT] = ptrs_ftoa,
				[PTRS_TYPE_POINTER] = ptrs_ptoa,
				[PTRS_TYPE_FUNCTION] = ptrs_functoa,
			};

			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
			ptrs_jit_reusableCallVoid(func, converters[val.constType],
				(jit_type_void_ptr, jit_type_long),
				(buff, val.val)
			);
			return ret;
		}
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
			return (intptr_t)val.nativeval;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, val.nativeval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_val_t arg0 = {.intval = PTRS_TYPE_INT};
				ptrs_meta_t arg1 = {.type = PTRS_TYPE_INT};
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.nativeval,
					(&arg0, &arg1));

				return ptrs_vartoi(ret.value, ret.meta);
			}
			/* fallthrough */
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
			return (intptr_t)val.nativeval;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, val.nativeval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_val_t arg0 = {.intval = PTRS_TYPE_FLOAT};
				ptrs_meta_t arg1 = {.type = PTRS_TYPE_INT};
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.nativeval,
					(&arg0, &arg1));

				return ptrs_vartoi(ret.value, ret.meta);
			}
			/* fallthrough */
		default: //pointer type
			return (intptr_t)val.nativeval;
	}
}

ptrs_var_t ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen)
{
	ptrs_var_t result;
	result.value.nativeval = buff;
	result.meta.type = PTRS_TYPE_NATIVE;
	result.meta.array.readOnly = false;
	result.meta.array.size = maxlen;

	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			strncpy(buff, "undefined", maxlen - 1);
			buff[maxlen] = 0;
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
			{
				result.value = val;
				result.meta = meta;
			}
			else
			{
				snprintf(buff, maxlen, "%.*s", len, val.strval); //wat do when maxlen < len? :(
			}
			break;
		case PTRS_TYPE_POINTER:
			snprintf(buff, maxlen, "pointer:%p", val.ptrval);
			break;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_tostring, val.nativeval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.nativeval, ());
				return ptrs_vartoa(ret.value, ret.meta, buff, maxlen);
			}

			snprintf(buff, maxlen, "%s:%p", struc->name, val.structval);
			break;
		case PTRS_TYPE_FUNCTION:
			snprintf(buff, maxlen, "function:%p", val.nativeval);
			break;
	}

	buff[maxlen - 1] = 0;
	return result;
}

void ptrs_metatoa(ptrs_meta_t meta, char *buff, size_t maxlen)
{
	switch(meta.type)
	{
		case PTRS_TYPE_NATIVE:
			if(meta.array.size == 0)
				snprintf(buff, maxlen, "native");
			else
				snprintf(buff, maxlen, "native[%d]", meta.array.size);
			break;
		case PTRS_TYPE_POINTER:
			if(meta.array.size == 0)
				snprintf(buff, maxlen, "pointer");
			else
				snprintf(buff, maxlen, "pointer[%d]", meta.array.size);
			break;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			snprintf(buff, maxlen, "struct:%s", struc->name);
			break;
		case PTRS_TYPE_FUNCTION:
			;
			ptrs_function_t *func = ptrs_meta_getPointer(meta);
			snprintf(buff, maxlen, "function:%s", func->name);
			break;
		default:
			snprintf(buff, maxlen, "%s", ptrs_typetoa(meta.type));
			break;
	}

}

const char * const ptrs_typeStrings[] = {
	[PTRS_TYPE_UNDEFINED] = "undefined",
	[PTRS_TYPE_INT] = "int",
	[PTRS_TYPE_FLOAT] = "float",
	[PTRS_TYPE_NATIVE] = "native",
	[PTRS_TYPE_POINTER] = "pointer",
	[PTRS_TYPE_STRUCT] = "struct",
	[PTRS_TYPE_FUNCTION] = "function",
};
const char *ptrs_typetoa(ptrs_vartype_t type)
{
	if(type < 0 || type >= PTRS_NUM_TYPES)
		return "unknown";
	else
		return ptrs_typeStrings[type];
}
