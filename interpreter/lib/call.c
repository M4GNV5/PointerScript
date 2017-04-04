#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ffi.h>

#ifndef _PTRS_PORTABLE
#include <ffcb.h>
#endif

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/call.h"
#include "../include/struct.h"
#include "../include/astlist.h"
#include "../interpreter.h"
#include "../../parser/common.h"

ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_nativetype_info_t *retType, ptrs_struct_t *thisArg, ptrs_var_t *func,
	ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope)
{
	int len = ptrs_astlist_length(arguments, ast, scope);
	ptrs_var_t args[len];
	ptrs_astlist_handle(arguments, len, args, scope);

	ptrs_var_t overload;

	if(func->type == PTRS_TYPE_FUNCTION)
	{
		result = ptrs_callfunc(ast, result, scope, thisArg, func, len, args);
	}
	else if(func->type == PTRS_TYPE_NATIVE)
	{
		ptrs_callnative(retType, result, func->value.nativeval, len, args);
	}
	else if(func->type == PTRS_TYPE_STRUCT && (overload.value.funcval = ptrs_struct_getOverload(func, ptrs_handle_call)) != NULL)
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

	if((result->type == PTRS_TYPE_NATIVE || result->type == PTRS_TYPE_POINTER || result->type == PTRS_TYPE_STRUCT)
		&& result->value.nativeval > scope->bp && result->value.nativeval < scope->sp)
	{
		size_t len;
		if(result->type == PTRS_TYPE_NATIVE)
			len = result->meta.array.size;
		else if(result->type == PTRS_TYPE_POINTER)
			len = sizeof(ptrs_var_t) * result->meta.array.size;
		else
			len = sizeof(ptrs_struct_t) + result->value.structval->size;

		void *val = ptrs_alloc(callScope, len);
		memcpy(val, result->value.nativeval, len);
		result->value.nativeval = val;
	}

	return result;
}

#ifndef _PTRS_PORTABLE

#ifndef FFI_CLOSURES
#error "Your plattform is neither supported by libffcb nor by libffi closures"
#endif

//using libffcb
void callClosure(ffcb_return_t ret, ptrs_function_t *func, va_list ap)
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
void ptrs_createClosure(ptrs_function_t *func)
{
	func->nativeCb = ffcb_create(callClosure, func);
}
void ptrs_deleteClosure(ptrs_function_t *func)
{
	ffcb_delete(func->nativeCb);
}

#else

//using libffi closures
void callClosure(ffi_cif *cif, int64_t *ret, void **args, ptrs_function_t *func)
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
		argv[i].value.intval = *(int64_t *)args[i];
	}

	*ret = ptrs_vartoi(ptrs_callfunc(NULL, &result, &scope, NULL, &funcvar, func->argc, argv));

	if(scope.stackstart != NULL)
		free(scope.stackstart);
}
void ptrs_createClosure(ptrs_function_t *func)
{
	func->nativeCbWrite = ffi_closure_alloc(sizeof(ffi_closure), &func->nativeCb);
	func->ffiCif = malloc(sizeof(ffi_cif) + sizeof(ffi_type *) * func->argc);
	ffi_type **types = (void *)((uint8_t *)func->ffiCif + sizeof(ffi_cif));

	for(int i = 0; i < func->argc; i++)
		types[i] = &ffi_type_sint64;

	ffi_prep_cif(func->ffiCif, FFI_DEFAULT_ABI, func->argc, &ffi_type_sint64, types);
	ffi_prep_closure_loc(func->nativeCbWrite, func->ffiCif, (void *)callClosure, func, func->nativeCb);
}
void ptrs_deleteClosure(ptrs_function_t *func)
{
	ffi_closure_free(func->nativeCbWrite);
	free(func->ffiCif);
}

#endif

int64_t ptrs_callnative(ptrs_nativetype_info_t *retType, ptrs_var_t *result, void *func, int argc, ptrs_var_t *argv)
{
	ptrs_function_t *callback;
	bool hasCallbackArgs = false;

	ffi_cif cif;
	ffi_type *types[argc];
	void *values[argc];

	for(int i = 0; i < argc; i++)
	{
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
			case PTRS_TYPE_FUNCTION:
				callback = argv[i].value.funcval;
				if(callback->nativeCb == NULL)
				{
					ptrs_createClosure(callback);
					if(callback->scope->outer != NULL)
						hasCallbackArgs = true;
				}

				types[i] = &ffi_type_pointer;
				values[i] = &callback->nativeCb;
				break;
			default:
				types[i] = &ffi_type_pointer;
				values[i] = &argv[i].value.nativeval;
				break;
		}
	}

	int64_t retVal;
	if(retType == NULL)
	{
		ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, &ffi_type_sint64, types);
		ffi_call(&cif, func, &retVal, values);

		if(result != NULL)
		{
			result->type = PTRS_TYPE_INT;
			result->value.intval = retVal;
			memset(&result->meta, 0, sizeof(ptrs_meta_t));
		}
	}
	else
	{
		uint8_t retBuff[retType->size];
		ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, retType->ffiType, types);
		ffi_call(&cif, func, retBuff, values);

		retType->getHandler(retBuff, retType->size, result);
		retVal = 0;
	}

	if(hasCallbackArgs)
	{
		for(int i = 0; i < argc; i++)
		{
			if(argv[i].type == PTRS_TYPE_FUNCTION && argv[i].value.funcval->scope->outer != NULL)
				ptrs_deleteClosure(argv[i].value.funcval);
		}
	}

	return retVal;
}
