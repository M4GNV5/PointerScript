#include <stdlib.h>
#include <dlfcn.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/memory.h"
#include "include/scope.h"

ptrs_var_t *ptrs_handle_body(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	ptrs_var_t *_result;

	while(list)
	{
		_result = list->entry->handler(list->entry, result, scope);
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

	ptrs_scope_set(scope, stmt.name, ptrs_vardup(result));

	return result;
}

ptrs_var_t *ptrs_handle_import(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t *value;
	char name[128];

	struct ptrs_ast_import import = node->arg.import;

	void *handle;
	if(import.from != NULL)
	{
		value = import.from->handler(import.from, &valuev, scope);
		ptrs_vartoa(value, name, 128);
		handle = dlopen(name, RTLD_LAZY);
	}
	else
	{
		handle = dlopen(NULL, RTLD_LAZY);
	}

	if(handle == NULL)
		ptrs_error(node, "%s", dlerror());

	struct ptrs_astlist *list = import.fields;
	while(list != NULL)
	{
		value = list->entry->handler(list->entry, &valuev, scope);
		ptrs_vartoa(value, name, 128);

		ptrs_var_t *func = ptrs_alloc(sizeof(ptrs_var_t));
		func->type = PTRS_TYPE_NATIVE;
		func->value.nativeval = dlsym(handle, name);

		char *error = dlerror();
		if(error != NULL)
			ptrs_error(list->entry, "%s", error);

		ptrs_scope_set(scope, name, func);

		list = list->next;
	}

	dlclose(handle);
	return result;
}

ptrs_var_t *ptrs_handle_function(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_function astfunc = node->arg.function;
	
	ptrs_function_t *func = ptrs_alloc(sizeof(ptrs_function_t));
	func->name = astfunc.name;
	func->argc = astfunc.argc;
	func->args = astfunc.args;
	func->body = astfunc.body;
	func->scope = scope;
	
	ptrs_var_t *var = ptrs_alloc(sizeof(ptrs_var_t));
	var->type = PTRS_TYPE_FUNCTION;
	var->value.funcval = func;
	
	ptrs_scope_set(scope, astfunc.name, var);
	return var;
}

ptrs_var_t *ptrs_handle_if(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse stmt = node->arg.ifelse;
	result = stmt.condition->handler(stmt.condition, result, scope);

	if(ptrs_vartob(result))
		result = stmt.ifBody->handler(stmt.ifBody, result, scope);
	else if(stmt.elseBody != NULL)
		result = stmt.elseBody->handler(stmt.elseBody, result, scope);

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
		stmt.step->handler(stmt.step, result, scope);
	}

	return result;
}

ptrs_var_t *ptrs_handle_exprstatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;
	result = expr->handler(expr, result, scope);
	return result;
}
