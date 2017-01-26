#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef _PTRS_NOCALLBACK
#include <ffcb.h>
#endif

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"
#include "include/call.h"
#include "include/struct.h"
#include "include/astlist.h"

ptrs_var_t *ptrs_handle_call(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_call expr = node->arg.call;

	if(expr.value->callHandler != NULL)
	{
		return expr.value->callHandler(expr.value, result, scope, expr.retType, node, expr.arguments);
	}
	else
	{
		ptrs_var_t funcv;
		ptrs_var_t *func = expr.value->handler(expr.value, &funcv, scope);
		return ptrs_call(expr.value, expr.retType, NULL, func, result, expr.arguments, scope);
	}
}

ptrs_var_t *ptrs_handle_stringformat(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	int len = node->arg.strformat.insertionCount + 3;
	struct ptrs_stringformat *curr = node->arg.strformat.insertions;
	ptrs_var_t args[len];

	for(int i = 3; i < len; i++)
	{
		ptrs_var_t *val = curr->entry->handler(curr->entry, args + i, scope);

		if(!curr->convert)
		{
			if(val != args + i)
				memcpy(args + i, val, sizeof(ptrs_var_t));
		}
		else if(val->type == PTRS_TYPE_NATIVE)
		{
			args[i].type = PTRS_TYPE_NATIVE;
			int len = strnlen(val->value.strval, val->meta.array.size);

			if(len < val->meta.array.size)
			{
				args[i].value.strval = val->value.strval;
			}
			else
			{
				char *dup = ptrs_alloc(scope, len + 1);
				memcpy(dup, val->value.strval, len);
				dup[len] = 0;

				args[i].value.nativeval = dup;
			}
		}
		else
		{
			char *str = alloca(32);

			args[i].value.nativeval = (char *)ptrs_vartoa(val, str, 32);
			args[i].type = PTRS_TYPE_NATIVE;
		}

		curr = curr->next;
	}

	args[0].type = PTRS_TYPE_NATIVE;
	args[0].value.strval = NULL;
	args[1].type = PTRS_TYPE_INT;
	args[1].value.intval = 0;
	args[2].type = PTRS_TYPE_NATIVE;
	args[2].value.strval = node->arg.strformat.str;

	args[1].value.intval = ptrs_callnative(NULL, NULL, snprintf, len, args) + 1;
	args[0].value.strval = ptrs_alloc(scope, args[1].value.intval);
	ptrs_callnative(NULL, NULL, snprintf, len, args);

	result->type = PTRS_TYPE_NATIVE;
	result->value.strval = args[0].value.strval;
	result->meta.array.size = args[1].value.intval;
	return result;
}

ptrs_var_t *ptrs_handle_new(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_new expr = node->arg.newexpr;
	return ptrs_struct_construct(expr.value->handler(expr.value, result, scope),
		expr.arguments, expr.onStack, node, result, scope);
}

