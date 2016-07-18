#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/stack.h"
#include "include/scope.h"
#include "include/call.h"
#include "include/struct.h"
#include "include/astlist.h"

ptrs_var_t *ptrs_handle_call(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;
	ptrs_var_t funcv;
	ptrs_var_t *func = expr.value->handler(expr.value, &funcv, scope);
	result = ptrs_call(expr.value, func, result, expr.arguments, scope);

	return result;
}

ptrs_var_t *ptrs_handle_arrayexpr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	int len = ptrs_astlist_length(node->arg.astlist, node, scope);
	uint8_t *array = ptrs_alloc(scope, len);

	ptrs_var_t vals[len];
	ptrs_astlist_handle(node->arg.astlist, vals, scope);
	for(int i = 0; i < len; i++)
	{
		array[i] = ptrs_vartoi(&vals[i]);
	}

	result->type = PTRS_TYPE_NATIVE;
	result->value.nativeval = array;
	result->meta.array.readOnly = false;
	result->meta.array.size = len;
	return result;
}

ptrs_var_t *ptrs_handle_vararrayexpr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	int len = ptrs_astlist_length(node->arg.astlist, node, scope);

	result->type = PTRS_TYPE_POINTER;
	result->value.ptrval = ptrs_alloc(scope, len * sizeof(ptrs_var_t));
	result->meta.array.size = len;
	ptrs_astlist_handle(node->arg.astlist, result->value.ptrval, scope);
	return result;
}

ptrs_var_t *ptrs_handle_stringformat(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_strformat stmt = node->arg.strformat;
	int len = ptrs_astlist_length(stmt.args, node, scope) + 3;
	ptrs_var_t args[len];
	ptrs_astlist_handle(stmt.args, &args[3], scope);

	args[0].type = PTRS_TYPE_NATIVE;
	args[0].value.strval = NULL;
	args[1].type = PTRS_TYPE_INT;
	args[1].value.intval = 0;
	args[2].type = PTRS_TYPE_NATIVE;
	args[2].value.strval = stmt.str;

	args[1].value.intval = ptrs_callnative(snprintf, len, args) + 1;
	args[0].value.strval = ptrs_alloc(scope, args[1].value.intval);
	ptrs_callnative(snprintf, len, args);

	result->type = PTRS_TYPE_NATIVE;
	result->value.strval = args[0].value.strval;
	result->meta.array.size = args[1].value.intval;
	return result;
}

ptrs_var_t *ptrs_handle_new(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;
	return ptrs_struct_construct(expr.value->handler(expr.value, result, scope),
		expr.arguments, false, node, result, scope);
}

ptrs_var_t *ptrs_handle_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_member expr = node->arg.member;
	ptrs_var_t *base = expr.base->handler(expr.base, result, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	ptrs_var_t *_result = ptrs_struct_get(base->value.structval, result, expr.name);

	ptrs_var_t overload;
	if(_result == NULL && (overload.value.funcval = ptrs_struct_getOverload(base, ptrs_handle_member, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		overload.meta.this = base->value.structval;
		ptrs_var_t arg = {{.strval = expr.name}, PTRS_TYPE_NATIVE, {.array = {.readOnly = true}}};
		return ptrs_callfunc(node, result, scope, &overload, 1, &arg);
	}
	else if(_result == NULL)
	{
		result->type = PTRS_TYPE_UNDEFINED;
		return result;
	}

	return _result;
}

ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);

	if(val == result)
	{
		if(val->meta.pointer != NULL)
		{
			result->type = PTRS_TYPE_NATIVE;
			result->value.nativeval = val->meta.pointer;
		}
		else
		{
			ptrs_error(node, scope, "Cannot get address from static expression");
		}
	}

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
		if(val->meta.array.readOnly)
			result->meta.pointer = NULL;
		else
			result->meta.pointer = val->value.nativeval;
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
	else if(valuet == PTRS_TYPE_NATIVE)
	{
		int64_t _index = ptrs_vartoi(index);
		result->type = PTRS_TYPE_INT;
		result->value.intval = value->value.strval[_index];
		if(value->meta.array.readOnly)
			result->meta.pointer = NULL;
		else
			result->meta.pointer = (uint8_t*)&value->value.strval[_index];
	}
	else if(valuet == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_var_t *_result = ptrs_struct_get(value->value.structval, result, key);

		ptrs_var_t overload;
		if(_result == NULL && (overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_index, true)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;
			overload.meta.this = value->value.structval;
			ptrs_var_t arg = {{.strval = key}, PTRS_TYPE_NATIVE};
			arg.meta.array.readOnly = key == buff || index->meta.array.readOnly;
			arg.meta.array.size = 0;
			return ptrs_callfunc(node, result, scope, &overload, 1, &arg);
		}
		else if(_result == NULL)
		{
			result->type = PTRS_TYPE_UNDEFINED;
			return result;
		}

		return _result;
	}
	else
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_error(expr.left, scope, "Cannot get index '%s' of type %s", key, ptrs_typetoa(valuet));
	}
	return result;
}

ptrs_var_t *ptrs_handle_as(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);

	result->type = expr.type;
	result->value = value->value;
	memset(&result->meta, 0, sizeof(ptrs_meta_t));
	return result;
}

ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);

	switch(expr.type)
	{
		case PTRS_TYPE_INT:
			result->value.intval = ptrs_vartoi(value);
			break;
		case PTRS_TYPE_FLOAT:
			result->value.floatval = ptrs_vartof(value);
			break;
		case PTRS_TYPE_NATIVE:
			;
			char *buff = ptrs_alloc(scope, 32);
			result->value.strval = ptrs_vartoa(value, buff, 32);
			break;
		default:
			ptrs_error(node, scope, "Cannot cast to %s", ptrs_typetoa(expr.type));
	}
	memset(&result->meta, 0, sizeof(ptrs_meta_t));

	result->type = expr.type;
	return result;
}

ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	return ptrs_scope_get(scope, node->arg.varval);
}

ptrs_var_t *ptrs_handle_constant(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	memcpy(result, &(node->arg.constval), sizeof(ptrs_var_t));
	return result;
}

ptrs_var_t *ptrs_handle_prefix_typeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	result->type = PTRS_TYPE_INT;
	result->value.intval = val->type;
	return result;
}

ptrs_var_t *ptrs_handle_op_ternary(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_ternary stmt = node->arg.ternary;

	ptrs_var_t *condition = stmt.condition->handler(stmt.condition, result, scope);
	if(ptrs_vartob(condition))
		return stmt.trueVal->handler(stmt.trueVal, result, scope);
	else
		return stmt.falseVal->handler(stmt.falseVal, result, scope);
}

ptrs_var_t *ptrs_handle_op_instanceof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary stmt = node->arg.binary;
	ptrs_var_t leftv;
	ptrs_var_t *left = stmt.left->handler(stmt.left, &leftv, scope);
	ptrs_var_t *right = stmt.right->handler(stmt.right, result, scope);

	if(left->type != PTRS_TYPE_STRUCT || right->type != PTRS_TYPE_STRUCT
		|| left->value.structval->data == NULL || right->value.structval->data != NULL
		|| left->value.structval->member != right->value.structval->member)
	{
		result->value.intval = false;
	}
	else
	{
		result->value.intval = true;
	}

	result->type = PTRS_TYPE_INT;
	result->meta.pointer = NULL;
	return result;
}
