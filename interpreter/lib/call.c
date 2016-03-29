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