ptrs_var_t *ptrs_handle_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_member expr = node->arg.member;
	ptrs_var_t *base = expr.base->handler(expr.base, result, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	ptrs_var_t *_result = ptrs_struct_get(base->value.structval, result, expr.name, node, scope);

	ptrs_var_t overload;
	if(_result == NULL && (overload.value.funcval = ptrs_struct_getOverload(base, ptrs_handle_member, true)) != NULL)
	{
		ptrs_var_t arg;
		arg.type = PTRS_TYPE_NATIVE;
		arg.value.strval = expr.name;
		arg.meta.array.size = expr.namelen;
		arg.meta.array.readOnly = true;

		overload.type = PTRS_TYPE_FUNCTION;
		return ptrs_callfunc(node, result, scope, base->value.structval, &overload, 1, &arg);
	}
	else if(_result == NULL)
	{
		ptrs_error(node, scope, "Struct %s has no property '%s'", base->value.structval->name, expr.name);
	}

	return _result;
}
ptrs_var_t *ptrs_handle_assign_member(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	struct ptrs_ast_member expr = node->arg.member;
	ptrs_var_t basev;
	ptrs_var_t *base = expr.base->handler(expr.base, &basev, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot assign property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	if(ptrs_struct_set(base->value.structval, value, expr.name, node, scope))
		return NULL;

	ptrs_var_t overload;
	if((overload.value.funcval = ptrs_struct_getOverload(base, ptrs_handle_assign_member, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;

		ptrs_var_t args[2];
		args[0].type = PTRS_TYPE_NATIVE;
		args[0].value.strval = expr.name;
		args[0].meta.array.size = expr.namelen;
		args[0].meta.array.readOnly = true;
		memcpy(args + 1, value, sizeof(ptrs_var_t));

		ptrs_var_t result;
		ptrs_callfunc(node, &result, scope, base->value.structval, &overload, 2, args);
	}
	else
	{
		ptrs_error(node, scope, "Struct %s has no property '%s'", base->value.structval->name, expr.name);
	}

	return NULL;
}
ptrs_var_t *ptrs_handle_addressof_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_member expr = node->arg.member;
	ptrs_var_t *base = expr.base->handler(expr.base, result, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot get address property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	if(!ptrs_struct_addressOf(base->value.structval, result, expr.name, node, scope))
	{
		ptrs_var_t overload;
		if((overload.value.funcval = ptrs_struct_getOverload(base, ptrs_handle_addressof_member, true)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;

			ptrs_var_t arg;
			arg.type = PTRS_TYPE_NATIVE;
			arg.value.strval = expr.name;
			arg.meta.array.size = expr.namelen;
			arg.meta.array.readOnly = true;

			return ptrs_callfunc(node, result, scope, base->value.structval, &overload, 1, &arg);
		}
		else
		{
			ptrs_error(node, scope, "Struct %s has no property '%s'", base->value.structval->name, expr.name);
		}
	}

	return result;
}
ptrs_var_t *ptrs_handle_call_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	ptrs_var_t funcv;
	struct ptrs_ast_member expr = node->arg.member;
	ptrs_var_t *base = expr.base->handler(expr.base, result, scope);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot call property '%s' of type %s", expr.name, ptrs_typetoa(base->type));

	ptrs_var_t *func = ptrs_struct_get(base->value.structval, &funcv, expr.name, node, scope);

	if(func != NULL)
	{
		return ptrs_call(node, retType, base->value.structval, func, result, arguments, scope);
	}
	else if((funcv.value.funcval = ptrs_struct_getOverload(base, ptrs_handle_call_member, true)) != NULL)
	{
		funcv.type = PTRS_TYPE_FUNCTION;

		int len = ptrs_astlist_length(arguments, node, scope) + 1;
		ptrs_var_t args[len];
		ptrs_astlist_handle(arguments, len, args + 1, scope);
		args[0].type = PTRS_TYPE_NATIVE;
		args[0].value.strval = expr.name;
		args[0].meta.array.size = expr.namelen;
		args[0].meta.array.readOnly = true;

		return ptrs_callfunc(node, result, scope, base->value.structval, &funcv, len, args);
	}
	else
	{
		ptrs_error(node, scope, "Struct %s has no property '%s'", base->value.structval->name, expr.name);
		return NULL; //doh
	}
}

ptrs_var_t *ptrs_handle_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_thismember expr = node->arg.thismember;
	ptrs_var_t *base = ptrs_scope_get(scope, expr.base);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.member->name, ptrs_typetoa(base->type));

	ptrs_var_t *_result = ptrs_struct_getMember(base->value.structval, result, expr.member, node, scope);
	if(_result == NULL)
	{
		result->type = PTRS_TYPE_UNDEFINED;
		return result;
	}
	return _result;
}
ptrs_var_t *ptrs_handle_assign_thismember(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	struct ptrs_ast_thismember expr = node->arg.thismember;
	ptrs_var_t *base = ptrs_scope_get(scope, expr.base);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot assign property '%s' of type %s", expr.member->name, ptrs_typetoa(base->type));

	ptrs_struct_setMember(base->value.structval, value, expr.member, node, scope);

	return NULL;
}
ptrs_var_t *ptrs_handle_addressof_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_thismember expr = node->arg.thismember;
	ptrs_var_t *base = ptrs_scope_get(scope, expr.base);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.member->name, ptrs_typetoa(base->type));

	ptrs_struct_addressOfMember(base->value.structval, result, expr.member, node, scope);
	return result;
}
ptrs_var_t *ptrs_handle_call_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	ptrs_var_t funcv;
	struct ptrs_ast_thismember expr = node->arg.thismember;
	ptrs_var_t *base = ptrs_scope_get(scope, expr.base);

	if(base->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot read property '%s' of type %s", expr.member->name, ptrs_typetoa(base->type));

	ptrs_var_t *func = ptrs_struct_getMember(base->value.structval, &funcv, expr.member, node, scope);
	if(func == NULL)
	{
		func = &funcv;
		funcv.type = PTRS_TYPE_UNDEFINED;
	}

	return ptrs_call(node, retType, base->value.structval, func, result, arguments, scope);
}

