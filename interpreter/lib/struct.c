#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../interpreter.h"

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/astlist.h"
#include "../include/call.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler, bool isLeftSide)
{
	bool isInstance = struc->value.structval->data != NULL;
	struct ptrs_opoverload *curr = struc->value.structval->overloads;
	while(curr != NULL)
	{
		if(curr->op == handler && curr->isLeftSide == isLeftSide
			&& (isInstance || curr->isStatic))
			return curr->handler;
		curr = curr->next;
	}
	return NULL;
}

bool ptrs_struct_canAccess(ptrs_struct_t *struc, struct ptrs_structmember *member, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	switch(member->protection)
	{
		case 0:
			return true;
		case 1:
			;
			ptrs_scope_t *_scope = scope;
			while(_scope->outer != NULL)
				_scope = _scope->outer;
			if(struc->scope->stackstart == _scope->stackstart)
				return true;
			//fallthrough
		default:
			//TODO make thismember expressions great again
			if(node->handler == (void *)ptrs_handle_thismember
				|| node->handler == (void *)ptrs_handle_assign_thismember
				|| node->handler == (void *)ptrs_handle_addressof_thismember
				|| node->handler == (void *)ptrs_handle_call_thismember)
				return true;

			if(node != NULL)
				ptrs_error(node, scope, "Cannot access property %s of struct %s\n", member->name, struc->name);
			return false;
	}
}

unsigned long ptrs_struct_hashName(const char *key)
{
	//TODO find a better hashing algorithm
	unsigned long hash = toupper(*key++) - '0';
	while(*key != 0)
	{
		if(isupper(*key) || isdigit(*key))
		{
			hash <<= 3;
			hash += toupper(*(key - 1)) - '0';
			hash ^= toupper(*key) - '0';
		}
		key++;
	}

	hash += toupper(*--key);
	return hash;
}

struct ptrs_structmember *ptrs_struct_find(ptrs_struct_t *struc, const char *key,
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	if(struc->memberCount == 0)
		return NULL;

	int i = ptrs_struct_hashName(key) % struc->memberCount;
	while(struc->member[i].name != NULL)
	{
		if(strcmp(struc->member[i].name, key) == 0 && struc->member[i].type != exclude)
		{
			ptrs_struct_canAccess(struc, &struc->member[i], ast, scope);
			return &struc->member[i];
		}

		i = (i + 1) % struc->memberCount;
	}

	return NULL;
}

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structmember *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, scope, "Cannot access non-static property of struct %s", struc->name);

		data = struc->staticData;
	}

	ptrs_var_t func;
	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return data + member->offset;
		case PTRS_STRUCTMEMBER_GETTER:
			func.type = PTRS_TYPE_FUNCTION;
			func.value.funcval = member->value.function;
			return ptrs_callfunc(ast, result, scope, struc, &func, 0, NULL);
		case PTRS_STRUCTMEMBER_SETTER:
			return NULL;
		case PTRS_STRUCTMEMBER_FUNCTION:
			result->type = PTRS_TYPE_FUNCTION;
			result->value.funcval = member->value.function;
			return result;
		case PTRS_STRUCTMEMBER_ARRAY:
			result->type = PTRS_TYPE_NATIVE;
			result->value.nativeval = data + member->offset;
			result->meta.array.readOnly = false;
			result->meta.array.size = member->value.size;
			return result;
		case PTRS_STRUCTMEMBER_VARARRAY:
			result->type = PTRS_TYPE_POINTER;
			result->value.ptrval = data + member->offset;
			result->meta.array.size = member->value.size / sizeof(ptrs_var_t);
			return result;
		case PTRS_STRUCTMEMBER_TYPED:
			return member->value.type->getHandler(data + member->offset, member->value.type->size, result);
	}

	return NULL;
}

void ptrs_struct_setMember(ptrs_struct_t *struc, ptrs_var_t *value, struct ptrs_structmember *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, scope, "Cannot assign non-static property of struct %s", struc->name);

		data = struc->staticData;
	}

	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		memcpy(data + member->offset, value, sizeof(ptrs_var_t));
	}
	else if(member->type == PTRS_STRUCTMEMBER_SETTER)
	{
		ptrs_var_t func;
		ptrs_var_t result;
		func.type = PTRS_TYPE_FUNCTION;
		func.value.funcval = member->value.function;
		ptrs_callfunc(ast, &result, scope, struc, &func, 1, value);
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		ptrs_nativetype_info_t *type = member->value.type;
		type->setHandler(data + member->offset, type->size, value);
	}
	else
	{
		ptrs_error(ast, scope, "Cannot assign to non-variable and non-property struct member");
	}
}

