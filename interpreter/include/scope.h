#ifndef _PTRS_SCOPE
#define _PTRS_SCOPE

#include <stdbool.h>
#include "../../parser/common.h"

ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, const char *name);
void ptrs_scope_set(ptrs_scope_t *scope, const char *name, ptrs_var_t *value);

#endif
