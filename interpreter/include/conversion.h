#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>

bool ptrs_vartob(ptrs_var_t *value);
int64_t ptrs_vartoi(ptrs_var_t *value);
double ptrs_vartof(ptrs_var_t *value);
const char *ptrs_vartoa(ptrs_var_t *val, char *buff, size_t maxlen);

const char *ptrs_typetoa(ptrs_vartype_t type);

#endif
