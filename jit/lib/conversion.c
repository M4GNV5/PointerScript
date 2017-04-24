#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <alloca.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "../../parser/common.h"

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
