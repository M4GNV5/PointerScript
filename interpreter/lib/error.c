#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <setjmp.h>
#include <dlfcn.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/call.h"
#include "../include/error.h"

ptrs_ast_t *ptrs_lastast = NULL;
ptrs_scope_t *ptrs_lastscope = NULL;

typedef struct codepos
{
	char *currLine;
	int line;
	int column;
} codepos_t;

void ptrs_getpos(codepos_t *pos, ptrs_ast_t *ast)
{
	int line = 1;
	int column = 1;
	char *currLine = ast->code;
	for(int i = 0; i < ast->codepos; i++)
	{
		if(ast->code[i] == '\n')
		{
			line++;
			column = 1;
			currLine = &(ast->code[i + 1]);
		}
		else
		{
			column++;
		}
	}

	pos->currLine = currLine;
	pos->line = line;
	pos->column = column;
}

int ptrs_printpos(char *buff, ptrs_ast_t *ast)
{
	codepos_t pos;
	ptrs_getpos(&pos, ast);
	return sprintf(buff, "(%s:%d:%d)\n", ast->file, pos.line, pos.column);
}

#ifdef _GNU_SOURCE
extern int main(int argc, char **argv);
#endif

char *ptrs_backtrace(ptrs_ast_t *pos, ptrs_scope_t *scope, int skipNative)
{
	char buff[1024];
	char *buffptr = buff;

#ifdef _GNU_SOURCE
	void *trace[32];
	int count = backtrace(trace, 32);
	Dl_info infos[count];

	Dl_info selfInfo;
	dladdr(main, &selfInfo); 

	for(int i = skipNative; i < count - 1; i++)
	{
		dladdr(trace[i], &infos[i]);

		if(infos[i].dli_fbase == selfInfo.dli_fbase)
			break;

		if(infos[i].dli_sname != NULL)
			buffptr += sprintf(buffptr, "    at %s ", infos[i].dli_sname);
		else
			buffptr += sprintf(buffptr, "    at %p ", trace[i]);

		if(infos[i].dli_fname != NULL)
			buffptr += sprintf(buffptr, "(%s)\n", infos[i].dli_fname);
		else
			buffptr += sprintf(buffptr, "(unknown)\n");
	}
#endif

	ptrs_scope_t *start = scope;
	while(scope != NULL)
	{
		if(scope == start)
			buffptr += sprintf(buffptr, "    >> ");
		else
			buffptr += sprintf(buffptr, "    at ");
		buffptr += sprintf(buffptr, "%s ", scope->calleeName);
		if(pos != NULL)
			buffptr += ptrs_printpos(buffptr, pos);
		else
			*buffptr++ = '\n';

		pos = scope->callAst;
		scope = scope->callScope;
	}
	return strdup(buff);
}

void ptrs_showpos(ptrs_ast_t *ast)
{
	codepos_t pos;
	ptrs_getpos(&pos, ast);

	fprintf(stderr, " at %s:%d:%d\n", ast->file, pos.line, pos.column);

	int linelen = strchr(pos.currLine, '\n') - pos.currLine;
	fprintf(stderr, "%.*s\n", linelen, pos.currLine);

	int linePos = (ast->code + ast->codepos) - pos.currLine;
	for(int i = 0; i < linePos; i++)
	{
		fprintf(stderr, pos.currLine[i] == '\t' ? "\t" : " ");
	}
	fprintf(stderr, "^\n\n");
}

void ptrs_vthrow(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *format, va_list ap)
{
	ptrs_error_t *error = scope->error;

	va_list ap2;
	va_copy(ap2, ap);
	int msglen = vsnprintf(NULL, 0, format, ap2);
	va_end(ap2);

	char *msg = malloc(msglen + 1);
	vsprintf(msg, format, ap);
	va_end(ap);

	codepos_t pos;
	ptrs_getpos(&pos, ast);

	error->message = msg;
	error->stack = ptrs_backtrace(ast, scope, 5);
	error->file = ast->file;
	error->line = pos.line;
	error->column = pos.column;

	while(scope->error == error)
	{
		scope->error = error;
		if(scope->callScope != NULL && scope->callScope->error == error)
			scope = scope->callScope;
		else
			scope = scope->outer;
	}

	siglongjmp(error->catch, 1);
}

void ptrs_throw(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	ptrs_vthrow(ast, scope, msg, ap);
}

void ptrs_handle_sig(int sig)
{
	if(ptrs_lastscope != NULL && ptrs_lastscope->error != NULL)
	{
		signal(sig, ptrs_handle_sig);
		ptrs_throw(ptrs_lastast, ptrs_lastscope, "Received signal: %s", strsignal(sig));
	}

	fprintf(stderr, "Received signal: %s", strsignal(sig));
	ptrs_showpos(ptrs_lastast);
	fprintf(stderr, "%s", ptrs_backtrace(ptrs_lastast, ptrs_lastscope, 3));
	exit(3);
}

void ptrs_handle_signals()
{
	signal(SIGINT, ptrs_handle_sig);
	signal(SIGQUIT, ptrs_handle_sig);
	signal(SIGILL, ptrs_handle_sig);
	signal(SIGABRT, ptrs_handle_sig);
	signal(SIGFPE, ptrs_handle_sig);
	signal(SIGKILL, ptrs_handle_sig);
	signal(SIGSEGV, ptrs_handle_sig);
	signal(SIGPIPE, ptrs_handle_sig);
}

void ptrs_error(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(ast == NULL)
		ast = ptrs_lastast;
	if(scope == NULL)
		scope = ptrs_lastscope;

	if(scope != NULL && scope->error != NULL)
		ptrs_vthrow(ast, scope, msg, ap);

	vfprintf(stderr, msg, ap);
	va_end(ap);

	if(ast != NULL)
	{
		ptrs_showpos(ast);
		if(scope != NULL)
			fprintf(stderr, "%s\n", ptrs_backtrace(ast, scope, 2));
	}
	exit(3);
}
