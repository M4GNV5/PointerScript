#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ffi.h>

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/stack.h"
#include "../include/call.h"
#include "../../parser/common.h"

ptrs_var_t *ptrs_call(ptrs_ast_t *ast, ptrs_var_t *func, ptrs_var_t *result, struct ptrs_astlist *arguments, ptrs_scope_t *scope)
{
	int len = 0;
	struct ptrs_astlist *list = arguments;
	while(list)
	{
		len++;
		list = list->next;
	}

	ptrs_var_t *arg;
	ptrs_var_t args[len + 1];
	list = arguments;
	for(int i = 0; i < len; i++)
	{
		arg = list->entry->handler(list->entry, &args[i], scope);

		if(arg != &args[i])
			memcpy(&args[i], arg, sizeof(ptrs_var_t));

		list = list->next;
	}
	args[len].type = PTRS_TYPE_NATIVE;
	args[len].value.nativeval = NULL;

	if(func->type == PTRS_TYPE_FUNCTION)
	{
		result = ptrs_callfunc(ast, result, scope, func, len, args);
	}
	else if(func->type == PTRS_TYPE_NATIVE)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = ptrs_callnative(ast, func->value.nativeval, len, args);
	}
	else
	{
		ptrs_error(ast, "Cannot call value of type %s", ptrs_typetoa(func->type));
	}

	return result;
}

typedef struct bundle
{
	ptrs_var_t func;
	ptrs_var_t result;
	ptrs_var_t arg;
	ptrs_scope_t scope;
} ptrs_bundle_t;

ptrs_bundle_t *ptrs_bundle(ptrs_var_t *func, ptrs_var_t *arg)
{
	ptrs_bundle_t *bundle = malloc(sizeof(ptrs_bundle_t));
	memcpy(&bundle->func, func, sizeof(ptrs_var_t));
	memcpy(&bundle->arg, arg, sizeof(ptrs_var_t));
	memset(&bundle->scope, 0, sizeof(ptrs_scope_t));
	return bundle;
}

ptrs_var_t *ptrs_callbundle(ptrs_bundle_t *bundle)
{
	ptrs_var_t *result = ptrs_callfunc(NULL, &bundle->result, &bundle->scope, &bundle->func, 1, &bundle->arg);
	if(result != &bundle->result)
		memcpy(&bundle->result, result, sizeof(ptrs_var_t));

	return &bundle->result;
}

ptrs_var_t *ptrs_callfunc(ptrs_ast_t *callAst, ptrs_var_t *result, ptrs_scope_t *callScope, ptrs_var_t *funcvar, int argc, ptrs_var_t *argv)
{
	ptrs_function_t *func = funcvar->value.funcval;
	ptrs_scope_t *scope = ptrs_scope_increase(callScope);
	scope->outer = func->scope;
	scope->callScope = callScope;
	scope->callAst = callAst;
	scope->calleeName = func->name;

	ptrs_var_t val;
	val.type = PTRS_TYPE_UNDEFINED;
	for(int i = 0; i < func->argc; i++)
	{
		if(i < argc)
			ptrs_scope_set(scope, func->args[i], &argv[i]);
		else
			ptrs_scope_set(scope, func->args[i], &val);
	}

	if(funcvar->meta.this != NULL)
	{
		val.type = PTRS_TYPE_STRUCT;
		val.value.structval = funcvar->meta.this;
		ptrs_scope_set(scope, "this", &val);
	}

	val.type = PTRS_TYPE_POINTER;
	val.value.ptrval = argv;
	ptrs_scope_set(scope, "arguments", &val);

	ptrs_var_t *_result = func->body->handler(func->body, result, scope);

	if(scope->exit != 3)
		result->type = PTRS_TYPE_UNDEFINED;
	else
		result = _result;

	return result;
}

intptr_t ptrs_callnative(ptrs_ast_t *ast, void *func, int argc, ptrs_var_t *argv)
{
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
			default:
				types[i] = &ffi_type_pointer;
				values[i] = &argv[i].value.strval;
				break;
		}
	}

	if(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, &ffi_type_pointer, types) != FFI_OK)
		ptrs_error(ast, "Could not call native function %p", func);

	int64_t retVal = 0;
	ffi_call(&cif, func, &retVal, values);
	return retVal;
}
