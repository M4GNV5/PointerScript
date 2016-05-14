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
		case PTRS_TYPE_NATIVE:
			return *(val->value.strval) != 0;
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
			return atoi(val->value.strval);
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
			snprintf(buff, maxlen, "%g", val->value.floatval);
			break;
		case PTRS_TYPE_NATIVE:
			return val->value.strval;
			break;
		case PTRS_TYPE_FUNCTION:
			snprintf(buff, maxlen, "function:%s", val->value.funcval->name);
			break;
		case PTRS_TYPE_STRUCT:
			snprintf(buff, maxlen, "struct:%s", val->value.structval->name);
			break;
		default: //pointer type
			snprintf(buff, maxlen, "%p", val->value.strval);
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
	"string",
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
