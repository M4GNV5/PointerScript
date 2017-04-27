#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include <stdio.h>
#include <setjmp.h>
#include "../../parser/ast.h"
#include "../../parser/common.h"

extern FILE *ptrs_errorfile;

struct ptrs_knownpos
{
	size_t size;
	ptrs_ast_t *ast;
};
typedef struct ptrs_positionlist
{
	void *start;
	void *end;
	const char *funcName;
	struct ptrs_knownpos *positions;
	struct ptrs_positionlist *next;
} ptrs_positionlist_t;

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...);
void ptrs_handle_signals();

typedef struct ptrs_error
{
	ptrs_ast_t *ast;
	jit_op *jump;
	struct ptrs_error *next;

	const char *text;
	size_t argCount;
	long *args;
} ptrs_error_t;

ptrs_error_t *ptrs_jit_addError(ptrs_ast_t *ast, ptrs_scope_t *scope, jit_op *jump,
	size_t argCount, const char *text, ...);
void ptrs_jit_compileErrors(jit_state_t *jit, ptrs_scope_t *scope);

#endif
