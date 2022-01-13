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
#include "../include/run.h"
#include "../jit.h"
#include "jit/jit-function.h"
#include "jit/jit-value.h"

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
	snprintf(buff, 32, "%"PRId64, val.intval);
}
void ptrs_ftoa(char *buff, ptrs_val_t val)
{
	snprintf(buff, 32, "%g", val.floatval);
}
const char *ptrs_ptoa(ptrs_val_t val, ptrs_meta_t meta, char *buff)
{
	if(meta.array.typeIndex == PTRS_NATIVETYPE_INDEX_CHAR && strnlen(val.ptrval, meta.array.size) < meta.array.size)
	{
		return val.ptrval;
	}

	ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(NULL, meta);
	snprintf(buff, 32, "%s[%d]@%p", type->name, meta.array.size, val.ptrval);
	return buff;
}
const char *ptrs_stoa(ptrs_val_t val, ptrs_meta_t meta, char *buff)
{
	ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
	jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_tostring, val.ptrval != NULL);
	if(overload != NULL)
	{
		ptrs_var_t ret;
		ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.ptrval, ());

		return ptrs_vartoa(ret.value, ret.meta, buff, 32).value.ptrval;
	}

	sprintf(buff, "%s:%p", struc->name, val.structval);
	return buff;
}
void ptrs_functoa(char *buff, ptrs_val_t val)
{
	jit_function_t func = jit_function_from_closure(ptrs_jit_context, val.ptrval);
	if(func)
	{
		const char *name = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
		snprintf(buff, 32, "function:%s", name);
	}
	else
	{
		snprintf(buff, 32, "function:%p", val.ptrval);
	}
}
ptrs_jit_var_t ptrs_jit_vartoa(jit_function_t func, ptrs_jit_var_t val)
{
	jit_value_t buff;
	ptrs_jit_var_t ret;
	ret.constType = PTRS_TYPE_POINTER;

	if(val.constType == -1)
	{
		buff = jit_insn_array(func, 32);

		val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
		jit_value_t retVal;
		ptrs_jit_reusableCall(func, ptrs_vartoa, retVal, ptrs_jit_getVarType(),
			(
				jit_type_long, //ptrs_val_t
				jit_type_ulong, //ptrs_meta_t
				jit_type_void_ptr,
				jit_type_ulong
			),
			(val.val, val.meta, buff, jit_const_int(func, nuint, 32))
		);

		ret = ptrs_jit_valToVar(func, retVal);
		ret.constType = PTRS_TYPE_POINTER;
		return ret;
	}
	else
	{
		if(val.constType == PTRS_TYPE_POINTER && jit_value_is_constant(val.meta))
		{
			ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
			if(meta.array.typeIndex == PTRS_NATIVETYPE_INDEX_CHAR)
				return val;
		}

		buff = jit_insn_array(func, 32);
		ptrs_jit_var_t ret;
		ret.constType = PTRS_TYPE_POINTER;
		ret.val = buff;
		ret.meta = ptrs_jit_const_arrayMeta(func, 32, PTRS_NATIVETYPE_INDEX_CHAR);

		if(val.constType == PTRS_TYPE_UNDEFINED)
		{
			jit_insn_memcpy(func, buff, jit_const_int(func, void_ptr, (uintptr_t)"undefined"),
				jit_const_int(func, nuint, strlen("undefined") + 1));
			return ret;
		}
		else if(val.constType == PTRS_TYPE_INT)
		{
			ptrs_jit_reusableCallVoid(func, ptrs_itoa,
				(jit_type_void_ptr, jit_type_long),
				(buff, val.val)
			);
		}
		else if(val.constType == PTRS_TYPE_FLOAT)
		{
			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
			ptrs_jit_reusableCallVoid(func, ptrs_ftoa,
				(jit_type_void_ptr, jit_type_long),
				(buff, val.val)
			);

		}
		else if(val.constType == PTRS_TYPE_STRUCT)
		{
			ptrs_jit_reusableCall(func, ptrs_stoa, ret.val, jit_type_void_ptr,
				(jit_type_long, jit_type_long, jit_type_void_ptr),
				(val.val, val.meta, buff)
			);
		}
		else if(val.constType == PTRS_TYPE_POINTER)
		{
			ptrs_jit_reusableCall(func, ptrs_ptoa, ret.val, jit_type_void_ptr,
				(jit_type_long, jit_type_long, jit_type_void_ptr),
				(val.val, val.meta, buff)
			);
		}
		else if(val.constType == PTRS_TYPE_FUNCTION)
		{
			ptrs_jit_reusableCallVoid(func, ptrs_functoa,
				(jit_type_void_ptr, jit_type_long),
				(buff, val.val)
			);
		}
		else
		{
			ptrs_error(NULL, "Cannot convert unknown type to string: %t", val.constType);
		}

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
		case PTRS_TYPE_POINTER:
			return val.ptrval != NULL && meta.array.size != 0;
		default: //pointer type
			return val.ptrval != NULL;
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
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, val.ptrval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_val_t arg0 = {.intval = PTRS_TYPE_INT};
				ptrs_meta_t arg1 = {.type = PTRS_TYPE_INT};
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.ptrval,
					(&arg0, &arg1));

				return ptrs_vartoi(ret.value, ret.meta);
			}
			/* fallthrough */
		default: //pointer type
			return (intptr_t)val.ptrval;
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
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_cast_builtin, val.ptrval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_val_t arg0 = {.intval = PTRS_TYPE_FLOAT};
				ptrs_meta_t arg1 = {.type = PTRS_TYPE_INT};
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.ptrval,
					(&arg0, &arg1));

				return ptrs_vartoi(ret.value, ret.meta);
			}
			/* fallthrough */
		default: //pointer type
			return (intptr_t)val.ptrval;
	}
}

