#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <setjmp.h>
#include <dlfcn.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "interpreter.h"
#include "include/error.h"
#include "include/debug.h"
#include "include/conversion.h"
#include "include/stack.h"
#include "include/scope.h"
#include "include/run.h"
#include "include/call.h"
#include "include/struct.h"

ptrs_var_t *ptrs_handle_body(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	ptrs_var_t *_result = result;

	scope->exit = 0;
	while(list)
	{
		ptrs_lastast = list->entry;
		ptrs_lastscope = scope;
		if(ptrs_debugEnabled)
			ptrs_debug_update(list->entry, scope);

		_result = list->entry->handler(list->entry, result, scope);

		if(scope->exit != 0)
			return _result;

		list = list->next;
	}
	return _result;
}

ptrs_var_t *ptrs_handle_define(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define stmt = node->arg.define;

	if(stmt.value != NULL)
		result = stmt.value->handler(stmt.value, result, scope);
	else
		result->type = PTRS_TYPE_UNDEFINED;

	ptrs_scope_set(scope, stmt.symbol, result);

	return result;
}

size_t ptrs_arraymax = PTRS_STACK_SIZE;
ptrs_var_t *ptrs_handle_array(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define stmt = node->arg.define;

	int size = -1;
	if(stmt.value != NULL)
	{
		ptrs_var_t *val = stmt.value->handler(stmt.value, result, scope);
		size = ptrs_vartoi(val);

		if(size <= 0 || size > ptrs_arraymax)
			ptrs_error(node, scope, "Trying to create array of size %d", size);
	}

	if(stmt.initVal != NULL)
	{
		int len = ptrs_astlist_length(stmt.initVal, node, scope);

		if(size < 0)
			size = len;
		else if(size < len)
			ptrs_error(node, scope, "Array size (%d) is too small for initializer size (%d)", size, len);

		uint8_t *array = ptrs_alloc(scope, size);

		ptrs_var_t vals[len];
		ptrs_astlist_handle(stmt.initVal, vals, scope);
		for(int i = 0; i < len; i++)
		{
			array[i] = ptrs_vartoi(&vals[i]);
		}
		result->value.nativeval = array;
	}
	else if(stmt.initExpr != NULL)
	{
		ptrs_var_t *val = stmt.initExpr->handler(stmt.initExpr, result, scope);
		char buff[32];
		const char *initVal = ptrs_vartoa(val, buff, 32);
		int len = strlen(initVal);

		if(size < 0)
			size = len + 1;
		else if(size < len)
			ptrs_error(node, scope, "Array size (%d) is too small for initializer size (%d)", size, len);

		result->value.nativeval = ptrs_alloc(scope, size);
		strncpy(result->value.nativeval, initVal, size - 1);
		((uint8_t *)result->value.nativeval)[size - 1] = 0;
	}
	else
	{
		if(size <= 0 || size > ptrs_arraymax)
			ptrs_error(node, scope, "Trying to create array of size %d", size);
		result->value.nativeval = ptrs_alloc(scope, size);
	}

	result->type = PTRS_TYPE_NATIVE;
	result->meta.array.readOnly = false;
	result->meta.array.size = size;
	ptrs_scope_set(scope, stmt.symbol, result);
	return result;
}

ptrs_var_t *ptrs_handle_vararray(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define stmt = node->arg.define;

	int size = -1;
	if(stmt.value != NULL)
	{
		ptrs_var_t *val = stmt.value->handler(stmt.value, result, scope);
		size = ptrs_vartoi(val) * sizeof(ptrs_var_t);

		if(size <= 0 || size > ptrs_arraymax)
			ptrs_error(node, scope, "Trying to create array of size %d", size);
	}

	if(stmt.initVal != NULL)
	{
		int len = ptrs_astlist_length(stmt.initVal, node, scope);

		if(size < 0)
			size = len * sizeof(ptrs_var_t);
		else if(size < len)
			ptrs_error(node, scope, "Array size (%d) is too small for initializer size (%d)", size, len);

		result->value.ptrval = ptrs_alloc(scope, size);
		ptrs_astlist_handle(stmt.initVal, result->value.ptrval, scope);
	}
	else
	{
		if(size <= 0 || size > ptrs_arraymax)
			ptrs_error(node, scope, "Trying to create array of size %d", size);
		result->value.ptrval = ptrs_alloc(scope, size);
	}

	result->type = PTRS_TYPE_POINTER;
	result->meta.array.size = size / sizeof(ptrs_var_t);
	ptrs_scope_set(scope, stmt.symbol, result);
	return result;
}

