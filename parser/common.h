#ifndef _PTRS_COMMON
#define _PTRS_COMMON

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdbool.h>

#ifndef _GNU_SOURCE
#include <alloca.h>
#define strdup(str) (strcpy(malloc(strlen(str) + 1), str))
#endif

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

typedef struct ptrs_patchlist
{
	void *patch;
	struct ptrs_patchlist *next;
} ptrs_patchlist_t;
typedef struct ptrs_scope
{
	intptr_t fpOffset;
	unsigned usedRegCount;
	uintptr_t continueLabel;
	uintptr_t breakLabel;
	ptrs_patchlist_t *continuePatches;
	ptrs_patchlist_t *breakPatches;
	struct ptrs_error *errors;
} ptrs_scope_t;

typedef struct ptrs_var *(*ptrs_nativetype_handler_t)(void *target, size_t typeSize, struct ptrs_var *value);
typedef struct ptrs_nativetype_info
{
	const char *name;
	size_t size;
	ptrs_nativetype_handler_t getHandler;
	ptrs_nativetype_handler_t setHandler;
	void *ffiType;
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
#ifdef _PTRS_PORTABLE
	void *ffiCif;
	void *nativeCbWrite;
#endif
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
	ptrs_symbol_t symbol;
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

typedef struct ptrs_var
{
	ptrs_val_t value;
	ptrs_meta_t meta;
} ptrs_var_t;

typedef struct ptrs_stackframe
{
	struct ptrs_stackframe *outer;
	ptrs_var_t variables[];
} ptrs_stackframe_t;



#define ptrs_jit_get_type(jit, result, meta) (jit_rshi_u(jit, result, meta, 54))
#define ptrs_jit_get_arraysize(jit, result, meta) (jit_andi(jit, result, meta, 0xFFFFFFFF))

#define ptrs_jit_clear_type(jit, meta) (jit_andi(jit, meta, meta, ~(0xFF << 54)))
#define ptrs_jit_clear_arraysize(jit, meta) (jit_andi(jit, meta, meta, ~0xFFFFFFFF))

#define ptrs_jit_seti_type(jit, meta, type) (jit_ori(jit, meta, meta, (uint64_t)(type) << 54))
#define ptrs_jit_seti_arraysize(jit, meta, size) (jit_ori(jit, meta, meta, size))

#define ptrs_jit_setr_type(jit, meta, type) do \
	{ \
		jit_lshi(jit, type, type, 54); \
		jit_orr(jit, meta, meta, type); \
	} while(0)
#define ptrs_jit_setr_arraysize(jit, meta, size) (jit_orr(jit, meta, meta, size))

#define ptrs_jit_load_val(jit, result, offset) (jit_ldxi(jit, result, R_FP, scope->fpOffset + offset, 8))
#define ptrs_jit_load_meta(jit, result, offset) (jit_ldxi(jit, result, R_FP, scope->fpOffset + offset + 8, 8))
#define ptrs_jit_load_type(jit, result, offset) (jit_ldxi(jit, result, R_FP, scope->fpOffset + offset + 8, 1))
#define ptrs_jit_load_arrayreadonly(jit, result, offset) (jit_ldxi(jit, result, R_FP, scope->fpOffset + offset + 9, 1))
#define ptrs_jit_load_arraysize(jit, result, offset) (jit_ldxi(jit, result, R_FP, scope->fpOffset + offset + 12, 4))

#define ptrs_jit_store_val(jit, offset, val) (jit_stxi(jit, R_FP, scope->fpOffset + offset, val, 8))
#define ptrs_jit_store_meta(jit, offset, val) (jit_stxi(jit, R_FP, scope->fpOffset + offset + 8, val, 8))
#define ptrs_jit_store_type(jit, offset, val) (jit_stxi(jit, R_FP, scope->fpOffset + offset + 8, val, 1))
#define ptrs_jit_store_arrayreadonly(jit, offset, val) (jit_stxi(jit, R_FP, scope->fpOffset + offset + 9, val, 1))
#define ptrs_jit_store_arraysize(jit, offset, val) (jit_stxi(jit, R_FP, scope->fpOffset + offset + 12, val, 4))

#endif