struct withVal
{
	ptrs_struct_t *this;
	struct ptrs_structlist **member;
};
ptrs_var_t *ptrs_handle_withmember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_withmember expr = node->arg.withmember;
	struct withVal *withVal = (struct withVal *)ptrs_scope_get(scope, expr.base);

	if(withVal->member[expr.index] == NULL)
	{
		withVal->member[expr.index] = ptrs_struct_find(withVal->this, expr.name,
			PTRS_STRUCTMEMBER_SETTER, node, scope);

		if(withVal->member[expr.index] == NULL)
			ptrs_error(node, scope, "Struct %s has no property '%s'", withVal->this->name, expr.name);
	}

	return ptrs_struct_getMember(withVal->this, result, withVal->member[expr.index], node, scope);
}
ptrs_var_t *ptrs_handle_assign_withmember(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	struct ptrs_ast_withmember expr = node->arg.withmember;
	struct withVal *withVal = (struct withVal *)ptrs_scope_get(scope, expr.base);

	if(withVal->member[expr.index] == NULL)
	{
		withVal->member[expr.index] = ptrs_struct_find(withVal->this, expr.name,
			PTRS_STRUCTMEMBER_GETTER, node, scope);

		if(withVal->member[expr.index] == NULL)
			ptrs_error(node, scope, "Struct %s has no property '%s'", withVal->this->name, expr.name);
	}

	ptrs_struct_setMember(withVal->this, value, withVal->member[expr.index], node, scope);
	return NULL;
}
ptrs_var_t *ptrs_handle_addressof_withmember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_withmember expr = node->arg.withmember;
	struct withVal *withVal = (struct withVal *)ptrs_scope_get(scope, expr.base);

	if(withVal->member[expr.index] == NULL)
	{
		withVal->member[expr.index] = ptrs_struct_find(withVal->this, expr.name,
			-1, node, scope);

		if(withVal->member[expr.index] == NULL)
			ptrs_error(node, scope, "Struct %s has no property '%s'", withVal->this->name, expr.name);
	}

	ptrs_struct_addressOfMember(withVal->this, result, withVal->member[expr.index], node, scope);
	return result;
}
ptrs_var_t *ptrs_handle_call_withmember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	ptrs_var_t funcv;
	struct ptrs_ast_withmember expr = node->arg.withmember;
	struct withVal *withVal = (struct withVal *)ptrs_scope_get(scope, expr.base);

	if(withVal->member[expr.index] == NULL)
	{
		withVal->member[expr.index] = ptrs_struct_find(withVal->this, expr.name,
			-1, node, scope);

		if(withVal->member[expr.index] == NULL)
			ptrs_error(node, scope, "Struct %s has no property '%s'", withVal->this->name, expr.name);
	}

	ptrs_var_t *func = ptrs_struct_getMember(withVal->this, result, withVal->member[expr.index], node, scope);
	if(func == NULL)
	{
		func = &funcv;
		funcv.type = PTRS_TYPE_UNDEFINED;
	}

	return ptrs_call(node, retType, withVal->this, func, result, arguments, scope);
}

ptrs_var_t *ptrs_handle_prefix_length(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t valuev;
	ptrs_var_t *value = node->arg.astval->handler(node->arg.astval, &valuev, scope);
	result->type = PTRS_TYPE_INT;
	if(value->type == PTRS_TYPE_NATIVE || value->type == PTRS_TYPE_POINTER)
	{
		result->value.intval = value->meta.array.size;
	}
	else if(value->type == PTRS_TYPE_STRUCT)
	{
		ptrs_var_t overload;
		if((overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_prefix_length, true)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;
			value = ptrs_callfunc(node, result, scope, value->value.structval, &overload, 0, NULL);
			result->value.intval = ptrs_vartoi(value);
		}
		else
		{
			result->value.intval = value->value.structval->size;
		}
	}
	else
	{
		ptrs_error(node, scope, "Cannot get the size of variable of type %s", ptrs_typetoa(value->type));
	}

	return result;
}

ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_ast_t *ast = node->arg.astval;
	if(ast->addressHandler != NULL)
	{
		return ast->addressHandler(ast, result, scope);
	}
	else
	{
		ptrs_var_t *val = ast->handler(ast, result, scope);
		if(val == result)
			ptrs_error(node, scope, "Cannot get address from static expression");

		result->type = PTRS_TYPE_POINTER;
		result->value.ptrval = val;
		return result;
	}
}

