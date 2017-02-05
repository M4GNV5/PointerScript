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

__thread ptrs_ast_t *ptrs_lastast = NULL;
__thread ptrs_scope_t *ptrs_lastscope = NULL;
FILE *ptrs_errorfile = NULL;

#ifndef _PTRS_NOASM
#include <jitas.h>

struct ptrs_asmStatement
{
	uint8_t *start;
	uint8_t *end;
	ptrs_ast_t *ast;
	jitas_context_t *context;
	struct ptrs_asmStatement *next;
};
extern struct ptrs_asmStatement *ptrs_asmStatements;
extern uint8_t *ptrs_asmBuffStart;
extern size_t ptrs_asmSize;
#endif

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

char *ptrs_backtrace(ptrs_ast_t *pos, ptrs_scope_t *scope, int skipNative, bool gotSig)
{
	int bufflen = 1024;
	char *buff = malloc(bufflen);
	char *buffptr = buff;

#ifdef _GNU_SOURCE
	static bool hadError;
	if(hadError)
	{
		buffptr += sprintf(buffptr,
			"It appears like we raised a signal when backtracing the native stack.\n"
			"Skipping native backtrace.\n");
		hadError = false;
	}
	else
	{
		hadError = true;
		uint8_t *trace[32];
		int count = backtrace((void **)trace, 32);
		Dl_info infos[count];

		Dl_info selfInfo;
		Dl_info ffiInfo;
		dladdr(ptrs_backtrace, &selfInfo);
		dladdr(dlsym(NULL, "ffi_call"), &ffiInfo);

		for(int i = skipNative; i < count; i++)
		{
#ifndef _PTRS_NOASM
			if(trace[i] >= ptrs_asmBuffStart && trace[i] < ptrs_asmBuffStart + ptrs_asmSize)
			{
				struct ptrs_asmStatement *curr = ptrs_asmStatements;
				jitas_symboltable_t *symbol = NULL;

				while(curr != NULL)
				{
					if(trace[i] >= curr->start && trace[i] <= curr->end)
					{
						symbol = curr->context->localSymbols;
						while(symbol != NULL)
						{
							if(symbol->next == NULL || symbol->next->ptr > trace[i])
								break;

							symbol = symbol->next;
						}

						break;
					}

					curr = curr->next;
				}

				if(curr != NULL)
				{
					if(symbol == NULL)
						buffptr += sprintf(buffptr, "    at asm+%lX ", (unsigned long)(trace[i] - curr->start));
					else
						buffptr += sprintf(buffptr, "    at %s+%lX ", symbol->symbol, (unsigned long)(trace[i] - symbol->ptr));

					buffptr +=  ptrs_printpos(buffptr, curr->ast);
					continue;
				}
			}
#endif

			if(dladdr(trace[i], &infos[i]) == 0)
			{
				infos[i].dli_sname = NULL;
				infos[i].dli_fname = NULL;
			}

			if((!gotSig || i != skipNative)
				&& (infos[i].dli_fbase == selfInfo.dli_fbase || infos[i].dli_fbase == ffiInfo.dli_fbase))
				break;

			if(buffptr - buff > bufflen - 128)
			{
				int diff = buffptr - buff;
				bufflen *= 2;
				buff = realloc(buff, bufflen);
				buffptr = buff + diff;
			}

			if(infos[i].dli_sname != NULL)
				buffptr += sprintf(buffptr, "    at %s ", infos[i].dli_sname);
			else
				buffptr += sprintf(buffptr, "    at %p ", trace[i]);

			if(infos[i].dli_fname != NULL)
				buffptr += sprintf(buffptr, "(%s)\n", infos[i].dli_fname);
			else
				buffptr += sprintf(buffptr, "(unknown)\n");
		}

		hadError = false;
	}
#endif

	ptrs_scope_t *start = scope;
	while(scope != NULL)
	{
		if(buffptr - buff > bufflen - 128)
		{
			int diff = buffptr - buff;
			bufflen *= 2;
			buff = realloc(buff, bufflen);
			buffptr = buff + diff;
		}

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
	return buff;
}

void ptrs_showpos(FILE *fd, ptrs_ast_t *ast)
{
	codepos_t pos;
	ptrs_getpos(&pos, ast);

	fprintf(fd, " at %s:%d:%d\n", ast->file, pos.line, pos.column);

	int linelen = strchr(pos.currLine, '\n') - pos.currLine;
	fprintf(fd, "%.*s\n", linelen, pos.currLine);

	int linePos = (ast->code + ast->codepos) - pos.currLine;
	for(int i = 0; i < linePos; i++)
	{
		fprintf(fd, pos.currLine[i] == '\t' ? "\t" : " ");
	}
	fprintf(fd, "^\n\n");
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
	error->stack = ptrs_backtrace(ast, scope, 5, false);
	error->file = ast->file;
	error->line = pos.line;
	error->column = pos.column;

	siglongjmp(error->catch, 1);
}

void ptrs_throw(ptrs_ast_t *ast, ptrs_scope_t *scope, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	ptrs_vthrow(ast, scope, msg, ap);
}

ptrs_error_t *_ptrs_error_catch(ptrs_scope_t *scope, ptrs_error_t *error, bool noReThrow)
{
	error->oldError = scope->error;

	if(scope->error != NULL || noReThrow)
		scope->error = error;

	return error;
}
void ptrs_error_stopCatch(ptrs_scope_t *scope, ptrs_error_t *error)
{
	scope->error = error->oldError;
}
void ptrs_error_reThrow(ptrs_scope_t *scope, ptrs_error_t *error)
{
	ptrs_error_t *oldError = error->oldError;
	oldError->message = error->message;
	oldError->stack = error->stack;
	oldError->file = error->file;
	oldError->line = error->line;
	oldError->column = error->column;

	scope->error = error->oldError;
	siglongjmp(scope->error->catch, 1);
}

void ptrs_handle_sig(int sig)
{
	if(sig != SIGINT && sig != SIGTERM &&
		ptrs_lastscope != NULL && ptrs_lastscope->error != NULL)
	{
		signal(sig, ptrs_handle_sig);
		ptrs_throw(ptrs_lastast, ptrs_lastscope, "Received signal: %s", strsignal(sig));
	}

	fprintf(ptrs_errorfile, "Received signal: %s", strsignal(sig));

	if(ptrs_lastast != NULL)
		ptrs_showpos(ptrs_errorfile, ptrs_lastast);
	else
		fprintf(ptrs_errorfile, "\n");

	if(ptrs_lastscope != NULL)
		fprintf(ptrs_errorfile, "%s", ptrs_backtrace(ptrs_lastast, ptrs_lastscope, 3, true));
	exit(EXIT_FAILURE);
}

void ptrs_handle_signals()
{
	signal(SIGINT, ptrs_handle_sig);
	signal(SIGQUIT, ptrs_handle_sig);
	signal(SIGTERM, ptrs_handle_sig);
	//SIGHUP?

	signal(SIGILL, ptrs_handle_sig);
	signal(SIGABRT, ptrs_handle_sig);
	signal(SIGFPE, ptrs_handle_sig);
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

	vfprintf(ptrs_errorfile, msg, ap);
	va_end(ap);

	if(ast != NULL)
	{
		ptrs_showpos(ptrs_errorfile, ast);
		if(scope != NULL)
			fprintf(ptrs_errorfile, "%s\n", ptrs_backtrace(ast, scope, 2, false));
	}
	exit(EXIT_FAILURE);
}