void ptrs_struct_addressOfMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structmember *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, scope, "Cannot get address of non-static property of struct %s", struc->name);

		data = struc->staticData;
	}

	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		result->type = PTRS_TYPE_POINTER;
		result->value.ptrval = data + member->offset;
		result->meta.array.size = 1;
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		result->type = PTRS_TYPE_NATIVE;
		result->value.nativeval = data + member->offset;
		result->meta.array.size = member->value.type->size;
	}
	else
	{
		ptrs_error(ast, scope, "Cannot get address of non-property struct member");
	}
}

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, ast, scope);
	if(member == NULL)
		return NULL;

	return ptrs_struct_getMember(struc, result, member, ast, scope);
}

bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast, scope);
	if(member == NULL)
		return false;

	ptrs_struct_setMember(struc, value, member, ast, scope);
	return true;
}

bool ptrs_struct_addressOf(ptrs_struct_t *struc, ptrs_var_t *result, const char *key,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast, scope);
	if(member == NULL)
		return false;

	ptrs_struct_addressOfMember(struc, result, member, ast, scope);
	return true;
}

ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_struct_t *type = constructor->value.structval;

	if(constructor->type != PTRS_TYPE_STRUCT || type->data != NULL)
		ptrs_error(node, scope, "Variable of type %s is not a constructor", ptrs_typetoa(constructor->type));

	ptrs_struct_t *instance;
	if(allocateOnStack)
	 	instance = ptrs_alloc(scope, sizeof(ptrs_struct_t) + type->size);
	else
		instance = malloc(sizeof(ptrs_struct_t) + type->size);

	memcpy(instance, type, sizeof(ptrs_struct_t));
	instance->data = instance + 1;
	instance->isOnStack = allocateOnStack;

	ptrs_error_t error;
	if(!allocateOnStack && ptrs_error_catch(scope, &error, false))
	{
		free(instance);
		ptrs_error_reThrow(scope, &error);
	}

	ptrs_scope_t *initScope = ptrs_scope_increase(scope, 0);
	initScope->outer = instance->scope;

	for(int i = 0; i < instance->memberCount; i++)
	{
		struct ptrs_structmember *member = &instance->member[i];

		if(member->type == PTRS_STRUCTMEMBER_VAR && member->value.startval != NULL && !member->isStatic)
		{
			ptrs_ast_t *ast = member->value.startval;
			ptrs_var_t *memberAddr = instance->data + member->offset;
			ptrs_var_t *val = ast->handler(ast, memberAddr, initScope);
			if(val != memberAddr)
				memcpy(memberAddr, val, sizeof(ptrs_var_t));
		}
		else if(member->type == PTRS_STRUCTMEMBER_ARRAY && member->value.arrayInit != NULL && !member->isStatic)
		{
			int len = ptrs_astlist_length(member->value.arrayInit, node, scope);
			if(len > member->value.size)
				ptrs_error(node, scope, "Cannot initialize array of size %d with %d elements", member->value.size, len);

			ptrs_astlist_handleByte(member->value.arrayInit, member->value.size, instance->data + member->offset, scope);
		}
		else if(member->type == PTRS_STRUCTMEMBER_VARARRAY && member->value.arrayInit != NULL && !member->isStatic)
		{
			int len = ptrs_astlist_length(member->value.arrayInit, node, scope);
			int size = member->value.size / sizeof(ptrs_var_t);
			if(len > size)
				ptrs_error(node, scope, "Cannot initialize var-array of size %d with %d elements", size, len);

			ptrs_astlist_handle(member->value.arrayInit, size, instance->data + member->offset, scope);
		}
	}

	ptrs_var_t overload = {{.structval = instance}, .type = PTRS_TYPE_STRUCT};
	if((overload.value.funcval = ptrs_struct_getOverload(&overload, ptrs_handle_new, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		ptrs_call(node, PTRS_TYPE_UNDEFINED, instance, &overload, result, arguments, scope);
	}

	if(!allocateOnStack)
		ptrs_error_stopCatch(scope, &error);

	result->type = PTRS_TYPE_STRUCT;
	result->value.structval = instance;
	return result;
}
