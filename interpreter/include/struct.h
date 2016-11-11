#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include <stdbool.h>

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler, bool isLeftSide);

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structlist *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope);
ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope);

void ptrs_struct_setMember(ptrs_struct_t *struc, ptrs_var_t *value, struct ptrs_structlist *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope);
bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope);

void ptrs_struct_addressOfMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structlist *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope);
bool ptrs_struct_addressOf(ptrs_struct_t *struc, ptrs_var_t *result, const char *key,
	ptrs_ast_t *ast, ptrs_scope_t *scope);

ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

#endif
