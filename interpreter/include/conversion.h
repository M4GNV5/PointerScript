#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>

bool ptrs_vartob(ptrs_var_t *value);
int64_t ptrs_vartoi(ptrs_var_t *value);
double ptrs_vartod(ptrs_var_t *value);
void ptrs_vartoa(ptrs_var_t *val, char *buff);

ptrs_var_t *ptrs_atovar(char *str);
ptrs_var_t *ptrs_itovar(int64_t val);
ptrs_var_t *ptrs_dtovar(double val);

const char *ptrs_typetoa(ptrs_vartype_t type);

#endif