ptrs_var_t ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen)
{
	ptrs_var_t result;
	result.value.ptrval = buff;
	result.meta.type = PTRS_TYPE_POINTER;
	result.meta.array.typeIndex = PTRS_NATIVETYPE_INDEX_CHAR;
	result.meta.array.size = maxlen;

	switch(meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			strncpy(buff, "undefined", maxlen - 1);
			buff[maxlen - 1] = 0;
			break;
		case PTRS_TYPE_INT:
			snprintf(buff, maxlen, "%"PRId64, val.intval);
			break;
		case PTRS_TYPE_FLOAT:
			snprintf(buff, maxlen, "%g", val.floatval);
			break;
		case PTRS_TYPE_POINTER:
			if(meta.array.typeIndex == PTRS_NATIVETYPE_INDEX_CHAR && meta.array.size > 0)
			{
				int len = strnlen(val.ptrval, meta.array.size);
				if(len < meta.array.size)
				{
					result.value = val;
					result.meta = meta;
				}
				else
				{
					snprintf(buff, maxlen, "%.*s", len, (char *)val.ptrval); // XXX: what should we do when maxlen < len? :(
				}
			}
			else
			{
				ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(NULL, meta);
				snprintf(buff, maxlen, "%s[%d]@%p", type->name, meta.array.size, val.ptrval);
			}
			break;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_tostring, val.ptrval != NULL);
			if(overload != NULL)
			{
				ptrs_var_t ret;
				ptrs_jit_applyNested(overload, &ret, struc->parentFrame, val.ptrval, ());
				return ptrs_vartoa(ret.value, ret.meta, buff, maxlen);
			}

			snprintf(buff, maxlen, "%s:%p", struc->name, val.structval);
			break;
		case PTRS_TYPE_FUNCTION:
			;
			jit_function_t func = jit_function_from_closure(ptrs_jit_context, val.ptrval);
			if(func)
			{
				const char *name = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
				snprintf(buff, maxlen, "function:%s", name);
			}
			else
			{
				snprintf(buff, maxlen, "function:%p", val.ptrval);
			}
			break;
	}

	buff[maxlen - 1] = 0;
	return result;
}

void ptrs_metatoa(ptrs_meta_t meta, char *buff, size_t maxlen)
{
	switch(meta.type)
	{
		case PTRS_TYPE_POINTER:
			;
			ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(NULL, meta);
			snprintf(buff, maxlen, "%s[%d]", type->name, meta.array.size);
			break;
		case PTRS_TYPE_STRUCT:
			;
			ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
			snprintf(buff, maxlen, "struct:%s", struc->name);
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
