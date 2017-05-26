#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include <stdio.h>
#include <setjmp.h>
#include "../../parser/ast.h"
#include "../../parser/common.h"

extern FILE *ptrs_errorfile;

typedef struct ptrs_error
{
	char *message;
	char *backtrace;
	const char *file;
	int line;
	int column;
} ptrs_error_t;

void ptrs_handle_signals();

void ptrs_jit_assert(ptrs_ast_t *ast, jit_function_t func, jit_value_t condition,
	size_t argCount, const char *text, ...);

#endif
