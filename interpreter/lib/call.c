#include <stdlib.h>
#include <stdint.h>

#include "../include/error.h"
#include "../../parser/common.h"

#define nativecaller_create(n, ...)\
	intptr_t ptrs_callnative_##n(ptrs_nativefunc_t func, intptr_t *args) \
	{ \
		return func(__VA_ARGS__); \
	}

nativecaller_create(0, 0)
nativecaller_create(1, args[0])
nativecaller_create(2, args[0], args[1])
nativecaller_create(3, args[0], args[1], args[2])
nativecaller_create(4, args[0], args[1], args[2], args[3])
nativecaller_create(5, args[0], args[1], args[2], args[3], args[4])
nativecaller_create(6, args[0], args[1], args[2], args[3], args[4], args[5])
nativecaller_create(7, args[0], args[1], args[2], args[3], args[4], args[5], args[6])
nativecaller_create(8, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7])
nativecaller_create(9, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8])
nativecaller_create(10, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9])


typedef intptr_t (*nativecaller_t)(ptrs_nativefunc_t, intptr_t *);
nativecaller_t caller[] = {
	ptrs_callnative_0,
	ptrs_callnative_1,
	ptrs_callnative_2,
	ptrs_callnative_3,
	ptrs_callnative_4,
	ptrs_callnative_5,
	ptrs_callnative_6,
	ptrs_callnative_7,
	ptrs_callnative_8,
	ptrs_callnative_9,
	ptrs_callnative_10,
};

intptr_t ptrs_callnative(ptrs_nativefunc_t func, int argc, ptrs_var_t *argv)
{
	intptr_t args[10];

	if(argc > 10)
		ptrs_error(NULL, "Cannot call a native method with more than 10 arguments");

	for(int i = 0; i < argc; i++)
	{
		args[i] = (intptr_t)argv[i].value.strval;
	}

	return caller[argc](func, args);
}
