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
	struct ptrs_error *oldError;
	char *message;
	char *stack;
	const char *file;
	int line;
	int column;
} ptrs_error_t;

ptrs_error_t *_ptrs_error_catch(ptrs_scope_t *scope, ptrs_error_t *error, bool noReThrow);
#define ptrs_error_catch(scope, error, canBeOuter) \
	(sigsetjmp(_ptrs_error_catch(scope, error, canBeOuter)->catch, 1) != 0)

void ptrs_error_stopCatch(ptrs_scope_t *scope, ptrs_error_t *error);
void ptrs_error_reThrow(ptrs_scope_t *scope, ptrs_error_t *error);

void ptrs_showpos(FILE *fd, ptrs_ast_t *ast);
void ptrs_error(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *msg, ...);
void ptrs_handle_signals();

#endif
