#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"

typedef struct
{
	char *currLine;
	int line;
	int column;
} codepos_t;

ptrs_positionlist_t *ptrs_positions = NULL;
FILE *ptrs_errorfile = NULL;
uintptr_t ptrs_jiterror;

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

ptrs_ast_t *ptrs_retrieveast(void *mem)
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

char *ptrs_backtrace(int skipNative, bool gotSig)
{
	int bufflen = 1024;
	char *buff = malloc(bufflen);
	char *buffptr = buff;

#ifdef _GNU_SOURCE
	uint8_t *trace[32];
	int count = backtrace((void **)trace, 32);
	Dl_info infos[count];

	Dl_info selfInfo;
	Dl_info ffiInfo;
	dladdr(ptrs_backtrace, &selfInfo);
	dladdr(dlsym(NULL, "ffi_call"), &ffiInfo);

	for(int i = skipNative; i < count; i++)
	{
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
#else
	sprintf(buff, "Backtraces are only available on GNU systems\n");
#endif

	return buff;
}

void ptrs_handle_sig(int sig, siginfo_t *info, void *data)
{
	fprintf(ptrs_errorfile, "Received signal: %s", strsignal(sig));
	ptrs_ast_t *ast = ptrs_retrieveast(info->si_addr);

	if(ast != NULL)
		ptrs_showpos(ptrs_errorfile, ast);
	else
		fprintf(ptrs_errorfile, "\n");

	fprintf(ptrs_errorfile, "%s", ptrs_backtrace(3, true));
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

	ptrs_jiterror = (uintptr_t)jit_get_label(jit);
	//TODO print some sort of error message
	//TODO allow catching errors
	jit_prepare(jit);
	jit_call(jit, abort);
}

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(ptrs_errorfile, msg, ap);

	if(ast != NULL)
		ptrs_showpos(ptrs_errorfile, ast);
	exit(EXIT_FAILURE);
}
