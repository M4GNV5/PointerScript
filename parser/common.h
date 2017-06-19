#ifndef _PTRS_COMMON
#define _PTRS_COMMON

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdbool.h>
#include <jit/jit.h>

#ifndef _GNU_SOURCE
#include <alloca.h>
#define strdup(str) (strcpy(malloc(strlen(str) + 1), str))
#endif

struct ptrs_var;
struct ptrs_ast;

typedef struct
{
	jit_value_t val;
	jit_value_t meta;
} ptrs_jit_var_t;

typedef enum type
{
	PTRS_TYPE_UNDEFINED,
	PTRS_TYPE_INT,
	PTRS_TYPE_FLOAT,
	PTRS_TYPE_NATIVE,
	PTRS_TYPE_POINTER,
	PTRS_TYPE_FUNCTION,
	PTRS_TYPE_STRUCT,
} ptrs_vartype_t;

struct ptrs_error;

typedef struct ptrs_patchlist
{
	void *patch;
	struct ptrs_patchlist *next;
} ptrs_patchlist_t;
typedef struct ptrs_scope
{
	jit_label_t continueLabel;
	jit_label_t breakLabel;
	struct ptrs_error *errors;
} ptrs_scope_t;

typedef void (*ptrs_nativetype_handler_t)(void *target, size_t typeSize, struct ptrs_var *value);
typedef struct ptrs_nativetype_info
{
	const char *name;
	size_t size;
	ptrs_nativetype_handler_t getHandler;
	ptrs_nativetype_handler_t setHandler;
} ptrs_nativetype_info_t;

typedef enum
{
	PTRS_JIT_FUNCTIONMETA_NAME,
} ptrs_jit_functionmeta_t;
typedef struct function
{
	char *name;
	int argc;
	ptrs_jit_var_t vararg;
	ptrs_jit_var_t *args;
	struct ptrs_ast **argv;
	struct ptrs_ast *body;
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
struct ptrs_structmember
{
	char *name;
	unsigned offset;
	uint16_t namelen;
	uint8_t protection : 2; //0 = public, 1 = internal, 2 = private
	uint8_t isStatic : 1;
	enum ptrs_structmembertype type;
	union
	{
		struct ptrs_ast *startval;
		struct
		{
			struct ptrs_astlist *arrayInit;
			uint64_t size;
		};
		ptrs_function_t *function;
		ptrs_nativetype_info_t *type;
	} value;
};
struct ptrs_opoverload
{
	uint8_t isStatic : 1;
	void *op;
	ptrs_function_t *handler;
	struct ptrs_opoverload *next;
};
typedef struct ptrs_struct
{
	char *name;
	ptrs_jit_var_t *location;
	struct ptrs_structmember *member;
	struct ptrs_opoverload *overloads;
	ptrs_scope_t *scope;
	uint32_t size;
	uint16_t memberCount;
	bool isOnStack;
	void *staticData;
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

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
typedef struct meta
{
	union
	{
		struct
		{
			uint32_t size;
			uint16_t padding;
			bool readOnly;
		} __attribute__((packed)) array;
	};
	uint8_t type;
} ptrs_meta_t;
#else
typedef struct meta
{
	uint8_t type;
	union
	{
		struct
		{
			bool readOnly;
			uint16_t padding;
			uint32_t size;
		} __attribute__((packed)) array;
	};
} ptrs_meta_t;
#endif

typedef struct ptrs_var
{
	ptrs_val_t value;
	ptrs_meta_t meta;
} ptrs_var_t;

#define jit_const_int(func, type, val) (jit_value_create_nint_constant(func, jit_type_##type, val))
#define jit_const_long(func, type, val) (jit_value_create_long_constant(func, jit_type_##type, val))

#define ptrs_const_meta(type) ((uintptr_t)(type) << 56)
#define ptrs_const_arrayMeta(type, readOnly, size) ((uint8_t)(type) << 56 | (bool)(readOnly) << 48 | (size))

#define ptrs_jit_const_meta(func, type) (jit_value_create_long_constant(func, jit_type_ulong, ptrs_const_meta(type)))
#define ptrs_jit_arrayMeta(func, type, readOnly, size) \
	(jit_insn_or(func, \
		jit_insn_or(func, \
			jit_insn_shl(func, (type), jit_const_long(func, ulong, 56)), \
			jit_insn_shl(func, (readOnly), jit_const_long(func, ulong, 48)) \
		), \
		(size) \
	))

#define ptrs_jit_getType(func, meta) (jit_insn_ushr(func, meta, jit_const_int(func, ubyte, 56)))
#define ptrs_jit_getArraySize(func, meta) (jit_insn_and(func, meta, jit_const_long(func, ulong, 0xFFFFFFFF)))

#define ptrs_jit_hasType(func, meta, type) (jit_insn_eq((func), ptrs_jit_getType(func, meta), jit_const_long(func, ulong, (type))))

#endif
