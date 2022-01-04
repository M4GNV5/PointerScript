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

struct ptrs_ast;
struct ptrs_struct;

typedef union
{
	int64_t intval;
	double floatval;
	void *ptrval;
	struct ptrs_struct *structval;
} ptrs_val_t;

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
typedef struct
{
	uint8_t type;
	union
	{
		struct
		{
			uint8_t typeIndex;
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
			uint8_t typeIndex;
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
	uint32_t loopControlAllowed : 1; // whether or not continue and break are currenlt allowed
	uint32_t returnForLoopControl : 1;
	uint32_t hasCustomContinueLabel : 1;
	jit_label_t continueLabel;
	jit_label_t breakLabel;
	struct ptrs_assertion *firstAssertion;
	struct ptrs_assertion *lastAssertion;
	jit_label_t rethrowLabel;
	struct ptrs_catcher_labels *tryCatches;
	jit_value_t tryCatchException; // at runtime a pointer to a ptrs_error_t
	ptrs_meta_t returnType;
	jit_value_t returnAddr;
	jit_value_t indexSize;
	jit_function_t rootFunc;
	void **rootFrame;
} ptrs_scope_t;

typedef void (*ptrs_nativetype_handler_t)(void *target, size_t typeSize, struct ptrs_var *value);
typedef void (*ptrs_nativetype_jit_handler_t)(jit_value_t target, size_t typeSize, ptrs_jit_var_t *value);
typedef struct
{
	uint64_t size; // make sure the size is the first element so we can load it directly from JIT code
	const char *name;
	jit_type_t jitType;
	ptrs_vartype_t varType;
	ptrs_nativetype_handler_t getHandler;
	ptrs_nativetype_handler_t setHandler;
} ptrs_nativetype_info_t;

// array with all existing native types
extern ptrs_nativetype_info_t ptrs_nativeTypes[];
extern const int ptrs_nativeTypeCount;

// index of the `var` and `char` type in ptrs_nativeTypes
#define PTRS_NATIVETYPE_INDEX_CHAR ((size_t)0)
#define PTRS_NATIVETYPE_INDEX_U8 ((size_t)14)
#define PTRS_NATIVETYPE_INDEX_CFUNC ((size_t)20)
#define PTRS_NATIVETYPE_INDEX_VAR ((size_t)28)

typedef struct
{
	ptrs_meta_t meta;
	ptrs_nativetype_info_t *nativetype; // optional
} ptrs_typing_t;

typedef enum
{
	PTRS_JIT_FUNCTIONMETA_NAME,
	PTRS_JIT_FUNCTIONMETA_AST,
	PTRS_JIT_FUNCTIONMETA_FUNCAST,
	PTRS_JIT_FUNCTIONMETA_CALLBACK,
	PTRS_JIT_FUNCTIONMETA_CLOSURE,
	PTRS_JIT_FUNCTIONMETA_UNCHECKED,
} ptrs_jit_functionmeta_t;
typedef struct ptrs_funcparameter
{
	char *name;
	ptrs_jit_var_t arg;
	ptrs_typing_t typing;
	struct ptrs_ast *argv;
	struct ptrs_funcparameter *next;
} ptrs_funcparameter_t;
typedef struct
{
	char *name;
	ptrs_jit_var_t thisVal;
	bool usesTryCatch;
	ptrs_jit_var_t *vararg;
	ptrs_funcparameter_t *args;
	ptrs_typing_t retType;
	struct ptrs_ast *body;
} ptrs_function_t;

enum ptrs_structmembertype
{
	PTRS_STRUCTMEMBER_VAR,
	PTRS_STRUCTMEMBER_FUNCTION,
	PTRS_STRUCTMEMBER_ARRAY,
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
		ptrs_meta_t array;
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
typedef struct ptrs_struct
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

#define jit_const_int(func, type, val) (jit_value_create_nint_constant(func, jit_type_##type, val))
#define jit_const_long(func, type, val) (jit_value_create_long_constant(func, jit_type_##type, val))
#define jit_const_float(func, val) (jit_value_create_float64_constant(func, jit_type_float64, val))

#define ptrs_const_meta(type) ((uint64_t)(type))
#define ptrs_const_arrayMeta(size, typeIndex) \
	((uint64_t)(size) << 32 | (uint64_t)(typeIndex) << 8 | (uint64_t)(PTRS_TYPE_POINTER))
#define ptrs_const_pointerMeta(type, ptr) \
	(((uint64_t)(uintptr_t)(ptr) << 8) | (uint64_t)(type))

#define ptrs_meta_getPointer(meta) \
	(void *)(*(uint64_t *)&(meta) >> 8)
#define ptrs_meta_setPointer(meta, ptr) \
	(*(uint64_t *)&(meta) = (((uint64_t)(uintptr_t)(ptr) << 8) | (meta).type))

#define ptrs_jit_const_meta(func, type) \
	(jit_const_long(func, ulong, ptrs_const_meta(type)))
#define ptrs_jit_const_arrayMeta(func, size, typeIndex) \
	(jit_const_long(func, ulong, ptrs_const_arrayMeta((size), (typeIndex))))
#define ptrs_jit_const_pointerMeta(func, type, ptr) \
	(jit_const_long(func, ulong, ptrs_const_pointerMeta(type, ptr)))

#define ptrs_jit_arrayMeta(func, size, typeIndex) \
	(jit_insn_or(func, \
		jit_insn_or(func, \
			jit_const_long(func, ulong, PTRS_TYPE_POINTER), \
			jit_insn_shl(func, (typeIndex), jit_const_long(func, ulong, 8)) \
		), \
		jit_insn_shl(func, \
			jit_insn_convert(func, (size), jit_type_ulong, 0), \
			jit_const_long(func, ulong, 32) \
		) \
	))

#define ptrs_jit_arrayMetaKnownType(func, length, typeIndex) \
	(jit_insn_or(func, \
		jit_insn_shl(func, \
			jit_insn_convert(func, (length), jit_type_ulong, 0), \
			jit_const_long(func, ulong, 32) \
		), \
		jit_const_long(func, ulong, ((uint64_t)(typeIndex) << 8) | (uint64_t)PTRS_TYPE_POINTER) \
	))

#define ptrs_jit_pointerMeta(func, type, ptr) \
	(jit_insn_or(func, \
		jit_insn_shl(func, (ptr), jit_const_long(func, ulong, 8)), \
		(type) \
	))

#define ptrs_jit_getType(func, meta) (jit_insn_and(func, meta, jit_const_long(func, ulong, 0xFF)))
#define ptrs_jit_getArraySize(func, meta) (jit_insn_shr(func, meta, jit_const_int(func, ubyte, 32)))
#define ptrs_jit_getArrayTypeIndex(func, meta) \
	(jit_insn_and(func, \
		jit_insn_shr(func, (meta), jit_const_int(func, ubyte, 8)), \
		jit_const_long(func, ulong, 0xFF) \
	))
#define ptrs_jit_getArrayTypeSize(func, typeIndex) \
	(jit_insn_load_elem(func, \
		jit_const_int(func, void_ptr, (uintptr_t)ptrs_nativeTypes), \
		jit_insn_mul(func, (typeIndex), jit_const_long(func, ulong, sizeof(ptrs_nativetype_info_t))), \
		jit_type_ulong \
	))
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
