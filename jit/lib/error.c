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
#include "../include/run.h"

struct ptrs_assertion
{
	struct ptrs_assertion *next;
	jit_label_t label;
	size_t argCount;
	jit_value_t args[];
};

FILE *ptrs_errorfile = NULL;
bool ptrs_enableExceptions = false;

extern jit_context_t ptrs_jit_context;

void ptrs_getpos(ptrs_codepos_t *pos, ptrs_ast_t *ast)
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

void ptrs_printError(ptrs_error_t *error)
{
	fprintf(ptrs_errorfile, "%s", error->message);

	if(error->ast != NULL)
	{
		fprintf(ptrs_errorfile, " at %s:%d:%d\n\n", error->ast->file, error->pos.line, error->pos.column);

		int linelen = strchr(error->pos.currLine, '\n') - error->pos.currLine;
		fprintf(ptrs_errorfile, "%.*s\n", linelen, error->pos.currLine);

		int linePos = (error->ast->code + error->ast->codepos) - error->pos.currLine;
		for(int i = 0; i < linePos; i++)
		{
			fprintf(ptrs_errorfile, error->pos.currLine[i] == '\t' ? "\t" : " ");
		}

		fprintf(ptrs_errorfile, "^\n");
	}

	fprintf(ptrs_errorfile, "\n%s", error->backtrace);
}

char *ptrs_backtrace()
{
	int bufflen = 1024;
	char *buff = malloc(bufflen);

	char *buffptr = buff;
	buff[0] = 0;

	jit_stack_trace_t trace = jit_exception_get_stack_trace();
	int count = jit_stack_trace_get_size(trace);

	for(int i = 0; i < count; i++)
	{
		if(buffptr - buff > bufflen - 128)
		{
			int diff = buffptr - buff;
			bufflen *= 2;
			buff = realloc(buff, bufflen);
			buffptr = buff + diff;
		}

		jit_function_t func = jit_stack_trace_get_function(ptrs_jit_context, trace, i);
		if(func)
		{
			const char *name = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
			const char *file = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_FILE);
			buffptr += sprintf(buffptr, "    at %s (%s)\n", name, file);
		}
		else
		{
#ifdef _GNU_SOURCE
			void *ptr = jit_stack_trace_get_pc(trace, i);
			Dl_info info;

			if(dladdr(ptr, &info) == 0)
			{
				info.dli_sname = NULL;
				info.dli_fname = NULL;
			}

			if(info.dli_sname != NULL)
				buffptr += sprintf(buffptr, "    at %s ", info.dli_sname);
			else
				buffptr += sprintf(buffptr, "    at %p ", ptr);

			if(info.dli_fname != NULL)
				buffptr += sprintf(buffptr, "(%s)\n", info.dli_fname);
			else
				buffptr += sprintf(buffptr, "(unknown)\n");
#else
			buffptr += sprintf(buffptr, "    at %p (unknown)\n", jit_stack_trace_get_pc(trace, i));
#endif
		}
	}

	return buff;
}

