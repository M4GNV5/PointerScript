#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../jit.h"

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/astlist.h"
#include "../include/util.h"

ptrs_function_t *ptrs_struct_getOverload(ptrs_var_t *struc, void *handler, bool isInstance)
{
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
			//TODO

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

ptrs_var_t ptrs_struct_getMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member)
{
	if(member->isStatic)
		data = struc->staticData;
	else if(data == NULL)
		ptrs_error(ast, "Cannot access non-static property of struct %s", struc->name);

	ptrs_var_t result;
	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return *(ptrs_var_t *)(data + member->offset);
		case PTRS_STRUCTMEMBER_GETTER:
			//TODO
			return result;
		case PTRS_STRUCTMEMBER_SETTER:
			//TODO
			return result;
		case PTRS_STRUCTMEMBER_FUNCTION:
			//TODO
			return result;
		case PTRS_STRUCTMEMBER_ARRAY:
			result.meta.type = PTRS_TYPE_NATIVE;
			result.value.nativeval = data + member->offset;
			result.meta.array.readOnly = false;
			result.meta.array.size = member->value.array.size;
			return result;
		case PTRS_STRUCTMEMBER_VARARRAY:
			result.meta.type = PTRS_TYPE_POINTER;
			result.value.ptrval = data + member->offset;
			result.meta.array.size = member->value.array.size / sizeof(ptrs_var_t);
			return result;
		case PTRS_STRUCTMEMBER_TYPED:
			member->value.type->getHandler(data + member->offset, member->value.type->size, &result);
			return result;
	}
}

void ptrs_struct_setMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member, ptrs_val_t val, ptrs_meta_t meta)
{
	if(member->isStatic)
		data = struc->staticData;
	else if(data == NULL)
		ptrs_error(ast, "Cannot assign non-static property of struct %s", struc->name);

	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		*(ptrs_val_t *)(data + member->offset) = val;
		*(ptrs_meta_t *)(data + member->offset + sizeof(ptrs_val_t)) = meta;
	}
	else if(member->type == PTRS_STRUCTMEMBER_SETTER)
	{
		//TODO
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		ptrs_nativetype_info_t *type = member->value.type;
		ptrs_var_t result;
		result.value = val;
		result.meta = meta;
		type->setHandler(data + member->offset, type->size, &result);
	}
	else
	{
		ptrs_error(ast, "Cannot assign to non-variable and non-property struct member");
	}
}

ptrs_var_t ptrs_struct_addressOfMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member)
{
	if(member->isStatic)
		data = struc->staticData;
	else if(data == NULL)
		ptrs_error(ast, "Cannot get the address of a non-static property of struct %s", struc->name);

	ptrs_var_t result;
	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		result.meta.type = PTRS_TYPE_POINTER;
		result.meta.array.size = 1;
		result.value.ptrval = data + member->offset;
	}
	else if(member->type == PTRS_STRUCTMEMBER_TYPED)
	{
		result.meta.type = PTRS_TYPE_NATIVE;
		result.meta.array.size = member->value.type->size;
		result.value.nativeval = data + member->offset;
	}
	else
	{
		ptrs_error(ast, "Cannot get address of non-property struct member");
	}
	return result;
}

ptrs_var_t ptrs_struct_get(ptrs_ast_t *ast, void *instance, ptrs_struct_t *struc, const char *key)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, ast);
	if(member == NULL)
		ptrs_error(ast, "Struct %s has no member named %s", struc->name, key);

	return ptrs_struct_getMember(ast, instance, struc, member);
}

void ptrs_struct_set(ptrs_ast_t *ast, void *instance, ptrs_struct_t *struc, const char *key,
	ptrs_val_t val, ptrs_meta_t meta)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast);
	if(member == NULL)
		ptrs_error(ast, "Struct %s has no member named %s", struc->name, key);

	ptrs_struct_setMember(ast, instance, struc, member, val, meta);
}

ptrs_var_t ptrs_struct_addressOf(ptrs_ast_t *ast, void *instance, ptrs_struct_t *struc, const char *key)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast);
	if(member == NULL)
		ptrs_error(ast, "Struct %s has no member named %s", struc->name, key);

	return ptrs_struct_addressOfMember(ast, instance, struc, member);
}

ptrs_jit_var_t ptrs_struct_construct(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t constructor, struct ptrs_astlist *arguments, bool allocateOnStack)
{
	ptrs_jit_typeCheck(ast, func, scope, constructor, PTRS_TYPE_STRUCT,
		1, "Value of type %t is not a constructor", TYPECHECK_TYPE);

	jit_value_t struc = ptrs_jit_getMetaPointer(func, constructor.meta);
	jit_value_t size = jit_insn_load_relative(func, struc,
		offsetof(ptrs_struct_t, size), jit_type_uint);

	jit_value_t instance = ptrs_jit_allocate(func, size, allocateOnStack, false);

	//TODO constructor

	ptrs_jit_var_t ret = {instance, constructor.meta, PTRS_TYPE_STRUCT};
	return ret;
}
