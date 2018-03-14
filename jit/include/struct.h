#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include <stdbool.h>

#include "../../parser/common.h"

jit_function_t ptrs_struct_getOverload(ptrs_struct_t *struc, void *handler, bool isInstance);
void *ptrs_struct_getOverloadClosure(ptrs_struct_t *struc, void *handler, bool isInstance);

bool ptrs_struct_canAccess(ptrs_ast_t *ast, ptrs_struct_t *struc, struct ptrs_structmember *member);

size_t ptrs_struct_hashName(const char *key);

struct ptrs_structmember *ptrs_struct_find(ptrs_struct_t *struc, const char *key,
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast);
bool ptrs_struct_hasKey(void *data, ptrs_struct_t *struc,
	const char *key, ptrs_meta_t keyMeta, ptrs_ast_t *ast);

ptrs_var_t ptrs_struct_getMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member);
ptrs_var_t ptrs_struct_get(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key);
ptrs_jit_var_t ptrs_jit_struct_get(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t key);

void ptrs_struct_setMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member, ptrs_val_t val, ptrs_meta_t meta);
void ptrs_struct_set(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key,
ptrs_val_t val, ptrs_meta_t valMeta);
void ptrs_jit_struct_set(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t key, ptrs_jit_var_t value);

ptrs_var_t ptrs_struct_addressOfMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member);
ptrs_var_t ptrs_struct_addressOf(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key);
ptrs_jit_var_t ptrs_jit_struct_addressof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal);

ptrs_jit_var_t ptrs_jit_struct_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal, ptrs_nativetype_info_t *retType, struct ptrs_astlist *args);

ptrs_jit_var_t ptrs_struct_construct(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t constructor, struct ptrs_astlist *arguments, bool allocateOnStack);

#endif
