#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dlfcn.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/stack.h"
#include "include/scope.h"
#include "include/run.h"

ptrs_var_t *ptrs_handle_body(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	ptrs_var_t *_result;

	while(list)
	{
		ptrs_lastast = list->entry;
		ptrs_lastscope = scope;
		_result = list->entry->handler(list->entry, result, scope);

		if(scope->exit != 0)
		{
			if(scope->exit == 1)
				scope->exit = 0;
			return _result;
		}

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

	ptrs_scope_set(scope, stmt.name, result);

	return result;
}

ptrs_var_t *ptrs_handle_array(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define stmt = node->arg.define;

	ptrs_var_t *val = stmt.value->handler(stmt.value, result, scope);
	int size = ptrs_vartoi(val);

	if(size <= 0 || size > PTRS_STACK_SIZE)
		ptrs_error(node, scope, "Trying to create array of size %d", size);

	if(size % sizeof(ptrs_var_t) == 0)
		result->type = PTRS_TYPE_POINTER;
	else
		result->type = PTRS_TYPE_NATIVE;

	result->value.nativeval = ptrs_alloc(scope, size);
	ptrs_scope_set(scope, stmt.name, result);
	return result;
}

void importNative(const char *from, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t *value;
	char namebuff[128];
	const char *name;
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		handle = dlopen(from, RTLD_LAZY);

		error = dlerror();
		if(error != NULL)
			ptrs_error(node->arg.import.from, scope, "%s", error);
	}

	struct ptrs_astlist *list = node->arg.import.fields;
	while(list != NULL)
	{
		value = list->entry->handler(list->entry, &valuev, scope);
		name = ptrs_vartoa(value, namebuff, 128);

		ptrs_var_t func;
		func.type = PTRS_TYPE_NATIVE;
		func.value.nativeval = dlsym(handle, name);

		error = dlerror();
		if(error != NULL)
			ptrs_error(list->entry, scope, "%s", error);

		if(name == namebuff)
		{
			size_t len = strlen(name) + 1;
			char *_name = ptrs_alloc(scope, len);
			memcpy(_name, name, len);
			name = _name;
		}

		ptrs_scope_set(scope, name, &func);

		list = list->next;
	}
}

typedef struct ptrs_cache
{
	const char *path;
	ptrs_scope_t *scope;
	struct ptrs_cache *next;
} ptrs_cache_t;
ptrs_cache_t *ptrs_cache = NULL;
ptrs_scope_t *importCachedScript(const char *path, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	char buff[1024];
	if(realpath(path, buff) == NULL)
		ptrs_error(node, scope, "Could not resolve path '%s'", path);

	ptrs_cache_t *cache = ptrs_cache;
	while(cache != NULL)
	{
		if(strcmp(cache->path, buff) == 0)
			return cache->scope;
		cache = cache->next;
	}

	ptrs_scope_t *_scope = calloc(1, sizeof(ptrs_scope_t));
	ptrs_var_t valuev;
	ptrs_dofile(buff, &valuev, _scope);

	cache = malloc(sizeof(ptrs_cache_t));
	cache->path = strdup(buff);
	cache->scope = _scope;
	cache->next = ptrs_cache;
	ptrs_cache = cache;
	return _scope;
}

