#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/scope.h"
#include "include/conversion.h"

void ptrs_eval(char *src, ptrs_var_t *result, ptrs_scope_t *scope)
{
	if(scope == NULL)
	{
		scope = malloc(sizeof(ptrs_scope_t));
		scope->current = NULL;
		scope->outer = NULL;
	}

	ptrs_ast_t *ast = parse(src);
	ptrs_var_t *_result = ast->handler(ast, result, scope);

	if(_result != result)
		memcpy(result, _result, sizeof(ptrs_var_t));
}

void ptrs_dofile(char *file, ptrs_var_t *result, ptrs_scope_t *scope)
{
	FILE *fd = fopen(file, "r");

	if(fd == NULL)
	{
		fprintf(stderr, "Failed to open file %s\n", file);
		exit(2);
	}

	fseek(fd, 0, SEEK_END);
	long fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char *src = malloc(fsize + 1);
	fread(src, fsize, 1, fd);
	fclose(fd);
	src[fsize] = 0;

	ptrs_eval(src, result, scope);
}

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

			ptrs_vartoa(&result, buff, 1024);
			printf("< %s\n", buff);
		}
	}
	else if(argc == 2)
	{
		ptrs_dofile(argv[1], &result, NULL);
	}

	return 0;
}