ptrs_var_t *ptrs_handle_prefix_dereference(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	ptrs_vartype_t valuet = val->type;

	if(valuet == PTRS_TYPE_NATIVE)
	{
		result->type = PTRS_TYPE_INT;
		result->value.intval = *(uint8_t *)val->value.strval;
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
ptrs_var_t *ptrs_handle_assign_dereference(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	ptrs_var_t valv;
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, &valv, scope);

	if(val->type == PTRS_TYPE_NATIVE && !val->meta.array.readOnly)
		*(uint8_t *)val->value.strval = (uint8_t)ptrs_vartoi(value);
	else if(val->type == PTRS_TYPE_POINTER)
		memcpy(val->value.ptrval, value, sizeof(ptrs_var_t));
	else if(val->type == PTRS_TYPE_NATIVE && val->meta.array.readOnly)
		ptrs_error(node, scope, "Cannot change a read-only string");
	else
		ptrs_error(node, scope, "Cannot dereference variable of type %s", ptrs_typetoa(val->type));

	return NULL;
}

__thread int64_t indexLen = 0;
ptrs_var_t *ptrs_handle_indexlength(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	result->type = PTRS_TYPE_INT;
	result->value.intval = indexLen;
	return result;
}

ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	char buff[32];
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);

	int64_t oldLen = indexLen;
	if(value->type == PTRS_TYPE_STRUCT)
		indexLen = -1;
	else
		indexLen = value->meta.array.size;

	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);
	indexLen = oldLen;

	ptrs_vartype_t valuet = value->type;

	if(valuet == PTRS_TYPE_POINTER)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= value->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of var-array of size %d", _index, value->meta.array.size);

		return &(value->value.ptrval[_index]);
	}
	else if(valuet == PTRS_TYPE_NATIVE)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= value->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of array of size %d", _index, value->meta.array.size);

		result->type = PTRS_TYPE_INT;
		result->value.intval = (uint8_t)value->value.strval[_index];
	}
	else if(valuet == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_var_t *_result = ptrs_struct_get(value->value.structval, result, key, node, scope);

		ptrs_var_t overload;
		if(_result == NULL && (overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_index, true)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;
			return ptrs_callfunc(node, result, scope, value->value.structval, &overload, 1, index);
		}
		else if(_result == NULL)
		{
			ptrs_error(node, scope, "Struct %s has no property '%s'", value->value.structval->name, key);
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
ptrs_var_t *ptrs_handle_assign_index(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	char buff[32];
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *val = expr.left->handler(expr.left, &valuev, scope);

	int64_t oldLen = indexLen;
	if(val->type == PTRS_TYPE_STRUCT)
		indexLen = -1;
	else
		indexLen = val->meta.array.size;

	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);
	indexLen = oldLen;

	if(val->type == PTRS_TYPE_POINTER)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= val->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of var-array of size %d", _index, val->meta.array.size);

		memcpy(val->value.ptrval + _index, value, sizeof(ptrs_var_t));
	}
	else if(val->type == PTRS_TYPE_NATIVE)
	{
		if(val->meta.array.readOnly)
			ptrs_error(node, scope, "Cannot change a read-only string");

		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= val->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of array of size %d", _index, val->meta.array.size);

		*(uint8_t *)(val->value.strval + _index) = (uint8_t)ptrs_vartoi(value);
	}
	else if(val->type == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		if(!ptrs_struct_set(val->value.structval, value, key, node, scope))
		{
			ptrs_var_t overload;
			if((overload.value.funcval = ptrs_struct_getOverload(val, ptrs_handle_assign_index, true)) != NULL)
			{
				overload.type = PTRS_TYPE_FUNCTION;

				ptrs_var_t args[2];
				memcpy(args, index, sizeof(ptrs_var_t));
				memcpy(args + 1, value, sizeof(ptrs_var_t));

				ptrs_callfunc(node, &indexv, scope, val->value.structval, &overload, 2, args);
			}
			else
			{
				ptrs_error(node, scope, "Struct %s has no property '%s'", val->value.structval->name, key);
			}
		}
	}
	else
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_error(expr.left, scope, "Cannot set index '%s' of type %s", key, ptrs_typetoa(val->type));
	}
	return NULL;
}
ptrs_var_t *ptrs_handle_addressof_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	char buff[32];
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);

	int64_t oldLen = indexLen;
	if(value->type == PTRS_TYPE_STRUCT)
		indexLen = -1;
	else
		indexLen = value->meta.array.size;

	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);
	indexLen = oldLen;

	if(value->type == PTRS_TYPE_POINTER)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= value->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of var-array of size %d", _index, value->meta.array.size);

		result->type = PTRS_TYPE_POINTER;
		result->value.ptrval = &(value->value.ptrval[_index]);
		result->meta.array.size = value->meta.array.size - _index;
	}
	else if(value->type == PTRS_TYPE_NATIVE)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= value->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of array of size %d", _index, value->meta.array.size);

		result->type = PTRS_TYPE_NATIVE;
		result->value.intval = value->value.strval[_index];
	}
	else if(value->type == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		if(!ptrs_struct_addressOf(value->value.structval, result, key, node, scope))
		{
			ptrs_var_t overload;
			if((overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_addressof_index, true)) != NULL)
			{
				overload.type = PTRS_TYPE_FUNCTION;
				return ptrs_callfunc(node, result, scope, value->value.structval, &overload, 1, index);
			}
			else
			{
				ptrs_error(node, scope, "Struct %s has no property '%s'", value->value.structval->name, key);
			}
		}
	}
	else
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_error(expr.left, scope, "Cannot get index '%s' of type %s", key, ptrs_typetoa(value->type));
	}
	return result;
}
ptrs_var_t *ptrs_handle_call_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	char buff[32];
	ptrs_var_t valuev;
	ptrs_var_t indexv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *value = expr.left->handler(expr.left, &valuev, scope);

	int64_t oldLen = indexLen;
	if(value->type == PTRS_TYPE_STRUCT)
		indexLen = -1;
	else
		indexLen = value->meta.array.size;

	ptrs_var_t *index = expr.right->handler(expr.right, &indexv, scope);
	indexLen = oldLen;

	ptrs_vartype_t valuet = value->type;

	if(valuet == PTRS_TYPE_POINTER)
	{
		int64_t _index = ptrs_vartoi(index);
		if(_index < 0 || _index >= value->meta.array.size)
			ptrs_error(node, scope, "Index %d is out of range of var-array of size %d", _index, value->meta.array.size);

		ptrs_var_t *func = &(value->value.ptrval[_index]);
		return ptrs_call(node, retType, NULL, func, result, arguments, scope);
	}
	else if(valuet == PTRS_TYPE_STRUCT)
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_var_t *func = ptrs_struct_get(value->value.structval, &indexv, key, node, scope);

		if(func != NULL)
		{
			return ptrs_call(node, retType, value->value.structval, func, result, arguments, scope);
		}
		else if((indexv.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_call_index, true)) != NULL)
		{
			indexv.type = PTRS_TYPE_FUNCTION;

			int len = ptrs_astlist_length(arguments, node, scope) + 1;
			ptrs_var_t args[len];
			ptrs_astlist_handle(arguments, len, args + 1, scope);
			memcpy(args, index, sizeof(ptrs_var_t));

			return ptrs_callfunc(node, result, scope, value->value.structval, &indexv, len, args);
		}
		else
		{
			ptrs_error(node, scope, "Struct %s has no property '%s'", value->value.structval->name, key);
			return NULL; //doh
		}
	}
	else
	{
		const char *key = ptrs_vartoa(index, buff, 32);
		ptrs_error(expr.left, scope, "Cannot call index '%s' of type %s", key, ptrs_typetoa(valuet));
		return NULL; //doh
	}
}

