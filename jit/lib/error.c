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
ptrs_ast_t *ptrs_lastAst = NULL;
bool ptrs_enableExceptions = false;
bool ptrs_enableSafety = true;

extern jit_context_t ptrs_jit_context;

void ptrs_getpos(ptrs_codepos_t *pos, const char *code, size_t index)
{
	int line = 1;
	int column = 1;
	const char *currLine = code;
	for(int i = 0; i < index; i++)
	{
		if(code[i] == '\n')
		{
			line++;
			column = 1;
			currLine = &(code[i + 1]);
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

	if(error->ast != NULL && error->pos.currLine != NULL)
	{
		fprintf(ptrs_errorfile, " at %s:%d:%d\n\n", error->file, error->pos.line, error->pos.column);

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

char *ptrs_backtrace(int traceSkip)
{
	int bufflen = 1024;
	char *buff = malloc(bufflen);

	char *buffptr = buff;
	buff[0] = 0;

	jit_stack_trace_t trace = jit_exception_get_stack_trace();
	int count = jit_stack_trace_get_size(trace);

	for(int i = traceSkip; i < count; i++)
	{
		if(buffptr - buff > bufflen - 256)
		{
			int diff = buffptr - buff;
			bufflen *= 2;
			buff = realloc(buff, bufflen);
			buffptr = buff + diff;
		}

		void *ptr = jit_stack_trace_get_pc(trace, i);

		jit_function_t func = jit_stack_trace_get_function(ptrs_jit_context, trace, i);
		if(func)
		{
			const char *name = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
			ptrs_ast_t *ast = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_AST);
			size_t offset = jit_stack_trace_get_offset(ptrs_jit_context, trace, i);

			if(name != NULL)
				buffptr += sprintf(buffptr, "    at %s", name);
			else
				buffptr += sprintf(buffptr, "    at %p", ptr);

			if(ast != NULL && offset != JIT_NO_OFFSET)
			{
				ptrs_codepos_t pos;
				ptrs_getpos(&pos, ast->code, offset);

				buffptr += sprintf(buffptr, " (%s:%d:%d)\n", ast->file, pos.line, pos.column);
			}
			else if(ast != NULL)
			{
				buffptr += sprintf(buffptr, " (%s)\n", ast->file);
			}
		}
		else
		{
#ifdef _GNU_SOURCE
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
				buffptr += sprintf(buffptr, "\n");
#else
			buffptr += sprintf(buffptr, "    at %p\n", ptr);
#endif
		}
	}

	jit_stack_trace_free(trace);
	return buff;
}

void *ptrs_formatErrorMsg(const char *msg, va_list ap)
{
	//special printf formats:
	//		%t for printing a type
	//		%m for printing a ptrs_meta_t
	//			native and pointer are printed as "<native|pointer>:<array size>"
	//			structs are printed as "struct:<struct name>"
	//			functions are printed as "function:<function name>"
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
			int len;
			switch(*++msg)
			{
				case 't':
					str = ptrs_typetoa(va_arg(ap, uintptr_t));
					break;
				case 'm':
					val = va_arg(ap, uintptr_t);
					ptrs_metatoa(*(ptrs_meta_t *)&val, valbuff, 32);
					str = valbuff;
					break;
				case 'v':
					val = va_arg(ap, uintptr_t);
					uintptr_t meta = va_arg(ap, uintptr_t);
					str = ptrs_vartoa(*(ptrs_val_t *)&val, *(ptrs_meta_t *)&meta, valbuff, 32).value.strval;
					break;
				default:
					;
					//TODO support float and double's
					val = va_arg(ap, uintptr_t);
					valbuff[0] = '%';
					valbuff[1] = *msg;
					valbuff[2] = 0;
					str = NULL;
					len = snprintf(NULL, 0, valbuff, val);
					break;
			}

			if(str != NULL)
				len = strlen(str);

			while(buffptr + len > buff + bufflen)
			{
				ptrdiff_t diff = buffptr - buff;
				bufflen *= 2;
				buff = realloc(buff, bufflen);
				buffptr = buff + diff;
			}

			if(str != NULL)
			{
				memcpy(buffptr, str, len + 1);
				buffptr += len;
			}
			else
			{
				buffptr += sprintf(buffptr, valbuff, val);
			}
		}
		else
		{
			*buffptr++ = *msg;
		}

		msg++;
	}

	*buffptr++ = 0;
	return buff;
}

static void _ptrs_verror(ptrs_ast_t *ast, int skipTrace, const char *msg, va_list ap)
{
	static __thread int hadError = false;

	ptrs_error_t *error = malloc(sizeof(ptrs_error_t));
	error->ast = ast;
	error->file = ast ? ast->file : "<unknown>";
	error->fileLen = strlen(error->file) + 1;
	error->message = ptrs_formatErrorMsg(msg, ap);
	error->messageLen = strlen(error->message) + 1;

	error->ast = ast;
	if(ast == NULL)
	{
		error->pos.currLine = NULL;
		error->pos.line = -1;
		error->pos.column = -1;
	}
	else
	{
		ptrs_getpos(&error->pos, ast->code, ast->codepos);
	}

	if(ptrs_enableExceptions && !hadError)
	{
		hadError = true;
		error->backtrace = ptrs_backtrace(skipTrace);
		error->backtraceLen = strlen(error->backtrace) + 1;
		hadError = false;
		jit_exception_throw(error);
	}
	else
	{
		error->backtrace = "";
		ptrs_printError(error);
		exit(EXIT_FAILURE);
	}

}
static void _ptrs_error(ptrs_ast_t *ast, int skipTrace, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	_ptrs_verror(ast, skipTrace, msg, ap);
}

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	_ptrs_verror(ast, 3, msg, ap);
}

