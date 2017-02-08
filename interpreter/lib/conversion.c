#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "../../parser/common.h"

bool ptrs_vartob(ptrs_var_t *val)
{
	if(val == NULL)
		return false;

	switch(val->type)
	{
		case PTRS_TYPE_UNDEFINED:
			return false;
		case PTRS_TYPE_INT:
			return val->value.intval != 0;
		case PTRS_TYPE_FLOAT:
			return val->value.floatval != 0;
		default: //pointer type
			return val->value.strval != NULL;
	}
}

int64_t ptrs_vartoi(ptrs_var_t *val)
{
	if(val == NULL)
		return 0;

	switch(val->type)
	{
		case PTRS_TYPE_UNDEFINED:
			return 0;
		case PTRS_TYPE_INT:
			return val->value.intval;
		case PTRS_TYPE_FLOAT:
			return val->value.floatval;
		case PTRS_TYPE_NATIVE:
			return strtoimax(val->value.strval, NULL, 0);
		default: //pointer type
			return (intptr_t)val->value.strval;
	}
}

double ptrs_vartof(ptrs_var_t *val)
{
	if(val == NULL)
		return 0;

	switch(val->type)
	{
		case PTRS_TYPE_UNDEFINED:
			return NAN;
		case PTRS_TYPE_INT:
			return val->value.intval;
		case PTRS_TYPE_FLOAT:
			return val->value.floatval;
		case PTRS_TYPE_NATIVE:
			return atof(val->value.strval);
		default: //pointer type
			return (intptr_t)val->value.strval;
	}
}

const char *ptrs_vartoa(ptrs_var_t *val, char *buff, size_t maxlen)
{
	if(val == NULL)
		return "undefined";

	ptrs_vartype_t type = val->type;
	switch(type)
	{
		case PTRS_TYPE_UNDEFINED:
			return "undefined";
			strncpy(buff, "undefined", maxlen);
			break;
		case PTRS_TYPE_INT:
			snprintf(buff, maxlen, "%"PRId64, val->value.intval);
			break;
		case PTRS_TYPE_FLOAT:
			snprintf(buff, maxlen, "%.8f", val->value.floatval);

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
			if(val->meta.array.size == 0)
			{
				snprintf(buff, maxlen, "native:%p", val->value.strval);
				break;
			}

			int len = strnlen(val->value.strval, val->meta.array.size);
			if(len < val->meta.array.size)
				return val->value.strval;
			else
				snprintf(buff, maxlen, "%.*s", len, val->value.strval); //wat do when maxlen < len? :(
			break;
		case PTRS_TYPE_POINTER:
			snprintf(buff, maxlen, "pointer:%p", val->value.ptrval);
			break;
		case PTRS_TYPE_FUNCTION:
			snprintf(buff, maxlen, "function:%p", val->value.funcval);
			break;
		case PTRS_TYPE_STRUCT:
			snprintf(buff, maxlen, "%s:%p", val->value.structval->name, val->value.structval);
			break;
	}
	buff[maxlen - 1] = 0;
	return buff;
}

const char *typeofStrings[] = {
	"undefined",
	"int",
	"float",
	"native",
	"pointer",
	"function",
	"struct"
};
int numTypes = sizeof(typeofStrings) / sizeof(char *);

const char *ptrs_typetoa(ptrs_vartype_t type)
{
	if(type < 0 || type >= numTypes)
		return "unknown";
	return typeofStrings[type];
}
