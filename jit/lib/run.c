#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/run.h"

jit_context_t ptrs_jit_context = NULL;

ptrs_result_t *ptrs_compile(char *src, const char *filename)
{
	ptrs_scope_t scope;
	scope.continueLabel = jit_label_undefined;
	scope.breakLabel = jit_label_undefined;
	scope.errors = NULL;

	if(ptrs_jit_context == NULL)
		ptrs_jit_context = jit_context_create();

	ptrs_result_t *result = malloc(sizeof(ptrs_result_t));

	result->symbols = NULL;
	result->ast = ptrs_parse(src, filename, &result->symbols);

	jit_context_build_start(ptrs_jit_context);

	jit_type_t params[1] = {jit_type_void_ptr};
	result->signature = jit_type_create_signature(jit_abi_cdecl, jit_type_ulong, params, 1, 1);

	result->func = jit_function_create(ptrs_jit_context, result->signature);
	jit_function_set_meta(result->func, PTRS_JIT_FUNCTIONMETA_NAME, "(root)", NULL, 0);
	jit_function_set_meta(result->func, PTRS_JIT_FUNCTIONMETA_FILE, filename, NULL, 0);

	result->ast->handler(result->ast, result->func, &scope);

	jit_function_compile(result->func);

	jit_context_build_end(ptrs_jit_context);

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
