#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include <stdbool.h>

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler);

bool ptrs_struct_canAccess(ptrs_struct_t *struc, struct ptrs_structmember *member, ptrs_ast_t *node);

unsigned long ptrs_struct_hashName(const char *key);

struct ptrs_structmember *ptrs_struct_find(ptrs_struct_t *struc, const char *key,
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast);

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result,
	struct ptrs_structmember *member, ptrs_ast_t *ast);
ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast);

void ptrs_struct_setMember(ptrs_struct_t *struc, ptrs_var_t *value,
	struct ptrs_structmember *member, ptrs_ast_t *ast);
bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast);

void ptrs_struct_addressOfMember(ptrs_struct_t *struc, ptrs_var_t *result,
	struct ptrs_structmember *member, ptrs_ast_t *ast);
bool ptrs_struct_addressOf(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast);

ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result);

#endif