ptrs_var_t *ptrs_handle_structvar(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define stmt = node->arg.define;
	result = ptrs_struct_construct(stmt.value->handler(stmt.value, result, scope),
		stmt.initVal, true, node, result, scope);

	ptrs_scope_set(scope, stmt.symbol, result);
	return result;
}

void importNative(const char *from, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	struct ptrs_ast_import stmt = node->arg.import;
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		handle = dlopen(from, RTLD_LAZY);

		error = dlerror();
		if(error != NULL)
			ptrs_error(stmt.from, scope, "%s", error);
	}

	for(int i = 0; i < stmt.count; i++)
	{
		ptrs_var_t func;
		func.type = PTRS_TYPE_NATIVE;
		func.value.nativeval = dlsym(handle, stmt.fields[i]);
		func.meta.array.readOnly = true;

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, scope, "%s", error);

		ptrs_scope_set(scope, stmt.symbols[i], &func);
	}
}

typedef struct ptrs_cache
{
	const char *path;
	ptrs_scope_t *scope;
	ptrs_symboltable_t *symbols;
	struct ptrs_cache *next;
} ptrs_cache_t;
ptrs_cache_t *ptrs_cache = NULL;
ptrs_cache_t *importCachedScript(char *path, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	ptrs_cache_t *cache = ptrs_cache;
	while(cache != NULL)
	{
		if(strcmp(cache->path, path) == 0)
		{
			free(path);
			return cache;
		}
		cache = cache->next;
	}

	ptrs_scope_t *_scope = calloc(1, sizeof(ptrs_scope_t));
	ptrs_symboltable_t *symbols;
	ptrs_var_t valuev;
	ptrs_dofile(path, &valuev, _scope, &symbols);

	cache = malloc(sizeof(ptrs_cache_t));
	cache->path = path;
	cache->scope = _scope;
	cache->symbols = symbols;
	cache->next = ptrs_cache;
	ptrs_cache = cache;
	return cache;
}

char *resolveRelPath(ptrs_ast_t *node, ptrs_scope_t *scope, const char *path)
{
	char *fullPath;
	if(path[0] != '/')
	{
		char dirbuff[strlen(node->file) + 1];
		strcpy(dirbuff, node->file);
		char *dir = dirname(dirbuff);

		char buff[strlen(dir) + strlen(path) + 2];
		sprintf(buff, "%s/%s", dir, path);

		fullPath = realpath(buff, NULL);
	}
	else
	{
		fullPath = realpath(path, NULL);
	}

	if(fullPath == NULL)
		ptrs_error(node, scope, "Could not resolve path '%s'", path);

	return fullPath;
}

void importScript(const char *from, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	struct ptrs_ast_import stmt = node->arg.import;

	char *file = resolveRelPath(node, scope, from);
	ptrs_cache_t *cache = importCachedScript(file, node, scope);

	for(int i = 0; i < stmt.count; i++)
	{
		ptrs_ast_t *ast;
		if(ptrs_ast_getSymbol(cache->symbols, stmt.fields[i], &ast) != 0)
			ptrs_error(node, scope, "Script '%s' has no property '%s'", file, stmt.fields[i]);

		ptrs_var_t valv;
		ptrs_var_t *val = ast->handler(ast, &valv, cache->scope);
		ptrs_scope_set(scope, stmt.symbols[i], val);
	}
}

ptrs_var_t *ptrs_handle_import(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	char buff[128];
	const char *name;
	struct ptrs_ast_import import = node->arg.import;

	if(import.from != NULL)
	{
		ptrs_var_t *from = import.from->handler(import.from, result, scope);
		name = ptrs_vartoa(from, buff, 128);

		char *ending = strrchr(name, '.');
		if(strcmp(ending, ".ptrs") == 0)
			importScript(name, node, scope);
		else
			importNative(name, node, scope);
	}
	else
	{
		importNative(NULL, node, scope);
	}

	return result;
}

