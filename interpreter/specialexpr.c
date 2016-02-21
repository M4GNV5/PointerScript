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
		//TODO
	}
	else if(func->type == PTRS_TYPE_NATIVE)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = ptrs_callnative(func->value.nativefunc, len, args);
	}
	else
	{
		ptrs_error(expr.value, "Cannot call value of type %s", ptrs_typetoa(func->type));
	}

	return result;
}

ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);

	if(val == result)
		ptrs_error(node, "Cannot get address from static expression");

	result->type = PTRS_TYPE_POINTER;
	result->value.ptrval = val;
}

ptrs_var_t *ptrs_handle_prefix_dereference(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	ptrs_vartype_t valuet = val->type;

	if(valuet == PTRS_TYPE_RAW)
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
}

ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);
	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);

	ptrs_vartype_t indext = index->type;
	ptrs_vartype_t valuet = value->type;

	if(indext == PTRS_TYPE_INT)
	{
		int64_t _index = index->value.intval;

		if(valuet == PTRS_TYPE_POINTER)
		{
			return &(value->value.ptrval[_index]);
		}
		else if(valuet == PTRS_TYPE_RAW || valuet == PTRS_TYPE_STRING)
		{
			result->type = PTRS_TYPE_INT;
			result->value.intval = value->value.strval[_index];
			return result;
		}
		else
		{
			ptrs_error(expr.left, "Cannot get index %d of type %s", _index, ptrs_typetoa(valuet));
		}
	}
	else
	{
		ptrs_error(expr.right, "Index has invalid type %s", ptrs_typetoa(indext));
	}
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

ptrs_var_t *ptrs_handle_string(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_STRING;
	result->value.strval = strdup(node->arg.strval);
	return result;
}

ptrs_var_t *ptrs_handle_int(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_INT;
	result->value.intval = node->arg.intval;
	return result;
}

ptrs_var_t *ptrs_handle_float(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_FLOAT;
	result->value.floatval = node->arg.floatval;
	return result;
}

ptrs_var_t *ptrs_handle_prefix_sizeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	if(node->arg.astval->handler == ptrs_handle_identifier
		&& strcmp(node->arg.astval->arg.strval, "var") == 0)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = sizeof(ptrs_var_t);
		return result;
	}

	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	ptrs_vartype_t type = val->type;

	result->type = PTRS_TYPE_INT;
	if(type == PTRS_TYPE_STRING)
		result->value.intval = strlen(val->value.strval) + 1;
	else
		result->value.intval = sizeof(ptrs_var_t);
	return result;
}

ptrs_var_t *ptrs_handle_prefix_typeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	result->value.strval = strdup(ptrs_typetoa(val->type));
	result->type = PTRS_TYPE_STRING;
	return result;
}