ptrs_var_t *ptrs_handle_slice(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_slice expr = node->arg.slice;
	ptrs_var_t basev;
	uint32_t start;
	uint32_t end;

	ptrs_var_t *base = expr.base->handler(expr.base, &basev, scope);

	int64_t oldLen = indexLen;
	if(base->type == PTRS_TYPE_STRUCT)
		indexLen = -1;
	else
		indexLen = base->meta.array.size;

	start = ptrs_vartoi(expr.start->handler(expr.start, result, scope));
	end = ptrs_vartoi(expr.end->handler(expr.end, result, scope));
	indexLen = oldLen;

	result->type = base->type;
	if(base->type == PTRS_TYPE_NATIVE)
		result->value.nativeval = base->value.nativeval + start;
	else if(base->type == PTRS_TYPE_POINTER)
		result->value.ptrval = base->value.ptrval + start;
	else
		ptrs_error(node, scope, "Cannot slice variable of type %s", ptrs_typetoa(base->type));

	result->meta.array.readOnly = base->meta.array.readOnly;
	result->meta.array.size = end - start;
	return result;
}

ptrs_var_t *ptrs_handle_as(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);

	result->type = expr.builtinType;
	result->value = value->value;
	memset(&result->meta, 0, sizeof(ptrs_meta_t));
	return result;
}

