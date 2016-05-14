#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/scope.h"
#include "include/conversion.h"
#include "include/run.h"
#include "include/error.h"

int main(int argc, char **argv)
{
	ptrs_var_t result;
	ptrs_handle_signals();

	ptrs_scope_t *scope = calloc(1, sizeof(ptrs_scope_t));

	ptrs_var_t arguments[argc + 1];
	for(int i = 0; i < argc; i++)
	{
		arguments[i].type = PTRS_TYPE_NATIVE;
		arguments[i].value.strval = argv[i];
	}
	arguments[argc].type = PTRS_TYPE_NATIVE;
	arguments[argc].value.nativeval = NULL;

	result.type = PTRS_TYPE_POINTER;
	result.value.ptrval = arguments;
	ptrs_scope_set(scope, "arguments", &result);

	if(argc < 2)
		printf("Usage: ptrs <file> [arguments...]\n");
	else
		ptrs_dofile(argv[1], &result, scope);

	return 0;
}
