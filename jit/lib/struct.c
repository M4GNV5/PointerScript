#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../jit.h"

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/astlist.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler)
{
	bool isInstance = struc->value.structval->data != NULL;
	struct ptrs_opoverload *curr = struc->value.structval->overloads;
	while(curr != NULL)
	{
		if(curr->op == handler && (isInstance || curr->isStatic))
			return curr->handler;
		curr = curr->next;
	}
	return NULL;
}

bool ptrs_struct_canAccess(ptrs_struct_t *struc, struct ptrs_structmember *member, ptrs_ast_t *node)
{
	switch(member->protection)
	{
		case 0:
			return true;
		case 1:
			;
			//TODO

			//fallthrough
		default:
			//TODO make thismember expressions great again
			if(node->handler == (void *)ptrs_handle_thismember
				|| node->handler == (void *)ptrs_handle_assign_thismember
				|| node->handler == (void *)ptrs_handle_addressof_thismember
				|| node->handler == (void *)ptrs_handle_call_thismember)
				return true;

			if(node != NULL)
				ptrs_error(node, "Cannot access property %s of struct %s\n", member->name, struc->name);
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
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast)
{
	if(struc->memberCount == 0)
		return NULL;

	int i = ptrs_struct_hashName(key) % struc->memberCount;
	while(struc->member[i].name != NULL)
	{
		if(strcmp(struc->member[i].name, key) == 0 && struc->member[i].type != exclude)
		{
			ptrs_struct_canAccess(struc, &struc->member[i], ast);
			return &struc->member[i];
		}

		i = (i + 1) % struc->memberCount;
	}

	return NULL;
}

ptrs_var_t *ptrs_struct_getMember(ptrs_struct_t *struc, ptrs_var_t *result,
	struct ptrs_structmember *member, ptrs_ast_t *ast)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, "Cannot access non-static property of struct %s", struc->name);

		data = struc->staticData;
	}

	ptrs_var_t func;
	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return data + member->offset;
		case PTRS_STRUCTMEMBER_GETTER:
			func.meta.type = PTRS_TYPE_FUNCTION;
			func.value.funcval = member->value.function;
			//TODO
			//return ptrs_callfunc(ast, result, scope, struc, &func, 0, NULL);
			return NULL;
		case PTRS_STRUCTMEMBER_SETTER:
			return NULL;
		case PTRS_STRUCTMEMBER_FUNCTION:
			result->meta.type = PTRS_TYPE_FUNCTION;
			result->value.funcval = member->value.function;
			return result;
		case PTRS_STRUCTMEMBER_ARRAY:
			result->meta.type = PTRS_TYPE_NATIVE;
			result->value.nativeval = data + member->offset;
			result->meta.array.readOnly = false;
			result->meta.array.size = member->value.size;
			return result;
		case PTRS_STRUCTMEMBER_VARARRAY:
			result->meta.type = PTRS_TYPE_POINTER;
			result->value.ptrval = data + member->offset;
			result->meta.array.size = member->value.size / sizeof(ptrs_var_t);
			return result;
		case PTRS_STRUCTMEMBER_TYPED:
			member->value.type->getHandler(data + member->offset, member->value.type->size, result);
			return result;
	}

	return NULL;
}

void ptrs_struct_setMember(ptrs_struct_t *struc, ptrs_var_t *value,
	struct ptrs_structmember *member, ptrs_ast_t *ast)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, "Cannot assign non-static property of struct %s", struc->name);

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
		func.meta.type = PTRS_TYPE_FUNCTION;
		func.value.funcval = member->value.function;
		//TODO
		//ptrs_callfunc(ast, &result, scope, struc, &func, 1, value);
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		ptrs_nativetype_info_t *type = member->value.type;
		type->setHandler(data + member->offset, type->size, value);
	}
	else
	{
		ptrs_error(ast, "Cannot assign to non-variable and non-property struct member");
	}
}

void ptrs_struct_addressOfMember(ptrs_struct_t *struc, ptrs_var_t *result,
	struct ptrs_structmember *member, ptrs_ast_t *ast)
{
	void *data = struc->data;
	if(data == NULL || member->isStatic)
	{
		if(!member->isStatic)
			ptrs_error(ast, "Cannot get address of non-static property of struct %s", struc->name);

		data = struc->staticData;
	}

	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		result->meta.type = PTRS_TYPE_POINTER;
		result->value.ptrval = data + member->offset;
		result->meta.array.size = 1;
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		result->meta.type = PTRS_TYPE_NATIVE;
		result->value.nativeval = data + member->offset;
		result->meta.array.size = member->value.type->size;
	}
	else
	{
		ptrs_error(ast, "Cannot get address of non-property struct member");
	}
}

ptrs_var_t *ptrs_struct_get(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, ast);
	if(member == NULL)
		return NULL;

	return ptrs_struct_getMember(struc, result, member, ast);
}

bool ptrs_struct_set(ptrs_struct_t *struc, ptrs_var_t *value, const char *key, ptrs_ast_t *ast)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast);
	if(member == NULL)
		return false;

	ptrs_struct_setMember(struc, value, member, ast);
	return true;
}

bool ptrs_struct_addressOf(ptrs_struct_t *struc, ptrs_var_t *result, const char *key, ptrs_ast_t *ast)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast);
	if(member == NULL)
		return false;

	ptrs_struct_addressOfMember(struc, result, member, ast);
	return true;
}

ptrs_var_t *ptrs_struct_construct(ptrs_var_t *constructor, struct ptrs_astlist *arguments, bool allocateOnStack,
		ptrs_ast_t *node, ptrs_var_t *result)
{
	//TODO
	return NULL;
}
