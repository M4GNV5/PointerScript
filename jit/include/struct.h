#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include <stdbool.h>

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler, bool isInstance);

bool ptrs_struct_canAccess(ptrs_ast_t *ast, ptrs_struct_t *struc, struct ptrs_structmember *member);

unsigned long ptrs_struct_hashName(const char *key);

struct ptrs_structmember *ptrs_struct_find(ptrs_struct_t *struc, const char *key,
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast);

ptrs_var_t ptrs_struct_getMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member);
ptrs_var_t ptrs_struct_get(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key);
ptrs_jit_var_t ptrs_jit_struct_get(jit_function_t func, ptrs_ast_t *ast, ptrs_scope_t *scope,
	jit_value_t data, ptrs_struct_t *struc, const char *key);

void ptrs_struct_setMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member, ptrs_val_t val, ptrs_meta_t meta);
void ptrs_struct_set(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key,
ptrs_val_t val, ptrs_meta_t valMeta);
void ptrs_jit_struct_set(jit_function_t func, ptrs_ast_t *ast, ptrs_scope_t *scope,
	jit_value_t data, ptrs_struct_t *struc, const char *key, ptrs_jit_var_t value);

ptrs_var_t ptrs_struct_addressOfMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member);
ptrs_var_t ptrs_struct_addressOf(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key);

ptrs_jit_var_t ptrs_jit_struct_call(jit_function_t func, ptrs_ast_t *ast,
	ptrs_scope_t *scope, jit_value_t data, ptrs_struct_t *struc,
	const char *key, ptrs_nativetype_info_t *retType, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_struct_construct(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t constructor, struct ptrs_astlist *arguments, bool allocateOnStack);

#endif
