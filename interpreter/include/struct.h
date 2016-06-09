#ifndef _PTRS_STRUCT
#define _PTRS_STRUCT

#include "../../parser/common.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, const char *op);
ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key);

#endif
