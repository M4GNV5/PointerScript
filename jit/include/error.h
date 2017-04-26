#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include <stdio.h>
#include <setjmp.h>
#include "../../parser/ast.h"
#include "../../parser/common.h"

extern FILE *ptrs_errorfile;
extern uintptr_t ptrs_jiterror;

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

#endif
