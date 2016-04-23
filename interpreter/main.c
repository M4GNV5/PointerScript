#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/scope.h"
#include "include/conversion.h"
#include "include/run.h"

int main(int argc, char **argv)
{
	ptrs_var_t result;

	if(argc == 1)
	{
		ptrs_scope_t *scope = malloc(sizeof(ptrs_scope_t));
		scope->current = NULL;
		scope->outer = NULL;

		for(;;)
		{
			result.type = PTRS_TYPE_UNDEFINED;
			printf("> ");
			char buff[1024];
			fgets(buff, 1024, stdin);
			ptrs_eval(buff, &result, scope);

			printf("< %s\n", ptrs_vartoa(&result, buff, 1024));
		}
	}
	else if(argc == 2)
	{
		ptrs_dofile(argv[1], &result, NULL);
	}

	return 0;
}
