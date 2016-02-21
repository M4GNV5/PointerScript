#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"

bool ptrs_vartob(ptrs_var_t *val)
{
	if(val == NULL)
		return false;

	ptrs_vartype_t type = val->type;
	switch(type)
	{
		case PTRS_TYPE_UNDEFINED:
			return false;
		case PTRS_TYPE_INT:
			return val->value.intval != 0;
		case PTRS_TYPE_FLOAT:
			return val->value.floatval != 0;
		case PTRS_TYPE_STRING:
			return *(val->value.strval) != 0;
		default: //pointer type
			return val->value.strval != NULL;
	}
}

void ptrs_vartoa(ptrs_var_t *val, char *buff)
{
	if(val == NULL)
	{
		strcpy(buff, "undefined");
		return;
	}

	ptrs_vartype_t type = val->type;
	switch(type)
	{
		case PTRS_TYPE_UNDEFINED:
			strcpy(buff, "undefined");
			break;
		case PTRS_TYPE_INT:
			sprintf(buff, "%d", val->value.intval);
			break;
		case PTRS_TYPE_FLOAT:
			sprintf(buff, "%f", val->value.floatval);
			break;
		case PTRS_TYPE_STRING:
			strcpy(buff, val->value.strval);
			break;
		default: //pointer type
			sprintf(buff, "%p", val->value.strval);
			break;
	}
}

const char *typeofStrings[] = {
	"undefined",
	"int",
	"float",
	"raw",
	"string",
	"pointer",
	"function",
	"function",
	"object"
};

const char *ptrs_typetoa(ptrs_vartype_t type)
{
	return typeofStrings[type];
}
