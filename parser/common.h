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
	int8_t constType;
	uint8_t addressable : 1;
} ptrs_jit_var_t;

typedef enum
{
	PTRS_TYPE_UNDEFINED,
	PTRS_TYPE_INT,
	PTRS_TYPE_FLOAT,
	PTRS_TYPE_NATIVE,
	PTRS_TYPE_POINTER,
	PTRS_TYPE_STRUCT,
	PTRS_TYPE_FUNCTION,

	PTRS_NUM_TYPES
} ptrs_vartype_t;

struct ptrs_error;

typedef struct ptrs_patchlist
{
	void *patch;
	struct ptrs_patchlist *next;
} ptrs_patchlist_t;
typedef struct
{
	bool loopControlAllowed; // whether or not continue and break are currenlt allowed
	jit_label_t continueLabel;
	jit_label_t breakLabel;
	struct ptrs_assertion *firstAssertion;
	struct ptrs_assertion *lastAssertion;
	jit_value_t returnAddr;
	jit_value_t indexSize;
	jit_function_t rootFunc;
	void **rootFrame;
} ptrs_scope_t;

typedef void (*ptrs_nativetype_handler_t)(void *target, size_t typeSize, struct ptrs_var *value);
typedef struct
{
	const char *name;
	size_t size;
	jit_type_t jitType;
	ptrs_vartype_t varType;
	ptrs_nativetype_handler_t getHandler;
	ptrs_nativetype_handler_t setHandler;
} ptrs_nativetype_info_t;

typedef enum
{
	PTRS_JIT_FUNCTIONMETA_NAME,
	PTRS_JIT_FUNCTIONMETA_AST,
	PTRS_JIT_FUNCTIONMETA_CALLBACK,
} ptrs_jit_functionmeta_t;
typedef struct ptrs_funcparameter
{
	char *name;
	ptrs_jit_var_t arg;
	int8_t type;
	struct ptrs_ast *argv;
	struct ptrs_funcparameter *next;
} ptrs_funcparameter_t;
typedef struct
{
	char *name;
	ptrs_jit_var_t thisVal;
	ptrs_jit_var_t *vararg;
	ptrs_funcparameter_t *args;
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
			struct ptrs_astlist *init;
			uint64_t size;
		} array;
		struct
		{
			ptrs_function_t *ast;
			jit_function_t func;
		} function;
		ptrs_nativetype_info_t *type;
	} value;
};
struct ptrs_opoverload
{
	uint8_t isStatic : 1;
	void *op;
	ptrs_function_t *handler;
	jit_function_t handlerFunc;
	struct ptrs_opoverload *next;
};
typedef struct
{
	char *name;
	struct ptrs_ast *ast;
	ptrs_jit_var_t *location;
	struct ptrs_structmember *member;
	struct ptrs_opoverload *overloads;
	uint32_t size;
	uint16_t memberCount;
	size_t lastCodepos;
	void *staticData;
	void *parentFrame;
} ptrs_struct_t;

typedef union
{
	int64_t intval;
	double floatval;
	const char *strval;
	struct ptrs_var *ptrval;
	void *nativeval;
	ptrs_struct_t *structval;
} ptrs_val_t;

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
typedef struct
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
		uint8_t pointer[7]; //actually 8, use the ptrs_meta_getPointer macro below
	};
} ptrs_meta_t;
#else
typedef struct
{
	union
	{
		struct
		{
			uint32_t size;
			uint16_t padding;
			bool readOnly;
		} __attribute__((packed)) array;
		uint8_t pointer[7]; //actually 8, use the ptrs_meta_getPointer macro below
	};
	uint8_t type;
} ptrs_meta_t;
#endif

typedef struct ptrs_var
{
	ptrs_val_t value;
	ptrs_meta_t meta;
} ptrs_var_t;

#define jit_const_int(func, type, val) (jit_value_create_nint_constant(func, jit_type_##type, val))
#define jit_const_long(func, type, val) (jit_value_create_long_constant(func, jit_type_##type, val))
#define jit_const_float(func, val) (jit_value_create_float64_constant(func, jit_type_float64, val))

#define ptrs_const_meta(type) ((uint64_t)(type))
#define ptrs_const_arrayMeta(type, readOnly, size) \
	(((uint64_t)(size) << 32) | (uint64_t)(readOnly) << 8 | (uint64_t)(type))
#define ptrs_const_pointerMeta(type, ptr) \
	(((uint64_t)(uintptr_t)(ptr) << 8) | (uint64_t)(type))

#define ptrs_meta_getPointer(meta) \
	(void *)(*(uint64_t *)&(meta) >> 8)
#define ptrs_meta_setPointer(meta, ptr) \
	(*(uint64_t *)&(meta) = (((uint64_t)(uintptr_t)(ptr) << 8) | (meta).type))

#define ptrs_jit_const_meta(func, type) \
	(jit_const_long(func, ulong, ptrs_const_meta(type)))
#define ptrs_jit_const_arrayMeta(func, type, readOnly, size) \
	(jit_const_long(func, ulong, ptrs_const_arrayMeta(type, readOnly, size)))
#define ptrs_jit_const_pointerMeta(func, type, ptr) \
	(jit_const_long(func, ulong, ptrs_const_pointerMeta(type, ptr)))

#define ptrs_jit_arrayMeta(func, type, readOnly, size) \
	(jit_insn_or(func, \
		jit_insn_or(func, \
			jit_insn_shl(func, \
				jit_insn_convert(func, (size), jit_type_ulong, 0), \
				jit_const_long(func, ulong, 32) \
			), \
			jit_insn_shl(func, (readOnly), jit_const_long(func, ulong, 8)) \
		), \
		(type) \
	))
#define ptrs_jit_pointerMeta(func, type, ptr) \
	(jit_insn_or(func, \
		jit_insn_shl(func, (ptr), jit_const_long(func, ulong, 8)), \
		(type) \
	))

#define ptrs_jit_getType(func, meta) (jit_insn_and(func, meta, jit_const_long(func, ulong, 0xFF)))
#define ptrs_jit_getArraySize(func, meta) (jit_insn_shr(func, meta, jit_const_int(func, ubyte, 32)))
#define ptrs_jit_getMetaPointer(func, meta) (jit_insn_shr(func, meta, jit_const_int(func, ubyte, 8)))

#define ptrs_jit_setArraySize(func, meta, size) \
	(jit_insn_or(func, \
		jit_insn_and(func, (meta), jit_const_long(func, ulong, 0xFFFFFFFF)), \
		jit_insn_shl(func, \
			jit_insn_convert(func, (size), jit_type_ulong, 0), \
			jit_const_long(func, ulong, 32) \
		) \
	))
#define ptrs_jit_setMetaPointer(func, meta, ptr) \
	(jit_insn_or(func, \
		jit_insn_and(func, (meta), jit_const_long(func, ulong, 0xFF)), \
		jit_insn_shl(func, (ptr), jit_const_int(func, ubyte, 8)) \
	))

#define ptrs_jit_hasType(func, meta, type) \
	(jit_insn_eq((func), ptrs_jit_getType(func, meta), jit_const_long(func, ulong, (type))))
#define ptrs_jit_doesntHaveType(func, meta, type) \
	(jit_insn_ne((func), ptrs_jit_getType(func, meta), jit_const_long(func, ulong, (type))))

#endif
