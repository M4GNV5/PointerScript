#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/scope.h"

const char *ptrs_file = NULL;

void ptrs_eval(char *src, ptrs_var_t *result, ptrs_scope_t *scope)
{
	if(scope == NULL)
		scope = calloc(1, sizeof(ptrs_scope_t));
	scope->calleeName = "(root)";

	ptrs_ast_t *ast = parse(src);
	ptrs_var_t *_result = ast->handler(ast, result, scope);

	if(_result != result)
		memcpy(result, _result, sizeof(ptrs_var_t));
}

void ptrs_dofile(const char *file, ptrs_var_t *result, ptrs_scope_t *scope)
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

	const char *oldFile = ptrs_file;
	ptrs_file = file;
	ptrs_eval(src, result, scope);
	ptrs_file = oldFile;
}
