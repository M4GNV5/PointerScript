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

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, ptrs_asthandler_t handler, bool isLeftSide)
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

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result, struct ptrs_structlist *curr)
{
	if(struc->data == NULL && curr->type != PTRS_STRUCTMEMBER_FUNCTION)
		return NULL;

	size_t offset;
	switch(curr->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return struc->data + curr->offset;
		case PTRS_STRUCTMEMBER_FUNCTION:
			result->type = PTRS_TYPE_FUNCTION;
			result->value.funcval = curr->function;
			result->meta.this = struc;
			return result;
		case PTRS_STRUCTMEMBER_ARRAY:
			offset = struc->offsetTable[curr->offset];
			result->type = PTRS_TYPE_NATIVE;
			result->value.nativeval = struc->data + offset;
			result->meta.array.readOnly = false;
			result->meta.array.size = struc->offsetTable[curr->offset + 1] - offset;
			return result;
		case PTRS_STRUCTMEMBER_VARARRAY:
			offset = struc->offsetTable[curr->offset];
			result->type = PTRS_TYPE_POINTER;
			result->value.ptrval = struc->data + offset;
			result->meta.array.size = struc->offsetTable[curr->offset + 1] - offset;
			return result;
		default:
			return NULL;
	}
}

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key)
{
	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(strcmp(curr->name, key) == 0)
			return ptrs_struct_getMember(struc, result, curr);
		curr = curr->next;
	}

	return NULL;
}

size_t ptrs_struct_calculateSize(ptrs_struct_t *struc, size_t *offsetTable, ptrs_scope_t *scope)
{
	ptrs_var_t valv;
	size_t size = sizeof(ptrs_struct_t) + struc->size + (struc->offsetTableSize + 1) * sizeof(size_t);
	int i;

	struct ptrs_structlist *curr = struc->member;
	while(curr != NULL)
	{
		if(curr->type == PTRS_STRUCTMEMBER_ARRAY || curr->type == PTRS_STRUCTMEMBER_VARARRAY)
		{
			ptrs_var_t *val = curr->value->handler(curr->value, &valv, scope);
			offsetTable[i] = size;

			if(curr->type == PTRS_STRUCTMEMBER_VARARRAY)
				size += ptrs_vartoi(val) * sizeof(ptrs_var_t);
			else
				size += ptrs_vartoi(val);
			i++;
		}
		curr = curr->next;
	}

	offsetTable[i] = size;
	return size;
}

ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_struct_t *type = constructor->value.structval;

	if(constructor->type != PTRS_TYPE_STRUCT || type->data != NULL)
		ptrs_error(node, scope, "Variable of type %s is not a constructor", ptrs_typetoa(constructor->type));

	size_t offsetTable[type->offsetTableSize + 1];
	size_t size = ptrs_struct_calculateSize(type, offsetTable, scope);

	ptrs_struct_t *instance;
	if(allocateOnStack)
	 	instance = ptrs_alloc(scope, size);
	else
		instance = malloc(size);

	memcpy(instance, type, sizeof(ptrs_struct_t));
	instance->offsetTable = (void*)(instance + 1);
	instance->data = (void*)(instance + 1) + (instance->offsetTableSize + 1) * sizeof(size_t);
	instance->size = size;
	memcpy(instance->offsetTable, offsetTable, (instance->offsetTableSize + 1) * sizeof(size_t));

	ptrs_scope_t *initScope = ptrs_scope_increase(scope, 0);
	initScope->outer = instance->scope;
	struct ptrs_structlist *member = instance->member;
	while(member != NULL)
	{
		if(member->type == PTRS_STRUCTMEMBER_VAR && member->value != NULL)
		{
			ptrs_ast_t *ast = member->value;
			ptrs_var_t *memberAddr = instance->data + member->offset;
			ptrs_var_t *val = ast->handler(ast, memberAddr, initScope);
			if(val != memberAddr)
				memcpy(memberAddr, val, sizeof(ptrs_var_t));
		}
		member = member->next;
	}

	ptrs_var_t overload = {{.structval = instance}, PTRS_TYPE_STRUCT};
	if((overload.value.funcval = ptrs_struct_getOverload(&overload, ptrs_handle_new, true)) != NULL)
	{
		overload.type = PTRS_TYPE_FUNCTION;
		overload.meta.this = instance;
		ptrs_call(node, &overload, result, arguments, scope);
	}

	result->type = PTRS_TYPE_STRUCT;
	result->value.structval = instance;
	return result;
}