void ptrs_handle_sig(int sig, siginfo_t *info, void *data)
{
	if(ptrs_lastAst != NULL)
	{
		fprintf(ptrs_errorfile, "Received signal %s while compiling.\n"
			"This is probably bug within PointerScript, please open an issue on github.\n",
			strsignal(sig));

		ptrs_error_t *error = malloc(sizeof(ptrs_error_t));
		error->ast = ptrs_lastAst;
		error->message = "";
		error->backtrace = ""; //TODO

		ptrs_getpos(&error->pos, ptrs_lastAst->code, ptrs_lastAst->codepos);

		ptrs_printError(error);
		exit(EXIT_FAILURE);
	}

	_ptrs_error(NULL, 5, "Received signal: %s", strsignal(sig));
}

void *ptrs_handle_exception(int type)
{
	_ptrs_error(NULL, 5, "JIT Exception: %d", type);
}

void ptrs_handle_signals(jit_function_t func)
{
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
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

struct ptrs_assertion *ptrs_jit_vassert(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t condition, size_t argCount, const char *text, va_list ap)
{
	if(!ptrs_enableSafety)
		return NULL;

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

struct ptrs_assertion *ptrs_jit_assert(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t condition, size_t argCount, const char *text, ...)
{
	va_list ap;
	va_start(ap, text);

	return ptrs_jit_vassert(ast, func, scope, condition, argCount, text, ap);
}

void ptrs_jit_appendAssert(jit_function_t func, struct ptrs_assertion *assertion, jit_value_t condition)
{
	if(ptrs_enableSafety)
		jit_insn_branch_if_not(func, condition, &assertion->label);
}

// this is used when a typing is defined by a user and a provided value has to be checked
// if it fits (i.e. if the type, the array size and/or the struct type match the expected)
void ptrs_jit_assertMetaCompatibility(jit_function_t func, struct ptrs_assertion *assertion,
	ptrs_meta_t expected, jit_value_t actual, jit_value_t actualType)
{
	if(actualType == NULL)
		actualType = ptrs_jit_getType(func, actual);

	jit_value_t expectedJit = jit_const_long(func, ulong, *(uint64_t *)&expected);

	jit_value_t condition;
	if(expected.type == PTRS_TYPE_STRUCT && ptrs_meta_getPointer(expected) != NULL)
		condition = jit_insn_eq(func, actual, expectedJit);
	else
		condition = jit_insn_eq(func, actualType, jit_const_int(func, ubyte, expected.type));

	ptrs_jit_appendAssert(func, assertion, condition);

	if((expected.type == PTRS_TYPE_NATIVE || expected.type == PTRS_TYPE_POINTER)
		&& expected.array.size != 0)
	{
		jit_value_t size = ptrs_jit_getArraySize(func, actual);
		condition = jit_insn_ge(func, size, jit_const_int(func, uint, expected.array.size));
		ptrs_jit_appendAssert(func, assertion, condition);
	}
}

ptrs_catcher_labels_t *ptrs_jit_addCatcher(ptrs_scope_t *scope)
{
	ptrs_catcher_labels_t *entry = malloc(sizeof(ptrs_catcher_labels_t));

	entry->beforeTry = jit_label_undefined;
	entry->afterTry = jit_label_undefined;
	entry->catcher = jit_label_undefined;

	entry->next = scope->tryCatches;
	scope->tryCatches = entry;

	return entry;
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

	if(scope->tryCatches != NULL)
	{
		jit_value_t exception = jit_insn_start_catcher(func);
		if(scope->tryCatchException != NULL)
			jit_insn_store(func, scope->tryCatchException, exception);

		ptrs_catcher_labels_t *curr = scope->tryCatches;
		while(curr != NULL)
		{
			jit_label_t next = jit_label_undefined;
			jit_insn_branch_if_pc_not_in_range(func, curr->beforeTry, curr->afterTry, &next);
			jit_insn_branch(func, &curr->catcher);
			jit_insn_label(func, &next);

			ptrs_catcher_labels_t *old = curr;
			curr = curr->next;
			free(old);
		}

		jit_insn_label(func, &scope->rethrowLabel);
		jit_insn_rethrow_unhandled(func);

		scope->rethrowLabel = jit_label_undefined;
		scope->tryCatches = NULL;
	}

	scope->firstAssertion = NULL;
	scope->lastAssertion = NULL;
}
