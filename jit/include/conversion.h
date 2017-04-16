#ifndef _PTRS_CONVERSION
#define _PTRS_CONVERSION

#include <stdint.h>
#include <stdbool.h>

bool ptrs_vartob(ptrs_val_t val, ptrs_meta_t meta);
int64_t ptrs_vartoi(ptrs_val_t val, ptrs_meta_t meta);
double ptrs_vartof(ptrs_val_t val, ptrs_meta_t meta);
const char *ptrs_vartoa(ptrs_val_t val, ptrs_meta_t meta, char *buff, size_t maxlen);

#define ptrs_jit_convert(jit, method, result, val, meta) \
	{ \
		jit_prepare(jit); \
		jit_putargr(jit, val); \
		jit_putargr(jit, meta); \
		jit_call(jit, method); \
		jit_retval(jit, result); \
	}


const char *ptrs_typetoa(ptrs_vartype_t type);

#endif