#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"
#include "include/call.h"

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key)
{
	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(strcmp(curr->name, key) == 0)
		{
			if(curr->function != NULL)
			{
				result->type = PTRS_TYPE_FUNCTION;
				result->value.funcval = curr->function;
				result->meta.this = struc;
				return result;
			}
			else
			{
				return struc->data + curr->offset;
			}
		}
		curr = curr->next;
	}

	result->type = PTRS_TYPE_UNDEFINED;
	return result;
}

ptrs_var_t *ptrs_handle_call(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;
	ptrs_var_t *func = expr.value->handler(expr.value, result, scope);
	result = ptrs_call(expr.value, func, result, expr.arguments, scope);

	return result;
}

ptrs_var_t *ptrs_handle_arrayexpr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	int len = 0;
	while(list != NULL)
	{
		len++;
		list = list->next;
	}

	ptrs_var_t *array = malloc(sizeof(ptrs_var_t) * (len + 1));
	list = node->arg.astlist;
	for(int i = 0; i < len; i++)
	{
		ptrs_var_t *val = list->entry->handler(list->entry, &array[i], scope);
		if(val != &array[i])
			memcpy(&array[i], val, sizeof(ptrs_var_t));
		list = list->next;
	}

	array[len].type = PTRS_TYPE_NATIVE;
	array[len].value.nativeval = NULL;
	result->type = PTRS_TYPE_POINTER;
	result->value.ptrval = array;
	return result;
}

ptrs_var_t *ptrs_handle_new(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;
	ptrs_var_t *constructor = expr.value->handler(expr.value, result, scope);
	ptrs_struct_t *type = constructor->value.structval;

	if(constructor->type != PTRS_TYPE_STRUCT || type->data != NULL)
		ptrs_error(node, scope, "Variable of type %s is not a constructor", ptrs_typetoa(constructor->type));

	ptrs_struct_t *instance = malloc(sizeof(ptrs_struct_t) + type->size);
	memcpy(instance, type, sizeof(ptrs_struct_t));
	instance->data = instance + 1;

	if(instance->constructor != NULL)
	{
		ptrs_var_t ctor;
		ctor.type = PTRS_TYPE_FUNCTION;
		ctor.value.funcval = instance->constructor;
		ctor.meta.this = instance;
		ptrs_call(node, &ctor, result, expr.arguments, scope);
	}

	result->type = PTRS_TYPE_STRUCT;
	result->value.structval = instance;
	return result;
}

ptrs_var_t *ptrs_handle_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_define expr = node->arg.define;
	ptrs_var_t *base = expr.value->handler(expr.value, result, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	return ptrs_struct_get(base->value.structval, result, expr.name);
}

ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);

	if(val == result)
		ptrs_error(node, scope, "Cannot get address from static expression");

	result->type = PTRS_TYPE_POINTER;
	result->value.ptrval = val;
	return result;
}

ptrs_var_t *ptrs_handle_prefix_dereference(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	ptrs_vartype_t valuet = val->type;

	if(valuet == PTRS_TYPE_NATIVE || valuet == PTRS_TYPE_STRING)
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
		ptrs_error(node, scope, "Cannot dereference variable of type %s", ptrs_typetoa(valuet));
	}
	return result;
}

ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	char buff[32];
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);
	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);

	ptrs_vartype_t valuet = value->type;

	if(valuet == PTRS_TYPE_POINTER)
	{
		int64_t _index = ptrs_vartoi(index);
		return &(value->value.ptrval[_index]);
	}
	else if(valuet == PTRS_TYPE_NATIVE || valuet == PTRS_TYPE_STRING)
	{
		int64_t _index = ptrs_vartoi(index);
		result->type = PTRS_TYPE_INT;
		result->value.intval = value->value.strval[_index];
	}
	else if(valuet == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		return ptrs_struct_get(value->value.structval, result, key);
	}
	else
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_error(expr.left, scope, "Cannot get index '%s' of type %s", key, ptrs_typetoa(valuet));
	}
	return result;
}

ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);;

	switch(expr.type)
	{
		case PTRS_TYPE_UNDEFINED:
			ptrs_error(node, scope, "Cannot cast to undefined");
			break;
		case PTRS_TYPE_INT:
			result->value.intval = ptrs_vartoi(value);
			break;
		case PTRS_TYPE_FLOAT:
			result->value.floatval = ptrs_vartof(value);
			break;
		default:
			memcpy(result, value, sizeof(ptrs_var_t));
	}

	result->type = expr.type;
	return result;
}

ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result = ptrs_scope_get(scope, node->arg.strval);
	if(result == NULL)
		ptrs_error(node, scope, "Unknown identifier %s", node->arg.strval);
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
