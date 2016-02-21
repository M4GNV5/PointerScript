#ifndef _PTRS_OBJECT
#define _PTRS_OBJECT

#include "../../parser/common.h"

ptrs_object_t *ptrs_object_set(ptrs_object_t *obj, char *key, ptrs_var_t *value);
ptrs_var_t *ptrs_object_get(ptrs_object_t *obj, char *key);

#endif