void importScript(const char *from, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t *value;
	char namebuff[1024];
	const char *name;

	if(from[0] != '/')
	{
		char dirbuff[1024];
		strcpy(dirbuff, node->file);
		char *dir = dirname(dirbuff);
		snprintf(namebuff, 1024, "%s/%s", dir, from);
		from = namebuff;
	}
	ptrs_scope_t *_scope = importCachedScript(from, node, scope);

	struct ptrs_astlist *list = node->arg.import.fields;
	while(list != NULL)
	{
		value = list->entry->handler(list->entry, &valuev, scope);
		name = ptrs_vartoa(value, namebuff, 1024);

		ptrs_var_t *val = ptrs_scope_get(_scope, name);
		if(val == NULL)
			ptrs_error(list->entry, scope, "Script '%s' has no property '%s'", from, name);

		if(name == namebuff)
		{
			size_t len = strlen(name) + 1;
			char *_name = ptrs_alloc(scope, len);
			memcpy(_name, name, len);
			name = _name;
		}

		ptrs_scope_set(scope, name, val);

		list = list->next;
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
	ptrs_var_t *val = ast->handler(ast, result, scope);

	if(val != result)
		memcpy(result, val, sizeof(ptrs_var_t));

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

ptrs_var_t *ptrs_handle_function(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_function astfunc = node->arg.function;

	ptrs_function_t *func = ptrs_alloc(scope, sizeof(ptrs_function_t));
	func->name = astfunc.name;
	func->argc = astfunc.argc;
	func->args = astfunc.args;
	func->body = astfunc.body;
	func->scope = scope;

	result->type = PTRS_TYPE_FUNCTION;
	result->value.funcval = func;
	result->meta.this = NULL;

	ptrs_scope_set(scope, astfunc.name, result);
	return result;
}

ptrs_var_t *ptrs_handle_struct(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_STRUCT;
	ptrs_struct_t *struc = &node->arg.structval;
	result->value.structval = struc;

	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(curr->function != NULL)
			curr->function->scope = scope;
		curr = curr->next;
	}

	if(struc->constructor != NULL)
		struc->constructor->scope = scope;

	ptrs_scope_set(scope, struc->name, result);
	return result;
}

ptrs_var_t *ptrs_handle_if(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse stmt = node->arg.ifelse;

	result = stmt.condition->handler(stmt.condition, result, scope);
	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope);

	if(ptrs_vartob(result))
		result = stmt.ifBody->handler(stmt.ifBody, result, stmtScope);
	else if(stmt.elseBody != NULL)
		result = stmt.elseBody->handler(stmt.elseBody, result, stmtScope);

	scope->exit = stmtScope->exit;

	return result;
}

ptrs_var_t *ptrs_handle_while(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;
	ptrs_var_t *_result = result;

	struct ptrs_ast_control stmt = node->arg.control;
	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope);

	for(;;)
	{
		condition = stmt.condition->handler(stmt.condition, &conditionv, scope);
		if(!ptrs_vartob(condition))
			break;

		result = _result;
		result = stmt.body->handler(stmt.body, result, stmtScope);

		if(stmtScope->exit == 3)
			scope->exit = 3;
		if(stmtScope->exit != 0)
			return result;
	}

	return result;
}

ptrs_var_t *ptrs_handle_dowhile(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;
	ptrs_var_t *_result = result;

	struct ptrs_ast_control stmt = node->arg.control;
	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope);

	do
	{
		result = _result;
		result = stmt.body->handler(stmt.body, result, stmtScope);

		if(stmtScope->exit == 3)
			scope->exit = 3;
		if(stmtScope->exit != 0)
			return result;

		condition = stmt.condition->handler(stmt.condition, &conditionv, stmtScope);
	} while(ptrs_vartob(condition));

	return result;
}

ptrs_var_t *ptrs_handle_for(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t conditionv;
	ptrs_var_t *condition;

	struct ptrs_ast_for stmt = node->arg.forstatement;
	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope);

	stmt.init->handler(stmt.init, result, stmtScope);

	for(;;)
	{
		condition = stmt.condition->handler(stmt.condition, &conditionv, stmtScope);
		if(!ptrs_vartob(condition))
			break;

		stmt.body->handler(stmt.body, result, stmtScope);

		if(stmtScope->exit == 3)
			scope->exit = 3;
		if(stmtScope->exit != 0)
			return result;

		stmt.step->handler(stmt.step, result, stmtScope);
	}

	return result;
}

ptrs_var_t *ptrs_handle_forin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_forin stmt = node->arg.forin;

	ptrs_var_t *val = stmt.value->handler(stmt.value, result, scope);

	if(val->type != PTRS_TYPE_STRUCT)
		ptrs_error(stmt.value, scope, "Cannot iterate over variable of type %s", ptrs_typetoa(val->type));

	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope);
	if(stmt.newvar)
		ptrs_scope_set(stmtScope, stmt.varname, result);
	ptrs_var_t *iterval = ptrs_scope_get(stmtScope, stmt.varname);

	if(iterval == NULL)
		ptrs_error(node, scope, "Unknown identifier %s", stmt.varname);

	struct ptrs_structlist *curr = val->value.structval->member;
	while(curr != NULL)
	{
		iterval->type = PTRS_TYPE_STRING;
		iterval->value.strval = curr->name;

		stmt.body->handler(stmt.body, result, stmtScope);

		if(stmtScope->exit == 3)
			scope->exit = 3;
		if(stmtScope->exit != 0)
			return result;

		curr = curr->next;
	}

	return result;
}

ptrs_var_t *ptrs_handle_exprstatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;
	result = expr->handler(expr, result, scope);
	return result;
}
