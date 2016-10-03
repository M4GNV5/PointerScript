#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include <stdio.h>
#include <setjmp.h>
#include "../../parser/ast.h"
#include "../../parser/common.h"

extern __thread ptrs_ast_t *ptrs_lastast;
extern __thread ptrs_scope_t *ptrs_lastscope;

typedef struct ptrs_error
{
	sigjmp_buf catch;
	char *message;
	char *stack;
	const char *file;
	int line;
	int column;
} ptrs_error_t;

void ptrs_showpos(FILE *fd, ptrs_ast_t *ast);
void ptrs_error(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *msg, ...);
void ptrs_handle_signals();

#endif
