#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>
#include <jit/jit.h>

#include "../../parser/common.h"

const char *ptrs_typetoa(ptrs_vartype_t type);
int64_t ptrs_vartoi(ptrs_val_t val, ptrs_meta_t meta);
double ptrs_vartof(ptrs_val_t val, ptrs_meta_t meta);
const char *ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen);

jit_value_t ptrs_jit_vartob(jit_function_t func, jit_value_t val, jit_value_t meta);
jit_value_t ptrs_jit_vartoi(jit_function_t func, jit_value_t val, jit_value_t meta);
jit_value_t ptrs_jit_vartof(jit_function_t func, jit_value_t val, jit_value_t meta);

#endif