ptrs_var_t *ptrs_handle_cast_builtin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);

	ptrs_var_t overload;
	if(value->type == PTRS_TYPE_STRUCT
		&& (overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_cast_builtin, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		ptrs_var_t arg;
		arg.type = PTRS_TYPE_INT;
		arg.value.intval = expr.builtinType;
		return ptrs_callfunc(node, result, scope, value->value.structval, &overload, 1, &arg);
	}

	switch(expr.builtinType)
	{
		case PTRS_TYPE_INT:
			result->value.intval = ptrs_vartoi(value);
			break;
		case PTRS_TYPE_FLOAT:
			result->value.floatval = ptrs_vartof(value);
			break;
		case PTRS_TYPE_NATIVE:
			switch(value->type)
			{
#ifndef _PTRS_NOCALLBACK
				case PTRS_TYPE_FUNCTION:
					;
					ptrs_function_t *func = value->value.funcval;
					if(func->nativeCb == NULL)
						func->nativeCb = ffcb_create(&ptrs_callcallback, func);

					result->value.nativeval = func->nativeCb;
					result->meta.array.size = 0;
					result->meta.array.readOnly = true;
					break;
#endif
				case PTRS_TYPE_STRUCT:
					result->value.nativeval = value->value.structval->data;
					result->meta.array.size = value->value.structval->size;
					result->meta.array.readOnly = false;
					break;
				default:
					ptrs_error(node, scope, "Cannot cast from %s to native", ptrs_typetoa(value->type));
			}
			break;
		default:
			ptrs_error(node, scope, "Cannot cast to %s", ptrs_typetoa(expr.builtinType));
	}

	result->type = expr.builtinType;
	return result;
}

ptrs_var_t *ptrs_handle_tostring(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;
	ptrs_var_t *val = expr.value->handler(expr.value, result, scope);

	int len;
	if(val->type == PTRS_TYPE_NATIVE)
		len = strnlen(val->value.strval, val->meta.array.size);

	if(val->type != PTRS_TYPE_NATIVE || len < val->meta.array.size)
	{
		char *buff = ptrs_alloc(scope, 32);
		result->value.strval = ptrs_vartoa(val, buff, 32);
		result->meta.array.size = strlen(result->value.strval) + 1;
		result->meta.array.readOnly = result->value.strval != buff && val->meta.array.readOnly;
	}
	else
	{
		char *buff = ptrs_alloc(scope, len + 1);
		memcpy(buff, val->value.strval, len);
		buff[len] = 0;

		result->value.strval = buff;
		result->meta.array.size = len + 1;
		result->meta.array.readOnly = false;
	}

	result->type = PTRS_TYPE_NATIVE;
	return result;
}

ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_cast expr = node->arg.cast;

	ptrs_var_t typev;
	ptrs_var_t *type = expr.type->handler(expr.type, &typev, scope);
	if(type->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Type of a cast has to be a variable type struct not %s", ptrs_typetoa(type->type));

	ptrs_var_t *value = expr.value->handler(expr.value, result, scope);

	ptrs_struct_t *struc;
	if(expr.onStack)
		struc = ptrs_alloc(scope, sizeof(ptrs_struct_t));
	else
		struc = malloc(sizeof(ptrs_struct_t));

	memcpy(struc, type->value.structval, sizeof(ptrs_struct_t));
	struc->data = value->value.nativeval;
	struc->isOnStack = expr.onStack;

	result->type = PTRS_TYPE_STRUCT;
	result->value.structval = struc;
	return result;
}

