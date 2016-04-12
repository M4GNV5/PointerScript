#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"
#include "include/call.h"

ptrs_var_t *ptrs_handle_call(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;

	int len = 0;
	struct ptrs_astlist *list = expr.arguments;
	while(list)
	{
		len++;
		list = list->next;
	}

	ptrs_var_t *arg;
	ptrs_var_t *args = malloc(sizeof(ptrs_var_t) * len);
	list = expr.arguments;
	for(int i = 0; i < len; i++)
	{
		arg = list->entry->handler(list->entry, &args[i], scope);

		if(arg != &args[i])
			memcpy(&args[i], arg, sizeof(ptrs_var_t));

		list = list->next;
	}

	ptrs_var_t *func = expr.value->handler(expr.value, result, scope);

	if(func->type == PTRS_TYPE_FUNCTION)
	{
		result = ptrs_callfunc(func->value.funcval, result, len, args);
	}
	else if(func->type == PTRS_TYPE_NATIVE)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = ptrs_callnative(expr.value, func->value.nativeval, len, args);
	}
	else
	{
		ptrs_error(expr.value, "Cannot call value of type %s", ptrs_typetoa(func->type));
	}

	free(args);
	return result;
}

ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);

	if(val == result)
		ptrs_error(node, "Cannot get address from static expression");

	result->type = PTRS_TYPE_POINTER;
	result->value.ptrval = val;
	return result;
}

ptrs_var_t *ptrs_handle_prefix_dereference(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	ptrs_vartype_t valuet = val->type;

	if(valuet == PTRS_TYPE_NATIVE)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = *val->value.strval;
	}
	else if(valuet == PTRS_TYPE_POINTER)
	{
		return val->value.ptrval;
	}
	else
	{
		ptrs_error(node, "Cannot dereference variable of type %s", ptrs_typetoa(valuet));
	}
	return result;
}

ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);
	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);

	ptrs_vartype_t valuet = value->type;
	int64_t _index = ptrs_vartoi(index);

	if(valuet == PTRS_TYPE_POINTER)
	{
		return &(value->value.ptrval[_index]);
	}
	else if(valuet == PTRS_TYPE_NATIVE || valuet == PTRS_TYPE_STRING)
	{
		result->type = valuet;
		result->value.intval = value->value.strval[_index];
	}
	else
	{
		ptrs_error(expr.left, "Cannot get index %d of type %s", _index, ptrs_typetoa(valuet));
	}
	return result;
}

ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t *value = &valuev;
	struct ptrs_ast_cast expr = node->arg.cast;

	value = expr.value->handler(expr.value, value, scope);
	memcpy(result, value, sizeof(ptrs_var_t));

	result->type = expr.type;
	return result;
}

ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result = ptrs_scope_get(scope, node->arg.strval);
	if(result == NULL)
		ptrs_error(node, "Unknown identifier %s", node->arg.strval);
	return result;
}

ptrs_var_t *ptrs_handle_constant(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	memcpy(result, &(node->arg.constval), sizeof(ptrs_var_t));
	return result;
}

ptrs_var_t *ptrs_handle_prefix_typeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	result->value.strval = ptrs_typetoa(val->type);
	result->type = PTRS_TYPE_STRING;
	return result;
}