void *ptrs_formatErrorMsg(const char *msg, va_list ap)
{
	//special printf formats:
	//		%t for printing a type
	//		%mt for printing the type stored in a ptrs_meta_t
	//		%ms for printing the array size stored in a ptrs_meta_t
	//		%v for printing a variable

	int bufflen = 1024;
	char *buff = malloc(bufflen);
	char *buffptr = buff;

	while(*msg != 0)
	{
		if(*msg == '%')
		{
			uintptr_t val;
			char valbuff[32];

			const char *str;
			switch(*++msg)
			{
				case 't':
					str = ptrs_typetoa(va_arg(ap, long));
					break;
				case 'm':
					val = va_arg(ap, uintptr_t);
					switch(*++msg)
					{
						case 't':
							str = ptrs_typetoa((*(ptrs_meta_t *)&val).type);
							break;
						case 's':
							sprintf(valbuff, "%d", (*(ptrs_meta_t *)&val).array.size);
							str = valbuff;
							break;
					}
					break;
				case 'v':
					val = va_arg(ap, uintptr_t);
					uintptr_t meta = va_arg(ap, uintptr_t);
					str = ptrs_vartoa(*(ptrs_val_t *)&val, *(ptrs_meta_t *)&meta, buff, 32);
				default:
					;
					char format[3] = {'%', *msg, 0};
					snprintf(valbuff, 32, format, va_arg(ap, long));
					str = valbuff;
			}

			int len = strlen(str);
			while(buffptr + len > buff + bufflen)
			{
				ptrdiff_t diff = buffptr - buff;
				bufflen *= 2;
				buff = realloc(buff, bufflen);
				buffptr = buff + diff;
			}

			memcpy(buffptr, str, len + 1);
			buffptr += len;
		}
		else
		{
			*buffptr++ = *msg;
		}

		msg++;
	}

	return buff;
}

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	ptrs_error_t *error = malloc(sizeof(ptrs_error_t));
	error->message = ptrs_formatErrorMsg(msg, ap);
	error->backtrace = ptrs_backtrace();

	error->ast = ast;
	if(ast == NULL)
	{
		error->pos.currLine = NULL;
		error->pos.line = -1;
		error->pos.column = -1;
	}
	else
	{
		ptrs_getpos(&error->pos, ast);
	}

	if(ptrs_enableExceptions)
		jit_exception_throw(error);

	ptrs_printError(error);
	exit(EXIT_FAILURE);
}

void ptrs_handle_sig(int sig, siginfo_t *info, void *data)
{
	ptrs_error(NULL, "Received signal: %s", strsignal(sig));
}

void *ptrs_handle_exception(int type)
{
	ptrs_error(NULL, "JIT Exception: %d", type);
}

void ptrs_handle_signals(jit_function_t func)
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

	jit_exception_set_handler(ptrs_handle_exception);
}

struct ptrs_assertion *ptrs_jit_assert(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t condition, size_t argCount, const char *text, ...)
{
	va_list ap;
	va_start(ap, text);

	argCount += 2;

	struct ptrs_assertion *assertion = malloc(sizeof(struct ptrs_assertion) + argCount * sizeof(jit_value_t));

	assertion->argCount = argCount;
	assertion->args[0] = jit_const_int(func, void_ptr, (uintptr_t)ast);
	assertion->args[1] = jit_const_int(func, void_ptr, (uintptr_t)text);

	for(size_t i = 2; i < argCount; i++)
		assertion->args[i] = va_arg(ap, jit_value_t);

	va_end(ap);

	assertion->label = jit_label_undefined;
	jit_insn_branch_if_not(func, condition, &assertion->label);

	assertion->next = NULL;
	if(scope->lastAssertion == NULL)
	{
		scope->firstAssertion = assertion;
		scope->lastAssertion = assertion;
	}
	else
	{
		scope->lastAssertion->next = assertion;
		scope->lastAssertion = assertion;
	}

	return assertion;
}

void ptrs_jit_appendAssert(jit_function_t func, struct ptrs_assertion *assertion, jit_value_t condition)
{
	jit_insn_branch_if_not(func, condition, &assertion->label);
}

void ptrs_jit_placeAssertions(jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_assertion *curr = scope->firstAssertion;
	while(curr != NULL)
	{
		jit_insn_label(func, &curr->label);

		jit_type_t *argDef = malloc(curr->argCount * sizeof(jit_type_t));
		for(int i = 0; i < curr->argCount; i++)
			argDef[i] = jit_type_void_ptr;

		jit_type_t signature = jit_type_create_signature(jit_abi_vararg, jit_type_void, argDef, curr->argCount, 1);
		jit_insn_call_native(func, "ptrs_error", ptrs_error, signature, curr->args, curr->argCount, JIT_CALL_NORETURN);

		struct ptrs_assertion *old = curr;
		curr = curr->next;

		jit_type_free(signature);
		free(argDef);
		free(old);
	}

	scope->firstAssertion = NULL;
	scope->lastAssertion = NULL;
}
