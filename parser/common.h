#ifndef _PTRS_COMMON
#define _PTRS_COMMON

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdbool.h>

struct ptrs_var;
struct ptrs_ast;

typedef struct ptrs_symbol
{
	unsigned scope;
	unsigned offset;
} ptrs_symbol_t;

typedef enum type
{
	PTRS_TYPE_UNDEFINED,
	PTRS_TYPE_INT,
	PTRS_TYPE_FLOAT,
	PTRS_TYPE_NATIVE,
	PTRS_TYPE_POINTER,
	PTRS_TYPE_FUNCTION,
	PTRS_TYPE_STRUCT
} ptrs_vartype_t;

struct ptrs_error;

typedef struct ptrs_scope
{
	struct ptrs_scope *outer;
	void *stackstart;
	void *sp;
	void *bp;
	struct ptrs_ast *callAst;
	struct ptrs_scope *callScope;
	const char *calleeName;
	struct ptrs_error *error;
	uint8_t exit : 2; // 00 = nothing   01 = exit block (continue)   10 = exit loop (break)   11 = exit function (return)
} ptrs_scope_t;

typedef enum
{
	PTRS_CTYPE_CHAR,
	PTRS_CTYPE_SHORT,
	PTRS_CTYPE_INT,
	PTRS_CTYPE_LONG,
	PTRS_CTYPE_LONGLONG,

	PTRS_CTYPE_UCHAR,
	PTRS_CTYPE_USHORT,
	PTRS_CTYPE_UINT,
	PTRS_CTYPE_ULONG,
	PTRS_CTYPE_ULONGLONG,

	PTRS_CTYPE_I8,
	PTRS_CTYPE_I16,
	PTRS_CTYPE_I32,
	PTRS_CTYPE_I64,

	PTRS_CTYPE_U8,
	PTRS_CTYPE_U16,
	PTRS_CTYPE_U32,
	PTRS_CTYPE_U64,

	PTRS_CTYPE_SINGLE,
	PTRS_CTYPE_DOUBLE,
	PTRS_CTYPE_POINTER,
} ptrs_nativetype_t;

typedef struct ptrs_var *(*ptrs_nativetype_handler_t)(void *target, size_t typeSize, struct ptrs_var *value);
typedef struct ptrs_nativetype_info
{
	const char *name;
	ptrs_nativetype_t type;
	size_t size;
	ptrs_nativetype_handler_t getHandler;
	ptrs_nativetype_handler_t setHandler;
} ptrs_nativetype_info_t;

typedef struct function
{
	char *name;
	int argc;
	unsigned stackOffset;
	ptrs_symbol_t vararg;
	ptrs_symbol_t *args;
	struct ptrs_ast **argv;
	struct ptrs_ast *body;
	ptrs_scope_t *scope;
	void *nativeCb;
} ptrs_function_t;

enum ptrs_structmembertype
{
	PTRS_STRUCTMEMBER_VAR,
	PTRS_STRUCTMEMBER_FUNCTION,
	PTRS_STRUCTMEMBER_ARRAY,
	PTRS_STRUCTMEMBER_VARARRAY,
	PTRS_STRUCTMEMBER_GETTER,
	PTRS_STRUCTMEMBER_SETTER,
	PTRS_STRUCTMEMBER_TYPED,
};
union ptrs_structmember
{
	struct ptrs_ast *startval;
	ptrs_function_t *function;
	uint64_t size;
	ptrs_nativetype_info_t *type;
};
struct ptrs_structlist
{
	char *name;
	unsigned int offset;
	bool isPrivate;
	enum ptrs_structmembertype type;
	union ptrs_structmember value;
	struct ptrs_structlist *next;
};
struct ptrs_opoverload
{
	bool isLeftSide;
	void *op;
	ptrs_function_t *handler;
	struct ptrs_opoverload *next;
};
typedef struct ptrs_struct
{
	char *name;
	ptrs_symbol_t symbol;
	struct ptrs_structlist *member;
	struct ptrs_opoverload *overloads;
	ptrs_scope_t *scope;
	int size;
	bool isOnStack;
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
	struct
	{
		bool readOnly;
		uint32_t size;
	} array;
} ptrs_meta_t;

struct ptrs_var
{
	ptrs_val_t value;
	ptrs_meta_t meta;
	ptrs_vartype_t type;
};
typedef struct ptrs_var ptrs_var_t;

#endif
