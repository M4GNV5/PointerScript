#ifndef _PTRS_RUN
#define _PTRS_RUN

#include "../../parser/common.h"

typedef struct
{
	ptrs_ast_t *ast;
	ptrs_symboltable_t *symbols;
	jit_function_t func;
	void *funcFrame;
} ptrs_result_t;

extern jit_context_t ptrs_jit_context;
extern bool ptrs_compileAot;
extern bool ptrs_analyzeFlow;

void ptrs_compile(ptrs_result_t *result, char *src, const char *filename);
void ptrs_compilefile(ptrs_result_t *result, const char *file);

#endif
