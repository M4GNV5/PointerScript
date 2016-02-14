#ifndef _PTRS_COMMON
#define _PTRS_COMMON

#include <stdint.h>

typedef enum type
{
	PTRS_TYPE_UNDEFINED,
	PTRS_TYPE_INT,
	PTRS_TYPE_FLOAT,
	PTRS_TYPE_RAW,
	PTRS_TYPE_STRING,
	PTRS_TYPE_POINTER,
	PTRS_TYPE_OBJECT
} ptrs_vartype_t;

typedef struct object
{
	char *name;
	ptrs_var_t *value;
	struct object *next;
} ptrs_object_t;

typedef union val
{
	int64_t intval;
	double floatval;
	void *rawval;
	char *strval;
	ptrs_var_t *ptrval;
	ptrs_object_t *objval;
} ptrs_val_t;

typedef struct var
{
	ptrs_vartype_t type;
	ptrs_val_t value;
} ptrs_var_t;

#endif
