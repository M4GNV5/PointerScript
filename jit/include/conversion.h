#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>
#include <jit/jit.h>

#include "../../parser/common.h"

const char *ptrs_typetoa(ptrs_vartype_t type);
void ptrs_metatoa(ptrs_meta_t meta, char *buff, size_t maxlen);

bool ptrs_vartob(ptrs_val_t val, ptrs_meta_t meta);
int64_t ptrs_vartoi(ptrs_val_t val, ptrs_meta_t meta);
double ptrs_vartof(ptrs_val_t val, ptrs_meta_t meta);
ptrs_var_t ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen);

jit_value_t ptrs_jit_vartob(jit_function_t func, ptrs_jit_var_t val);
jit_value_t ptrs_jit_vartoi(jit_function_t func, ptrs_jit_var_t val);
jit_value_t ptrs_jit_vartof(jit_function_t func, ptrs_jit_var_t val);
ptrs_jit_var_t ptrs_jit_vartoa(jit_function_t func, ptrs_jit_var_t val);

void ptrs_jit_branch_if(jit_function_t func, jit_label_t *target, ptrs_jit_var_t val);
void ptrs_jit_branch_if_not(jit_function_t func, jit_label_t *target, ptrs_jit_var_t val);

#endif
