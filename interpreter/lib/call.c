#include <stdlib.h>
#include <stdint.h>

#include "../include/error.h"
#include "../../parser/common.h"

ptrs_var_t *ptrs_callfunc(ptrs_function_t *func, ptrs_var_t *result, int argc, ptrs_var_t *argv)
{
	for(int i = 0; i < argc && i < func->argc; i++)
	{
		ptrs_scope_set(func->scope, func->args[i], &argv[i]);
	}
	
	result = func->body->handler(func->body, result, func->scope);
	return result;
}

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

//see callhelper.asm
intptr_t ptrs_call_amd64ABI(void *func, int64_t *intArgv, uint8_t floatArgc, double *floatArgv);

intptr_t ptrs_callnative(ptrs_nativefunc_t func, int argc, ptrs_var_t *argv)
{
	intptr_t intArgv[6];
	uint8_t intArgc = 0;
	
	double floatArgv[8];
	uint8_t floatArgc = 0;

	for(int i = 0; i < argc; i++)
	{
		switch(argv[i].type)
		{
			case PTRS_TYPE_FLOAT:
				floatArgv[floatArgc] = argv[i].value.floatval;
				floatArgc++;
				break;
			case PTRS_TYPE_INT:
				intArgv[intArgc] = (intptr_t)argv[i].value.intval;	//this could be done by the default: case but
				intArgc++;											//just to be sure about esoteric byte aligns
				break;
			case PTRS_TYPE_POINTER:
				intArgv[intArgc] = (intptr_t)&(argv[i].value.ptrval->value);
				intArgc++;
				break;
			default:
				intArgv[intArgc] = (intptr_t)argv[i].value.strval;
				intArgc++;
				break;
		}	
	}

	return ptrs_call_amd64ABI(func, intArgv, floatArgc, floatArgv);
}