ptrs_var_t *ptrs_handle_return(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *ast = node->arg.astval;
	if(ast == NULL)
	{
		result->type = PTRS_TYPE_UNDEFINED;
	}
	else
	{
		ptrs_var_t *val = ast->handler(ast, result, scope);

		if(val != result)
			memcpy(result, val, sizeof(ptrs_var_t));
	}

	scope->exit = 3;
	return result;
}

ptrs_var_t *ptrs_handle_break(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	scope->exit = 2;
	return result;
}

ptrs_var_t *ptrs_handle_continue(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	scope->exit = 1;
	return result;
}

ptrs_var_t *ptrs_handle_delete(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *ast = node->arg.astval;
	ptrs_var_t *val = ast->handler(ast, result, scope);

	if(val->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot delete value of type %s", ptrs_typetoa(val->type));

	ptrs_var_t overload;
	if((overload.value.funcval = ptrs_struct_getOverload(val, ptrs_handle_delete, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		result = ptrs_callfunc(node, result, scope, val->value.structval, &overload, 0, NULL);
	}

	if(!val->value.structval->isOnStack)
		free(val->value.structval);
	return result;
}

ptrs_var_t *ptrs_handle_throw(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *arg;
	ptrs_ast_t *ast = node->arg.astval;
	arg = ast->handler(ast, result, scope);

	char buff[32];
	const char *msg = ptrs_vartoa(arg, buff, 32);
	ptrs_error(node, scope, "%s", msg);
	return arg; //doh
}

ptrs_var_t *ptrs_handle_trycatch(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_trycatch stmt = node->arg.trycatch;
	ptrs_error_t error;
	ptrs_error_t *oldError = scope->error;

	scope->error = &error;
	int k = sigsetjmp(error.catch, 1);

	if(k == 0)
	{
		result = stmt.tryBody->handler(stmt.tryBody, result, scope);
		scope->error = oldError;
	}
	else if(stmt.catchBody != NULL)
	{
		ptrs_var_t val;
		char *msg;
		scope->error = oldError;

		val.type = PTRS_TYPE_NATIVE;
		val.meta.array.readOnly = false;
		if(stmt.argc > 0 && stmt.args[0].scope != (unsigned)-1)
		{
			msg = ptrs_alloc(scope, strlen(error.message) + 1);
			strcpy(msg, error.message);
			val.value.strval = msg;
			ptrs_scope_set(scope, stmt.args[0], &val);
		}
		if(stmt.argc > 1 && stmt.args[1].scope != (unsigned)-1)
		{
			msg = ptrs_alloc(scope, strlen(error.stack) + 1);
			strcpy(msg, error.stack);
			val.value.strval = msg;
			ptrs_scope_set(scope, stmt.args[1], &val);
		}
		if(stmt.argc > 2 && stmt.args[2].scope != (unsigned)-1)
		{
			val.value.strval = error.file;
			ptrs_scope_set(scope, stmt.args[2], &val);
		}
		free(error.message);
		free(error.stack);

		val.type = PTRS_TYPE_INT;
		if(stmt.argc > 3 && stmt.args[3].scope != (unsigned)-1)
		{
			val.value.intval = error.line;
			ptrs_scope_set(scope, stmt.args[3], &val);
		}
		if(stmt.argc > 4 && stmt.args[4].scope != (unsigned)-1)
		{
			val.value.intval = error.column;
			ptrs_scope_set(scope, stmt.args[4], &val);
		}

		result = stmt.catchBody->handler(stmt.catchBody, result, scope);
	}

	return result;
}

ptrs_var_t *ptrs_handle_function(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_function astfunc = node->arg.function;

	ptrs_function_t *func = ptrs_alloc(scope, sizeof(ptrs_function_t));
	func->stackOffset = astfunc.stackOffset;
	func->name = astfunc.name;
	func->vararg = astfunc.vararg;
	func->argc = astfunc.argc;
	func->args = astfunc.args;
	func->argv = astfunc.argv;
	func->body = astfunc.body;
	func->scope = scope;
	func->nativeCb = NULL;

	result->type = PTRS_TYPE_FUNCTION;
	result->value.funcval = func;

	if(!astfunc.isAnonymous) //lambda
		ptrs_scope_set(scope, astfunc.symbol, result);
	return result;
}

ptrs_var_t *ptrs_handle_struct(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_STRUCT;
	ptrs_struct_t *struc = &node->arg.structval;
	struc->scope = scope;
	result->value.structval = struc;

	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(curr->type == PTRS_STRUCTMEMBER_FUNCTION
			|| curr->type == PTRS_STRUCTMEMBER_GETTER
			|| curr->type == PTRS_STRUCTMEMBER_SETTER)
			curr->value.function->scope = scope;
		curr = curr->next;
	}

	struct ptrs_opoverload *currop = struc->overloads;
	while(currop != NULL)
	{
		currop->handler->scope = scope;
		currop = currop->next;
	}

	ptrs_scope_set(scope, struc->symbol, result);
	return result;
}

ptrs_var_t *ptrs_handle_if(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse stmt = node->arg.ifelse;

	ptrs_var_t *condition = stmt.condition->handler(stmt.condition, result, scope);

	if(ptrs_vartob(condition))
		result = stmt.ifBody->handler(stmt.ifBody, result, scope);
	else if(stmt.elseBody != NULL)
		result = stmt.elseBody->handler(stmt.elseBody, result, scope);

	return result;
}

ptrs_var_t *ptrs_handle_switch(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_switch stmt = node->arg.switchcase;
	int64_t val = ptrs_vartoi(stmt.condition->handler(stmt.condition, result, scope));

	struct ptrs_ast_case *curr = stmt.cases;
	while(curr != NULL)
	{
		if(curr->value == val)
			return curr->body->handler(curr->body, result, scope);

		curr = curr->next;
	}

	if(stmt.defaultCase != NULL)
		return stmt.defaultCase->handler(stmt.defaultCase, result, scope);

	result->type = PTRS_TYPE_UNDEFINED;
	return result;
}

ptrs_var_t *ptrs_handle_while(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;
	ptrs_var_t *_result = result;

	struct ptrs_ast_control stmt = node->arg.control;

	for(;;)
	{
		condition = stmt.condition->handler(stmt.condition, &conditionv, scope);
		if(!ptrs_vartob(condition))
			break;

		result = _result;
		result = stmt.body->handler(stmt.body, result, scope);

		if(scope->exit > 1)
		{
			if(scope->exit != 3)
				scope->exit = 0;
			return result;
		}
	}

	return result;
}

ptrs_var_t *ptrs_handle_dowhile(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;
	ptrs_var_t *_result = result;

	struct ptrs_ast_control stmt = node->arg.control;

	do
	{
		result = _result;
		result = stmt.body->handler(stmt.body, result, scope);

		if(scope->exit > 1)
		{
			if(scope->exit != 3)
				scope->exit = 0;
			return result;
		}

		condition = stmt.condition->handler(stmt.condition, &conditionv, scope);
	} while(ptrs_vartob(condition));

	return result;
}

ptrs_var_t *ptrs_handle_for(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;

	struct ptrs_ast_for stmt = node->arg.forstatement;
	stmt.init->handler(stmt.init, result, scope);

	for(;;)
	{
		condition = stmt.condition->handler(stmt.condition, &conditionv, scope);
		if(!ptrs_vartob(condition))
			break;

		stmt.body->handler(stmt.body, result, scope);

		if(scope->exit > 1)
		{
			if(scope->exit != 3)
				scope->exit = 0;
			return result;
		}

		stmt.step->handler(stmt.step, result, scope);
	}

	return result;
}

ptrs_var_t __thread ptrs_forinOverloadResult;
ptrs_var_t *ptrs_handle_forin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_forin stmt = node->arg.forin;
	ptrs_var_t valuev;
	ptrs_var_t *val = stmt.value->handler(stmt.value, &valuev, ptrs_scope_increase(scope, 0));

	ptrs_var_t overload;
	if(val->type == PTRS_TYPE_STRUCT && (overload.value.funcval = ptrs_struct_getOverload(val, ptrs_handle_forin, true)) != NULL)
	{
		void *yieldVal[2];
		yieldVal[0] = &node->arg.forin;
		yieldVal[1] = (void*)scope;
		
		ptrs_var_t arg;
		arg.type = PTRS_TYPE_NATIVE;
		arg.value.nativeval = yieldVal;

		overload.type = PTRS_TYPE_FUNCTION;
		val = ptrs_callfunc(node, result, scope, val->value.structval, &overload, 1, &arg);

		if(scope->exit == 3)
			memcpy(result, &ptrs_forinOverloadResult, sizeof(ptrs_var_t));
		return result;
	}
	else if(val->type == PTRS_TYPE_POINTER || val->type == PTRS_TYPE_NATIVE)
	{
		ptrs_scope_t *stmtScope = ptrs_scope_increase(scope, stmt.stackOffset);
		ptrs_var_t *indexvar = ptrs_scope_get(stmtScope, stmt.varsymbols[0]);
		ptrs_var_t *valvar = NULL;
		if(stmt.varcount > 1)
			valvar = ptrs_scope_get(stmtScope, stmt.varsymbols[1]);

		int len = val->meta.array.size;
		if(len == 0 && val->type == PTRS_TYPE_NATIVE)
			len = strlen(val->value.strval);

		for(int i = 0; i < len; i++)
		{
			indexvar->type = PTRS_TYPE_INT;
			indexvar->value.intval = i;

			if(valvar != NULL && val->type == PTRS_TYPE_NATIVE)
			{
				valvar->type = PTRS_TYPE_INT;
				valvar->value.intval = val->value.strval[i];
			}
			else if(valvar != NULL)
			{
				memcpy(valvar, val->value.ptrval + i, sizeof(ptrs_var_t));
			}

			stmt.body->handler(stmt.body, result, stmtScope);

			if(stmtScope->exit == 3)
				scope->exit = 3;
			if(stmtScope->exit > 1)
				return result;
		}
		return result;
	}
	else if(val->type == PTRS_TYPE_STRUCT)
	{
		ptrs_scope_t *stmtScope = ptrs_scope_increase(scope, stmt.stackOffset);
		ptrs_var_t *keyvar = ptrs_scope_get(stmtScope, stmt.varsymbols[0]);
		ptrs_var_t *valvar;
		if(stmt.varcount > 1)
			valvar = ptrs_scope_get(stmtScope, stmt.varsymbols[1]);
		else
			valvar = NULL;

		struct ptrs_structlist *curr = val->value.structval->member;
		while(curr != NULL)
		{
			if(curr->isPrivate)
				goto skip_curr;

			keyvar->type = PTRS_TYPE_NATIVE;
			keyvar->value.strval = curr->name;
			keyvar->meta.array.readOnly = true;

			if(valvar != NULL)
			{
				ptrs_var_t *_valvar = ptrs_struct_getMember(val->value.structval, valvar, curr, node, scope);
				if(_valvar == NULL)
					goto skip_curr;
				if(_valvar != valvar)
					memcpy(valvar, _valvar, sizeof(ptrs_var_t));
			}

			stmt.body->handler(stmt.body, result, stmtScope);

			if(stmtScope->exit == 3)
				scope->exit = 3;
			if(stmtScope->exit > 1)
				return result;

			skip_curr:
			curr = curr->next;
		}

		return result;
	}
	else
	{
		ptrs_error(stmt.value, scope, "Cannot iterate over variable of type %s", ptrs_typetoa(val->type));
		return result; //doh
	}
}

ptrs_var_t *ptrs_handle_file(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_alloc(scope, 0);
	scope->sp = scope->bp + node->arg.scopestatement.stackOffset;
	ptrs_ast_t *body = node->arg.scopestatement.body;
	return body->handler(body, result, scope);
}

ptrs_var_t *ptrs_handle_scopestatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.scopestatement.body;
	return body->handler(body, result, scope);
}

ptrs_var_t *ptrs_handle_exprstatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr == NULL)
		result->type = PTRS_TYPE_UNDEFINED;
	else
		result = expr->handler(expr, result, scope);
	return result;
}
