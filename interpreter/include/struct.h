#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include <stdbool.h>

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, ptrs_asthandler_t handler, bool isLeftSide);
ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key);
bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope);
ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

#endif
