#ifndef _PTRS_COMMON
#define _PTRS_COMMON

#include <stdint.h>

struct ptrs_var;
struct ptrs_ast;

typedef enum type
{
	PTRS_TYPE_UNDEFINED,
	PTRS_TYPE_INT,
	PTRS_TYPE_FLOAT,
	PTRS_TYPE_NATIVE,
	PTRS_TYPE_STRING,
	PTRS_TYPE_POINTER,
	PTRS_TYPE_FUNCTION,
	PTRS_TYPE_STRUCT
} ptrs_vartype_t;

typedef struct object
{
	const char *key;
	struct ptrs_var *value;
	struct object *next;
} ptrs_object_t;

typedef struct ptrs_scope
{
	struct ptrs_scope *outer;
	ptrs_object_t *current;
	void *stackstart;
	void *sp;
	struct ptrs_ast *callAst;
	struct ptrs_scope *callScope;
	const char *calleeName;
	void *error;
	uint8_t exit : 2; // 00 = nothing   01 = exit block (continue)   10 = exit loop (break)   11 = exit function (return)
} ptrs_scope_t;

typedef struct function
{
	char *name;
	char **args;
	int argc;
	struct ptrs_ast *body;
	ptrs_scope_t *scope;
	void *nativeCb;
} ptrs_function_t;

struct ptrs_structlist
{
	char *name;
	unsigned int offset;
	ptrs_function_t *function;
	struct ptrs_structlist *next;
};
typedef struct struc
{
	char *name;
	struct ptrs_structlist *member;
	ptrs_function_t *constructor;
	int size;
	void *data;
} ptrs_struct_t;

typedef union val
{
	int64_t intval;
	double floatval;
	const char *strval;
	struct ptrs_var *ptrval;
	void *nativeval;
	ptrs_function_t *funcval;
	ptrs_struct_t *structval;
} ptrs_val_t;

typedef union meta
{
	ptrs_struct_t *this;
	int8_t *pointer;
} ptrs_meta_t;

struct ptrs_var
{
	ptrs_val_t value;
	ptrs_vartype_t type;
	ptrs_meta_t meta;
};
typedef struct ptrs_var ptrs_var_t;

#endif
