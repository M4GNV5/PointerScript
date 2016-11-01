#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ffi.h>

#ifndef _PTRS_NOCALLBACK
#include <ffcb.h>
#endif

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/call.h"
#include "../include/struct.h"
#include "../include/astlist.h"
#include "../interpreter.h"
#include "../../parser/common.h"

ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_vartype_t retType, ptrs_struct_t *thisArg, ptrs_var_t *func,
	ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope)
{
	int len = ptrs_astlist_length(arguments, ast, scope);
	ptrs_var_t args[len];
	ptrs_astlist_handle(arguments, args, scope);

	ptrs_var_t overload;

	if(func->type == PTRS_TYPE_FUNCTION)
	{
		result = ptrs_callfunc(ast, result, scope, thisArg, func, len, args);
	}
	else if(func->type == PTRS_TYPE_NATIVE)
	{
		result->type = retType;
		result->value = ptrs_callnative(retType, func->value.nativeval, len, args);
		memset(&result->meta, 0, sizeof(ptrs_meta_t));
	}
	else if(func->type == PTRS_TYPE_STRUCT && (overload.value.funcval = ptrs_struct_getOverload(func, ptrs_handle_call, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		result = ptrs_callfunc(ast, result, scope, func->value.structval, &overload, len, args);
	}
	else
	{
		ptrs_error(ast, scope, "Cannot call value of type %s", ptrs_typetoa(func->type));
	}

	return result;
}

ptrs_var_t *ptrs_callfunc(ptrs_ast_t *callAst, ptrs_var_t *result, ptrs_scope_t *callScope,
	ptrs_struct_t *thisArg, ptrs_var_t *funcvar, int argc, ptrs_var_t *argv)
{
	ptrs_function_t *func = funcvar->value.funcval;
	ptrs_scope_t *scope = ptrs_scope_increase(callScope, func->stackOffset);
	scope->outer = func->scope;
	scope->callScope = callScope;
	scope->callAst = callAst;
	scope->calleeName = func->name;

	ptrs_var_t val;
	if(thisArg != NULL)
	{
		val.type = PTRS_TYPE_STRUCT;
		val.value.structval = thisArg;
		ptrs_scope_set(scope, ptrs_thisSymbol, &val);
	}

	val.type = PTRS_TYPE_UNDEFINED;
	for(int i = 0; i < func->argc; i++)
	{
		if(i < argc && func->args[i].scope == (unsigned)-1)
			continue;
		else if(i < argc && argv[i].type != PTRS_TYPE_UNDEFINED)
			ptrs_scope_set(scope, func->args[i], &argv[i]);
		else if(func->argv != NULL && func->argv[i] != NULL)
			ptrs_scope_set(scope, func->args[i], func->argv[i]->handler(func->argv[i], result, scope));
		else
			ptrs_scope_set(scope, func->args[i], &val);
	}

	if(func->vararg.scope != (unsigned)-1)
	{
		val.type = PTRS_TYPE_POINTER;
		val.value.ptrval = &argv[func->argc];
		val.meta.array.size = argc - func->argc;
		ptrs_scope_set(scope, func->vararg, &val);
	}

	ptrs_var_t *_result = func->body->handler(func->body, result, scope);

	if(scope->exit != 3)
		result->type = PTRS_TYPE_UNDEFINED;
	else if(result != _result)
		memcpy(result, _result, sizeof(ptrs_var_t));

	if((result->type == PTRS_TYPE_NATIVE || result->type == PTRS_TYPE_STRUCT)
		&& result->value.nativeval > scope->stackstart && result->value.nativeval < scope->sp)
	{
		size_t len;
		if(result->type == PTRS_TYPE_NATIVE)
			len = strlen(result->value.nativeval) + 1;
		else
			len = sizeof(ptrs_struct_t) + result->value.structval->size;

		void *val = ptrs_alloc(callScope, len);
		memcpy(val, result->value.nativeval, len);
		result->value.nativeval = val;
	}

	return result;
}

#ifndef _PTRS_NOCALLBACK
void ptrs_callcallback(ffcb_return_t ret, ptrs_function_t *func, va_list ap)
{
	ptrs_var_t argv[func->argc];

	ptrs_var_t result;
	ptrs_var_t funcvar;
	funcvar.type = PTRS_TYPE_FUNCTION;
	funcvar.value.funcval = func;

	ptrs_scope_t scope;
	memset(&scope, 0, sizeof(ptrs_scope_t));
	scope.calleeName = "(native callback)";

	for(int i = 0; i < func->argc; i++)
	{
		argv[i].type = PTRS_TYPE_INT;
		argv[i].value.intval = va_arg(ap, intptr_t);
	}

	ptrs_callfunc(NULL, &result, &scope, NULL, &funcvar, func->argc, argv);

	if(scope.stackstart != NULL)
		free(scope.stackstart);

	switch(result.type)
	{
		case PTRS_TYPE_UNDEFINED:
			break;
		case PTRS_TYPE_INT:
			ffcb_return_int(ret, result.value.intval);
			break;
		case PTRS_TYPE_FLOAT:
			ffcb_return_float(ret, result.value.floatval);
			break;
		default: //pointer type
			ffcb_return_pointer(ret, result.value.nativeval);
			break;
	}
}
#endif

ptrs_val_t ptrs_callnative(ptrs_vartype_t retType, void *func, int argc, ptrs_var_t *argv)
{
	ptrs_val_t retVal;
	ptrs_function_t *callback;

	ffi_cif cif;
	ffi_type *types[argc];
	void *values[argc];

#ifndef _PTRS_NOCALLBACK
	bool hasCallbackArgs = false;
	void *callbackArgs[argc];
#endif

	for(int i = 0; i < argc; i++)
	{
#ifndef _PTRS_NOCALLBACK
		callbackArgs[i] = NULL;
#endif

		switch(argv[i].type)
		{
			case PTRS_TYPE_FLOAT:
				types[i] = &ffi_type_double;
				values[i] = &argv[i].value.floatval;
				break;
			case PTRS_TYPE_INT:
				types[i] = &ffi_type_sint64;
				values[i] = &argv[i].value.intval;
				break;
			case PTRS_TYPE_STRUCT:
				types[i] = &ffi_type_pointer;
				values[i] = &argv[i].value.structval->data;
				break;
#ifndef _PTRS_NOCALLBACK
			case PTRS_TYPE_FUNCTION:
				callback = argv[i].value.funcval;
				types[i] = &ffi_type_pointer;

				if(callback->scope->outer == NULL)
				{
					if(callback->nativeCb == NULL)
						callback->nativeCb = ffcb_create(&ptrs_callcallback, callback);

					values[i] = &callback->nativeCb;
				}
				else
				{
					hasCallbackArgs = true;
					callbackArgs[i] = ffcb_create(&ptrs_callcallback, callback);
					values[i] = &callbackArgs[i];
				}

				break;
#endif
			default:
				types[i] = &ffi_type_pointer;
				values[i] = &argv[i].value.nativeval;
				break;
		}
	}

	switch(retType)
	{
		case PTRS_TYPE_INT:
			ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, &ffi_type_sint64, types);
			break;
		case PTRS_TYPE_FLOAT:
			ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, &ffi_type_double, types);
			break;
		default:
			ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, &ffi_type_pointer, types);
			break;
	}
	ffi_call(&cif, func, &retVal, values);

#ifndef _PTRS_NOCALLBACK
	if(hasCallbackArgs)
	{
		for(int i = 0; i < argc; i++)
		{
			if(callbackArgs[i] != NULL)
				ffcb_delete(callbackArgs[i]);
		}
	}
#endif

	return retVal;
}
