#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>
#include <jit/jit.h>

#include "../../parser/common.h"

void ptrs_typetoa(const char **result, ptrs_vartype_t type);
void ptrs_vartoi(int64_t *result, ptrs_val_t val, ptrs_meta_t meta);
void ptrs_vartof(double *result, ptrs_val_t val, ptrs_meta_t meta);
const char *ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen);

jit_value_t ptrs_jit_vartob(jit_function_t func, jit_value_t val, jit_value_t meta);
jit_value_t ptrs_jit_vartoi(jit_function_t func, jit_value_t val, jit_value_t meta);
jit_value_t ptrs_jit_vartof(jit_function_t func, jit_value_t val, jit_value_t meta);

#endif
