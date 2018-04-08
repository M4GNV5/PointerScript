#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/run.h"
#include "../include/error.h"
#include "../include/util.h"
#include "../include/call.h"
#include "../include/flow.h"

jit_context_t ptrs_jit_context = NULL;
bool ptrs_compileAot = true;
bool ptrs_analyzeFlow = true;

void ptrs_compile(ptrs_result_t *result, char *src, const char *filename)
{
	ptrs_scope_t scope;
	ptrs_initScope(&scope, NULL);

	if(ptrs_jit_context == NULL)
		ptrs_jit_context = jit_context_create();

	result->symbols = NULL;
	result->ast = ptrs_parse(src, filename, &result->symbols);

	if(ptrs_analyzeFlow)
		ptrs_flow_analyze(result->ast);

	jit_context_build_start(ptrs_jit_context);

	static jit_type_t rootSignature = NULL;
	if(rootSignature == NULL)
	{
		jit_type_t params[1] = {jit_type_void_ptr};
		rootSignature = jit_type_create_signature(jit_abi_cdecl, ptrs_jit_getVarType(), params, 1, 1);
	}

	result->func = ptrs_jit_createFunction(result->ast, NULL, rootSignature, "(root)");

	scope.rootFunc = result->func;
	scope.rootFrame = &result->funcFrame;
	jit_insn_store_relative(result->func,
		jit_const_int(result->func, void_ptr, (uintptr_t)scope.rootFrame), 0,
		jit_insn_get_frame_pointer(result->func)
	);

	result->ast->handler(result->ast, result->func, &scope);

	jit_insn_return_struct_from_values(result->func,
		jit_const_int(result->func, long, 0),
		ptrs_jit_const_meta(result->func, PTRS_TYPE_UNDEFINED)
	);

	ptrs_jit_placeAssertions(result->func, &scope);

	if(ptrs_compileAot && jit_function_compile(result->func) == 0)
		ptrs_error(result->ast, "Failed compiling the root function");

	jit_context_build_end(ptrs_jit_context);
}

void ptrs_compilefile(ptrs_result_t *result, const char *file)
{
	char *src = ptrs_readFile(file);

	if(src == NULL)
	{
		fprintf(stderr, "%s : %s\n", file, strerror(errno));
		exit(EXIT_FAILURE);
	}

	{
		char cwd[1024];
		getcwd(cwd, 1024);

		int len = strlen(cwd);
		if(strncmp(cwd, file, len) == 0)
			file += len + 1;
	}

	return ptrs_compile(result, src, strdup(file));
}
