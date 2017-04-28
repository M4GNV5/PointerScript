#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#ifdef _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#endif

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"
#include "../include/conversion.h"

typedef struct
{
	char *currLine;
	int line;
	int column;
} codepos_t;

ptrs_positionlist_t *ptrs_positions = NULL;
FILE *ptrs_errorfile = NULL;

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

ptrs_ast_t *ptrs_retrieveast(void *mem, const char **funcName)
{
	ptrs_positionlist_t *curr = ptrs_positions;
	while(curr != NULL)
	{
		if(curr->start < mem && mem < curr->end)
			break;

		curr = curr->next;
	}

	if(curr == NULL)
		return NULL;

	if(funcName != NULL)
		*funcName = curr->funcName;

	void *ptr = curr->start;
	for(int i = 0; ptr < curr->end; i++)
	{
		if(ptr >= mem)
			return curr->positions[i].ast;
		else
			ptr += curr->positions[i].size;
	}
	return NULL;
}

char *ptrs_backtrace()
{
	int bufflen = 1024;
	char *buff = malloc(bufflen);
	char *buffptr = buff;

#ifdef _GNU_SOURCE
	uint8_t *trace[32];
	int count = backtrace((void **)trace, 32);

	for(int i = 0; i < count; i++)
	{
		if(buffptr - buff > bufflen - 128)
		{
			int diff = buffptr - buff;
			bufflen *= 2;
			buff = realloc(buff, bufflen);
			buffptr = buff + diff;
		}

		const char *func;
		ptrs_ast_t *ast = ptrs_retrieveast(trace[i], &func);
		if(ast != NULL)
		{
			codepos_t pos;
			ptrs_getpos(&pos, ast);

			buffptr += sprintf(buffptr, "    at %s (%s:%d:%d)\n", func, ast->file, pos.line, pos.column);
		}
		else
		{
			Dl_info info;
			if(dladdr(trace[i], &info) == 0)
			{
				info.dli_sname = NULL;
				info.dli_fname = NULL;
			}

			if(info.dli_sname != NULL)
				buffptr += sprintf(buffptr, "    at %s ", info.dli_sname);
			else
				buffptr += sprintf(buffptr, "    at %p ", trace[i]);

			if(info.dli_fname != NULL)
				buffptr += sprintf(buffptr, "(%s)\n", info.dli_fname);
			else
				buffptr += sprintf(buffptr, "(unknown)\n");
		}

	}
#else
	sprintf(buff, "Backtraces are only available on GNU systems\n");
#endif

	return buff;
}

void ptrs_handle_sig(int sig, siginfo_t *info, void *data)
{
	//TODO allow catching errors

	fprintf(ptrs_errorfile, "Received signal: %s", strsignal(sig));
	ptrs_ast_t *ast = ptrs_retrieveast(info->si_addr, NULL);

	if(ast != NULL)
		ptrs_showpos(ptrs_errorfile, ast);
	else
		fprintf(ptrs_errorfile, "\n");

	fprintf(ptrs_errorfile, "%s", ptrs_backtrace());
	exit(EXIT_FAILURE);
}

void ptrs_error_init(jit_state_t *jit)
{
	struct sigaction action;
	action.sa_sigaction = ptrs_handle_sig;
	action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	//SIGHUP?

	sigaction(SIGILL, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
	sigaction(SIGFPE, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGPIPE, &action, NULL);
}

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...)
{
	//TODO allow catching errors
	//TODO implement special printf formats
	//		%t for printing a type
	//		%mt for printing the type stored in a ptrs_meta_t
	//		%ms for printing the array size stored in a ptrs_meta_t
	//		%v for printing a variable

	va_list ap;
	va_start(ap, msg);
	vfprintf(ptrs_errorfile, msg, ap);

	if(ast != NULL)
		ptrs_showpos(ptrs_errorfile, ast);

	fprintf(ptrs_errorfile, "%s", ptrs_backtrace());
	exit(EXIT_FAILURE);
}

ptrs_error_t *ptrs_jit_addError(ptrs_ast_t *ast, ptrs_scope_t *scope, jit_op *jump,
	size_t argCount, const char *text, ...)
{
	long *args;
	if(argCount > 0)
	{
		va_list ap;
		va_start(ap, text);

		args = malloc(sizeof(long) * argCount);
		for(size_t i = 0; i < argCount; i++)
			args[i] = va_arg(ap, long);

		va_end(ap);
	}
	else
	{
		args = NULL;
	}

	ptrs_error_t *error = malloc(sizeof(ptrs_error_t));
	error->ast = ast;
	error->jump = jump;
	error->text = text;
	error->argCount = argCount;
	error->args = args;

	error->next = scope->errors;
	scope->errors = error;
	return error;
}

void ptrs_jit_compileErrors(jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_error_t *curr = scope->errors;
	while(curr != NULL)
	{
		jit_prepare(jit);

		jit_putargi(jit, (uintptr_t)curr->ast);
		for(size_t i = 0; i < curr->argCount; i++)
			jit_putargr(jit, curr->args[i]);

		jit_call(jit, ptrs_error);

		ptrs_error_t *next = curr->next;
		free(curr);
		curr = next;
	}
}
