#include <stdint.h>

#include "../../parser/common.h"
#include "../include/conversion.h"

ptrs_var_t *ptrs_handle_native_getInt(void *target, size_t size, ptrs_var_t *value)
{
	value->type = PTRS_TYPE_INT;
	switch(size)
	{
		case 1:
			value->value.intval = *(int8_t *)target;
			break;
		case 2:
			value->value.intval = *(int16_t *)target;
			break;
		case 4:
			value->value.intval = *(int32_t *)target;
			break;
		case 8:
			value->value.intval = *(int64_t *)target;
			break;
	}
	return value;
}

ptrs_var_t *ptrs_handle_native_setInt(void *target, size_t size, ptrs_var_t *value)
{
	int64_t val = ptrs_vartoi(value);
	switch(size)
	{
		case 1:
			*(int8_t *)target = val;
			break;
		case 2:
			*(int16_t *)target = val;
			break;
		case 4:
			*(int32_t *)target = val;
			break;
		case 8:
			*(int64_t *)target = val;
			break;
	}
	return NULL;
}

ptrs_var_t *ptrs_handle_native_getUInt(void *target, size_t size, ptrs_var_t *value)
{
	value->type = PTRS_TYPE_INT;
	switch(size)
	{
		case 1:
			value->value.intval = *(uint8_t *)target;
			break;
		case 2:
			value->value.intval = *(uint16_t *)target;
			break;
		case 4:
			value->value.intval = *(uint32_t *)target;
			break;
		case 8:
			value->value.intval = *(uint64_t *)target;
			break;
	}
	return value;
}

ptrs_var_t *ptrs_handle_native_setUInt(void *target, size_t size, ptrs_var_t *value)
{
	uint64_t val = ptrs_vartoi(value);
	switch(size)
	{
		case 1:
			*(uint8_t *)target = val;
			break;
		case 2:
			*(uint16_t *)target = val;
			break;
		case 4:
			*(uint32_t *)target = val;
			break;
		case 8:
			*(uint64_t *)target = val;
			break;
	}
	return NULL;
}

ptrs_var_t *ptrs_handle_native_getNative(void *target, size_t size, ptrs_var_t *value)
{
	value->type = PTRS_TYPE_NATIVE;
	value->value.ptrval = *(ptrs_var_t **)target;
	value->meta.array.size = 0;
	value->meta.array.readOnly = false;
	return value;
}

ptrs_var_t *ptrs_handle_native_getPointer(void *target, size_t size, ptrs_var_t *value)
{
	value->type = PTRS_TYPE_POINTER;
	value->value.ptrval = *(ptrs_var_t **)target;
	value->meta.array.size = 0;
	return value;
}

ptrs_var_t *ptrs_handle_native_setPointer(void *target, size_t size, ptrs_var_t *value)
{
	if(value->type == PTRS_TYPE_FLOAT)
		*(void **)target = (void *)(uint64_t)value->value.floatval;
	else
		*(void **)target = value->value.nativeval;
	return NULL;
}

ptrs_var_t *ptrs_handle_native_getFloat(void *target, size_t size, ptrs_var_t *value)
{
	value->type = PTRS_TYPE_FLOAT;
	switch(size)
	{
		case 4:
			value->value.floatval = *(float *)target;
			break;
		case 8:
			value->value.floatval = *(double *)target;
			break;
	}
	return value;
}

ptrs_var_t *ptrs_handle_native_setFloat(void *target, size_t size, ptrs_var_t *value)
{
	double val = ptrs_vartof(value);
	switch(size)
	{
		case 4:
			*(float *)target = val;
			break;
		case 8:
			*(double *)target = val;
			break;
	}
	return NULL;
}