ptrs_var_t *ptrs_handle_wildcardsymbol(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_wildcard expr = node->arg.wildcard;
	ptrs_var_t *imports = ptrs_scope_get(scope, expr.symbol);

	if(imports->type == PTRS_TYPE_NATIVE)
	{
		void **nativeImports = imports->value.nativeval;
		result->type = PTRS_TYPE_NATIVE;
		result->value.nativeval = nativeImports[expr.index];
		result->meta.array.size = 0;
		result->meta.array.readOnly = true;
		return result;
	}
	else
	{
		ptrs_var_t *scriptImports = imports->value.ptrval;
		return &scriptImports[expr.index];
	}
}

ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	if(node->arg.varval.scope == 0)
		return scope->bp + node->arg.varval.offset;
	else
		return ptrs_scope_get(scope, node->arg.varval);
}
ptrs_var_t *ptrs_handle_assign_identifier(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope)
{
	ptrs_scope_set(scope, node->arg.varval, value);
	return NULL;
}

ptrs_var_t *ptrs_handle_constant(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	memcpy(result, &(node->arg.constval), sizeof(ptrs_var_t));
	return result;
}

ptrs_var_t *ptrs_handle_prefix_typeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *val = node->arg.astval->handler(node->arg.astval, result, scope);
	result->value.intval = val->type;
	result->type = PTRS_TYPE_INT;
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
	return result;
}

ptrs_var_t *ptrs_handle_op_in(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary stmt = node->arg.binary;
	char buff[16];
	ptrs_var_t *left = stmt.left->handler(stmt.left, result, scope);
	const char *key = ptrs_vartoa(left, buff, 16);

	ptrs_var_t *right = stmt.right->handler(stmt.right, result, scope);
	if(right->type != PTRS_TYPE_STRUCT)
		ptrs_error(node, scope, "Cannot use the 'in' operator on a variable of type %s", ptrs_typetoa(right->type));

	ptrs_var_t overload;
	if(ptrs_struct_get(right->value.structval, result, key, node, scope) != NULL)
	{
		result->value.intval = true;
	}
	else if((overload.value.funcval = ptrs_struct_getOverload(right, ptrs_handle_op_in, false)) != NULL)
	{
		ptrs_var_t arg;
		arg.type = PTRS_TYPE_NATIVE;
		arg.value.strval = key;
		arg.meta.array.size = 0;
		arg.meta.array.readOnly = false;

		overload.type = PTRS_TYPE_FUNCTION;
		return ptrs_callfunc(node, result, scope, right->value.structval, &overload, 1, &arg);
	}
	else
	{
		result->value.intval = false;
	}

	result->type = PTRS_TYPE_INT;
	return result;
}

extern ptrs_var_t __thread ptrs_forinOverloadResult;
ptrs_var_t *ptrs_handle_yield(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_yield expr = node->arg.yield;
	int len = ptrs_astlist_length(node->arg.yield.values, node, scope);
	ptrs_var_t vals[len];
	ptrs_astlist_handle(node->arg.yield.values, len, vals, scope);

	void **yieldVal = ptrs_scope_get(scope, expr.yieldVal)->value.nativeval;
	struct ptrs_ast_forin *forStmt = yieldVal[0];

	ptrs_scope_t *stmtScope = ptrs_scope_increase(scope, forStmt->stackOffset);
	stmtScope->outer = yieldVal[1];
	stmtScope->callScope = scope;
	stmtScope->callAst = node;
	stmtScope->calleeName = "(for in loop)";

	for(int i = 0; i < forStmt->varcount; i++)
		ptrs_scope_set(stmtScope, forStmt->varsymbols[i], &vals[i]);

	ptrs_var_t *_result = forStmt->body->handler(forStmt->body, result, stmtScope);

	if(stmtScope->exit > 1)
	{
		memcpy(&ptrs_forinOverloadResult, _result, sizeof(ptrs_var_t));
		result->value.intval = true;

		if(stmtScope->exit == 3)
			stmtScope->outer->exit = 3;
	}
	else
	{
		result->value.intval = false;
	}

	result->type = PTRS_TYPE_INT;
	return result;
}
