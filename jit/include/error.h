#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include <stdio.h>
#include <setjmp.h>
#include "../../parser/ast.h"
#include "../../parser/common.h"

extern FILE *ptrs_errorfile;
extern ptrs_ast_t *ptrs_lastAst;
extern bool ptrs_enableExceptions;
extern bool ptrs_enableSafety;

typedef struct
{
	const char *currLine;
	int line;
	int column;
} ptrs_codepos_t;

typedef struct ptrs_error
{
	const char *message;
	char *backtrace;
	const char *file;
	int messageLen;
	int backtraceLen;
	int fileLen;
	ptrs_ast_t *ast;
	ptrs_codepos_t pos;
} ptrs_error_t;

typedef struct ptrs_catcher_labels
{
	struct ptrs_catcher_labels *next;
	jit_label_t beforeTry;
	jit_label_t afterTry;
	jit_label_t catcher;
} ptrs_catcher_labels_t;

void ptrs_getpos(ptrs_codepos_t *pos, const char *code, size_t index);

ptrs_error_t *ptrs_createError(ptrs_ast_t *ast, int skipTrace, const char *message, bool dupMessage);

void ptrs_handle_signals();
void ptrs_printError(ptrs_error_t *error);
void ptrs_error(ptrs_ast_t *ast, const char *msg, ...);

struct ptrs_assertion *ptrs_jit_vassert(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t condition, size_t argCount, const char *text, va_list ap);
struct ptrs_assertion *ptrs_jit_assert(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t condition, size_t argCount, const char *text, ...);
void ptrs_jit_assertMetaCompatibility(jit_function_t func, struct ptrs_assertion *assertion,
	ptrs_meta_t expected, jit_value_t actual, jit_value_t actualType);
void ptrs_jit_appendAssert(jit_function_t func, struct ptrs_assertion *assert, jit_value_t condition);
ptrs_catcher_labels_t *ptrs_jit_addCatcher(ptrs_scope_t *scope);
void ptrs_jit_placeAssertions(jit_function_t func, ptrs_scope_t *scope);

#endif
