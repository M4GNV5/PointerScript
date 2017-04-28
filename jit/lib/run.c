#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/run.h"

ptrs_result_t *ptrs_compile(char *src, const char *filename)
{
	ptrs_scope_t scope;
	scope.errors = NULL;
	scope.usedRegCount = 0;

	ptrs_result_t *result = malloc(sizeof(ptrs_result_t));

	result->ast = ptrs_parse(src, filename, &result->symbols);
	result->jit = jit_init();

	jit_prolog(result->jit, &result->code);
	result->ast->handler(result->ast, result->jit, &scope);
	jit_reti(result->jit, 0);

	return result;
}

ptrs_result_t *ptrs_compilefile(const char *file)
{
	FILE *fd = fopen(file, "r");

	if(fd == NULL)
	{
		fprintf(stderr, "%s : %s\n", file, strerror(errno));
		exit(EXIT_FAILURE);
	}

	fseek(fd, 0, SEEK_END);
	long fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char *src = malloc(fsize + 1);
	fread(src, fsize, 1, fd);
	fclose(fd);
	src[fsize] = 0;

	{
		char cwd[1024];
		getcwd(cwd, 1024);

		int len = strlen(cwd);
		if(strncmp(cwd, file, len) == 0)
			file += len + 1;
	}

	return ptrs_compile(src, strdup(file));
}
