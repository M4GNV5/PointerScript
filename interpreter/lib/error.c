#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>

#include "../include/run.h"
#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/call.h"

ptrs_ast_t *ptrs_lastast = NULL;
ptrs_scope_t *ptrs_lastscope = NULL;

typedef struct codepos
{
	const char *file;
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

	pos->file = ptrs_file;
	pos->currLine = currLine;
	pos->line = line;
	pos->column = column;
}

void ptrs_printpos(ptrs_ast_t *ast)
{
	codepos_t pos;
	ptrs_getpos(&pos, ast);
	fprintf(stderr, "(%s:%d:%d)\n", pos.file, pos.line, pos.column);
}

void ptrs_showpos(ptrs_ast_t *ast)
{
	codepos_t pos;
	ptrs_getpos(&pos, ast);

	int linelen = strchr(pos.currLine, '\n') - pos.currLine;
	fprintf(stderr, "\n%.*s\n", linelen, pos.currLine);

	int linePos = (ast->code + ast->codepos) - pos.currLine;
	for(int i = 0; i < linePos; i++)
	{
		fprintf(stderr, pos.currLine[i] == '\t' ? "\t" : " ");
	}
	fprintf(stderr, "^\n\n");
}

void ptrs_printstack(ptrs_ast_t *pos, ptrs_scope_t *scope, bool gotSig)
{
	ptrs_showpos(pos);

#ifdef _GNU_SOURCE
	void *buff[32];
	int count = backtrace(buff, 32);
	Dl_info infos[count];

	for(int i = 0; i < count; i++)
	{
		dladdr(buff[i], &infos[i]);
		if(infos[i].dli_saddr == ptrs_callnative)
		{
			for(int j = gotSig ? 3 : 2; j < i - 2; j++)
			{
				if(infos[j].dli_sname != NULL && infos[j].dli_fname != NULL)
					fprintf(stderr, "    at %s (%s)\n", infos[j].dli_sname, infos[j].dli_fname);
				else
					fprintf(stderr, "    at %p (unknown)\n", buff[j]);
			}
			break;
		}
	}
#endif

	ptrs_scope_t *start = scope;
	while(scope != NULL)
	{
		if(scope == start)
			fprintf(stderr, "    >> ");
		else
			fprintf(stderr, "    at ");
		fprintf(stderr, "%s ", scope->calleeName);
		if(pos != NULL)
			ptrs_printpos(pos);
		else
			fprintf(stderr, "\n");

		pos = scope->callAst;
		scope = scope->callScope;
	}
}

void ptrs_handle_sig(int sig)
{
	fprintf(stderr, "Received signal: %s", strsignal(sig));
	ptrs_printstack(ptrs_lastast, ptrs_lastscope, true);
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
	vfprintf(stderr, msg, ap);
	va_end(ap);

	if(ast == NULL)
		ast = ptrs_lastast;
	if(scope == NULL)
		scope = ptrs_lastscope;
	ptrs_printstack(ast, scope, false);
	exit(3);
}
