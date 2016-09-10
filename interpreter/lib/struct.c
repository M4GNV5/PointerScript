#include <stdlib.h>
#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../interpreter.h"

#include "../include/error.h"
#include "../include/stack.h"
#include "../include/conversion.h"
#include "../include/astlist.h"
#include "../include/call.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler, bool isLeftSide)
{
	struct ptrs_opoverload *curr = struc->value.structval->overloads;
	while(curr != NULL)
	{
		if(curr->op == handler && curr->isLeftSide == isLeftSide)
			return curr->handler;
		curr = curr->next;
	}
	return NULL;
}

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structlist *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	if(struc->data == NULL && member->type != PTRS_STRUCTMEMBER_FUNCTION)
		return NULL;

	ptrs_var_t func;
	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return struc->data + member->offset;
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
			result->value.nativeval = struc->data + member->offset;
			result->meta.array.readOnly = false;
			result->meta.array.size = member->value.size;
			return result;
		case PTRS_STRUCTMEMBER_VARARRAY:
			result->type = PTRS_TYPE_POINTER;
			result->value.ptrval = struc->data + member->offset;
			result->meta.array.size = member->value.size;
			return result;
		case PTRS_STRUCTMEMBER_TYPED:
			return member->value.type->getHandler(struc->data + member->offset, member->value.type->size, result);
	}

	return NULL;
}

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(strcmp(curr->name, key) == 0 && !curr->isPrivate && curr->type != PTRS_STRUCTMEMBER_SETTER)
			return ptrs_struct_getMember(struc, result, curr, ast, scope);
		curr = curr->next;
	}

	return NULL;
}

void ptrs_struct_setMember(ptrs_struct_t *struc, ptrs_var_t *value, struct ptrs_structlist *member,
	ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		memcpy(struc->data + member->offset, value, sizeof(ptrs_var_t));
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
		type->setHandler(struc->data + member->offset, type->size, value);
	}
	else
	{
		ptrs_error(ast, scope, "Cannot assign to non-variable and non-property struct member\n");
	}
}

bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(strcmp(curr->name, key) == 0 && !curr->isPrivate)
		{
			ptrs_struct_setMember(struc, value, curr, ast, scope);
			return true;
		}
		curr = curr->next;
	}
	return false;
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

	ptrs_scope_t *initScope = ptrs_scope_increase(scope, 0);
	initScope->outer = instance->scope;
	struct ptrs_structlist *member = instance->member;
	while(member != NULL)
	{
		if(member->type == PTRS_STRUCTMEMBER_VAR && member->value.startval != NULL)
		{
			ptrs_ast_t *ast = member->value.startval;
			ptrs_var_t *memberAddr = instance->data + member->offset;
			ptrs_var_t *val = ast->handler(ast, memberAddr, initScope);
			if(val != memberAddr)
				memcpy(memberAddr, val, sizeof(ptrs_var_t));
		}
		member = member->next;
	}

	ptrs_var_t overload = {{.structval = instance}, .type = PTRS_TYPE_STRUCT};
	if((overload.value.funcval = ptrs_struct_getOverload(&overload, ptrs_handle_new, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		ptrs_call(node, PTRS_TYPE_UNDEFINED, instance, &overload, result, arguments, scope);
	}

	result->type = PTRS_TYPE_STRUCT;
	result->value.structval = instance;
	return result;
}
