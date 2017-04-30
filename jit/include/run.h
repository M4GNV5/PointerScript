#ifndef _PTRS_RUN
#define _PTRS_RUN

#include "../../parser/common.h"

typedef struct
{
	ptrs_ast_t *ast;
	ptrs_symboltable_t *symbols;
	jit_state_t *jit;
	void (*code)();
} ptrs_result_t;

ptrs_result_t *ptrs_compile(char *src, const char *filename);
ptrs_result_t *ptrs_compilefile(const char *file);

#endif