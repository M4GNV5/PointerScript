#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../jit.h"

#include "../include/error.h"
#include "../include/conversion.h"
#include "../include/astlist.h"
#include "../include/util.h"
#include "../include/call.h"

jit_function_t ptrs_struct_getOverload(ptrs_struct_t *struc, void *handler, bool isInstance)
{
	struct ptrs_opoverload *curr = struc->overloads;
	while(curr != NULL)
	{
		if(curr->op == handler && (isInstance || curr->isStatic))
			return curr->handlerFunc;
		curr = curr->next;
	}
	return NULL;
}

void *ptrs_struct_getOverloadClosure(ptrs_struct_t *struc, void *handler, bool isInstance)
{
	jit_function_t func = ptrs_struct_getOverload(struc, handler, isInstance);
	if(func == NULL)
		return NULL;
	else
		return jit_function_to_closure(func);
}

const char const *accessorNames[] = {
	"public",
	"internal",
	"private"
};
bool ptrs_struct_canAccess(ptrs_ast_t *node, ptrs_struct_t *struc, struct ptrs_structmember *member)
{
	switch(member->protection)
	{
		case 0:
			return true;
		case 1:
			if(strcmp(node->file, struc->ast->file) == 0)
				return true;
			else
				break;
		default:
			if(strcmp(node->file, struc->ast->file) == 0
				&& node->codepos >= struc->ast->codepos
				&& node->codepos < struc->lastCodepos)
				return true;
			else
				break;
			break;
	}

	if(node == NULL)
		return false;

	ptrs_error(node, "Cannot access %s property %s of struct %s\n",
		accessorNames[member->protection], member->name, struc->name);
}

size_t ptrs_struct_hashName(const char *key)
{
	//see https://stackoverflow.com/a/2624210
	size_t hash = 7;
	while(*key != 0)
	{
		hash = hash * 31 + *key;
		key++;
	}

	return hash;
}

struct ptrs_structmember *ptrs_struct_find(ptrs_struct_t *struc, const char *key,
	enum ptrs_structmembertype exclude, ptrs_ast_t *ast)
{
	if(struc->memberCount == 0)
		return NULL;

	size_t i = ptrs_struct_hashName(key) % struc->memberCount;
	struct ptrs_structmember *ignored = NULL;
	while(struc->member[i].name != NULL)
	{
		if(strcmp(struc->member[i].name, key) == 0)
		{
			if(struc->member[i].type == exclude)
				ignored = &struc->member[i];
			else if(ptrs_struct_canAccess(ast, struc, &struc->member[i]))
				return &struc->member[i];
		}

		i = (i + 1) % struc->memberCount;
	}

	return ignored;
}

bool ptrs_struct_hasKey(void *data, ptrs_struct_t *struc,
	const char *key, ptrs_meta_t keyMeta, ptrs_ast_t *ast)
{
	if(ptrs_struct_find(struc, key, -1, ast) != NULL)
		return true;

	jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_op_in, data != NULL);
	if(overload == NULL)
		return false;

	ptrs_var_t result;
	ptrs_jit_applyNested(overload, &result,
		struc->parentFrame, data, (&key, &keyMeta));

	return ptrs_vartob(result.value, result.meta);
}

ptrs_var_t ptrs_struct_getMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member)
{
	if(member->isStatic)
		data = struc->staticData;
	else if(data == NULL)
		ptrs_error(ast, "Property %s of struct %s is only available on instances", member->name, struc->name);

	ptrs_var_t result;
	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_VAR:
			return *(ptrs_var_t *)(data + member->offset);

		case PTRS_STRUCTMEMBER_GETTER:
			ptrs_jit_applyNested(member->value.function.func, &result,
				struc->parentFrame, data, ());
			return result;

		case PTRS_STRUCTMEMBER_FUNCTION:
			result.value.nativeval = jit_function_to_closure(member->value.function.func);
			*(uint64_t *)&result.meta = ptrs_const_pointerMeta(PTRS_TYPE_FUNCTION, struc->parentFrame);
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
		ptrs_error(ast, "Property %s of struct %s is only available on instances", member->name, struc->name);

	if(member->type == PTRS_STRUCTMEMBER_VAR)
	{
		*(ptrs_val_t *)(data + member->offset) = val;
		*(ptrs_meta_t *)(data + member->offset + sizeof(ptrs_val_t)) = meta;
	}
	else if(member->type == PTRS_STRUCTMEMBER_SETTER)
	{
		ptrs_var_t result;
		ptrs_jit_applyNested(member->value.function.func, &result,
			struc->parentFrame, data, (&val, &meta));
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
		ptrs_error(ast, "Property %s of struct %s is not a valid lvalue", member->name, struc->name);
	}
}

ptrs_var_t ptrs_struct_addressOfMember(ptrs_ast_t *ast, void *data, ptrs_struct_t *struc,
	struct ptrs_structmember *member)
{
	if(member->isStatic)
		data = struc->staticData;
	else if(data == NULL)
		ptrs_error(ast, "Property %s of struct %s is only available on instances", member->name, struc->name);

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
		ptrs_error(ast, "Property %s of struct %s is not a valid lvalue", member->name, struc->name);
	}
	return result;
}

ptrs_var_t ptrs_struct_get(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key)
{
	if(meta.type != PTRS_TYPE_STRUCT)
		ptrs_error(ast, "Cannot get property %s of a value of type %t", key, meta.type);
	ptrs_struct_t *struc = ptrs_meta_getPointer(meta);

	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, ast);
	if(member == NULL)
	{
		jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_member, instance != NULL);
		if(overload == NULL)
			ptrs_error(ast, "Struct %s has no property named %s", struc->name, key);

		ptrs_var_t result;
		uint64_t nameMeta = ptrs_const_arrayMeta(PTRS_TYPE_NATIVE, true, 0);
		ptrs_jit_applyNested(overload, &result, struc->parentFrame, instance, (&key, &nameMeta));
		return result;
	}
	else if(member->type == PTRS_STRUCTMEMBER_SETTER)
	{
		ptrs_error(ast, "Cannot get setter only property %s of struct %s", key, struc->name);
	}

	return ptrs_struct_getMember(ast, instance, struc, member);
}
ptrs_jit_var_t ptrs_jit_struct_get(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal)
{
	if(jit_value_is_constant(base.meta) && jit_value_is_constant(keyVal))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		const char *key = (const char *)jit_value_get_nint_constant(keyVal);

		if(meta.type != PTRS_TYPE_STRUCT)
			ptrs_error(node, "Cannot get property %s of value of type %t", key, meta.type);

		struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, node);
		if(member == NULL)
		{
			bool isInstance = !jit_value_is_constant(base.val) || jit_value_is_true(base.val);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_member, isInstance);
			if(overload == NULL)
				ptrs_error(node, "Struct %s has no property named %s", struc->name, key);

			ptrs_jit_var_t arg = {
				.val = keyVal,
				.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, true, 0),
				.constType = PTRS_TYPE_NATIVE
			};
			return ptrs_jit_ncallnested(func, base.val, overload, 1, &arg);
		}
		else if(member->type == PTRS_STRUCTMEMBER_SETTER)
		{
			ptrs_error(node, "Cannot get setter only property %s of struct %s", key, struc->name);
		}

		jit_value_t data;
		if(member->isStatic)
		{
			data = jit_const_int(func, void_ptr, (uintptr_t)struc->staticData);
		}
		else if(jit_value_is_constant(base.val) && !jit_value_is_true(base.val))
		{
			ptrs_error(node, "Property %s of struct %s is only available on instances",
				member->name, struc->name);
		}
		else
		{
			data = base.val;
		}

		ptrs_jit_var_t result;
		switch(member->type)
		{
			case PTRS_STRUCTMEMBER_VAR:
				result.val = jit_insn_load_relative(func, data, member->offset, jit_type_long);
				result.meta = jit_insn_load_relative(func, data,
				member->offset + sizeof(ptrs_val_t), jit_type_long);
				result.constType = -1;
				return result;

			case PTRS_STRUCTMEMBER_GETTER:
				return ptrs_jit_callnested(func, scope, base.val, member->value.function.func, NULL);

			case PTRS_STRUCTMEMBER_FUNCTION:
				;
				jit_function_t target = member->value.function.func;
				result.val = jit_const_long(func, long, (uintptr_t)jit_function_to_closure(target));
				result.meta = ptrs_jit_pointerMeta(func,
					jit_const_long(func, ulong, PTRS_TYPE_FUNCTION),
					jit_insn_get_parent_frame_pointer_of(func, target)
				);
				result.constType = PTRS_TYPE_FUNCTION;
				return result;

			case PTRS_STRUCTMEMBER_ARRAY:
				result.val = jit_insn_add_relative(func, data, member->offset);
				result.meta = ptrs_jit_const_arrayMeta(func,
					PTRS_TYPE_NATIVE,
					false,
					member->value.array.size
				);
				result.constType = PTRS_TYPE_NATIVE;
				return result;

			case PTRS_STRUCTMEMBER_VARARRAY:
				result.val = jit_insn_add_relative(func, data, member->offset);
				result.meta = ptrs_jit_const_arrayMeta(func,
					PTRS_TYPE_POINTER,
					false,
					member->value.array.size / sizeof(ptrs_var_t)
				);
				result.constType = PTRS_TYPE_POINTER;
				return result;

			case PTRS_STRUCTMEMBER_TYPED:
				result.val = jit_insn_load_relative(func, data, member->offset, member->value.type->jitType);
				result.val = ptrs_jit_normalizeForVar(func, result.val);
				result.meta = ptrs_jit_const_meta(func, member->value.type->varType);
				result.constType = member->value.type->varType;
				return result;
		}
	}
	else
	{
		jit_value_t ret;
		jit_value_t astVal = jit_const_int(func, void_ptr, (uintptr_t)node);
		ptrs_jit_reusableCall(func, ptrs_struct_get, ret, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_void_ptr),
			(astVal, base.val, base.meta, keyVal)
		);

		return ptrs_jit_valToVar(func, ret);
	}
}

void ptrs_struct_set(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key,
	ptrs_val_t val, ptrs_meta_t valMeta)
{
	if(meta.type != PTRS_TYPE_STRUCT)
		ptrs_error(ast, "Cannot set property %s of a value of type %t", key, meta.type);
	ptrs_struct_t *struc = ptrs_meta_getPointer(meta);

	struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, ast);
	if(member == NULL)
	{
		jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_assign_member, instance != NULL);
		if(overload == NULL)
			ptrs_error(ast, "Struct %s has no property named %s", struc->name, key);

		ptrs_var_t result;
		uint64_t nameMeta = ptrs_const_arrayMeta(PTRS_TYPE_NATIVE, true, 0);
		ptrs_jit_applyNested(overload, &result, struc->parentFrame,
			instance, (&key, &nameMeta, &val, &valMeta));
		return;
	}
	else if(member->type == PTRS_STRUCTMEMBER_GETTER)
	{
		ptrs_error(ast, "Cannot set getter only property %s of struct %s", key, struc->name);
	}

	ptrs_struct_setMember(ast, instance, struc, member, val, valMeta);
}
void ptrs_jit_struct_set(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal, ptrs_jit_var_t value)
{
	if(jit_value_is_constant(base.meta) && jit_value_is_constant(keyVal))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		const char *key = (const char *)jit_value_get_nint_constant(keyVal);

		if(meta.type != PTRS_TYPE_STRUCT)
			ptrs_error(node, "Cannot set property %s of value of type %t", key, meta.type);

		struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_GETTER, node);
		if(member == NULL)
		{
			bool isInstance = !jit_value_is_constant(base.val) || jit_value_is_true(base.val);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_assign_member, isInstance);
			if(overload == NULL)
				ptrs_error(node, "Struct %s has no property named %s", struc->name, key);

			ptrs_jit_var_t args[2];
			args[0].constType = PTRS_TYPE_NATIVE;
			args[0].val = keyVal;
			args[0].meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, true, 0);
			args[1] = value;

			ptrs_jit_ncallnested(func, base.val, overload, 2, args);
			return;
		}
		else if(member->type == PTRS_STRUCTMEMBER_GETTER)
		{
			ptrs_error(node, "Cannot set getter only property %s of struct %s", key, struc->name);
		}

		jit_value_t data;
		if(member->isStatic)
		{
			data = jit_const_int(func, void_ptr, (uintptr_t)struc->staticData);
		}
		else if(jit_value_is_constant(base.val) && !jit_value_is_true(base.val))
		{
			ptrs_error(node, "Property %s of struct %s is only available on instances",
				member->name, struc->name);
		}
		else
		{
			data = base.val;
		}

		if(member->type == PTRS_STRUCTMEMBER_VAR)
		{
			jit_insn_store_relative(func, data, member->offset, value.val);
			jit_insn_store_relative(func, data, member->offset + sizeof(ptrs_val_t), value.meta);
		}
		else if(member->type == PTRS_STRUCTMEMBER_SETTER)
		{
			jit_value_t args[3] = {base.val, value.val, value.meta};

			const char *name = jit_function_get_meta(member->value.function.func, PTRS_JIT_FUNCTIONMETA_NAME);
			jit_insn_call(func, name, member->value.function.func, NULL, args, 3, 0);
		}
		else if(member->type == PTRS_STRUCTMEMBER_TYPED)
		{
			ptrs_nativetype_info_t *type = member->value.type;
			jit_value_t val;
			if(type->varType == PTRS_TYPE_FLOAT)
				val = ptrs_jit_vartof(func, value);
			else
				val = ptrs_jit_vartoi(func, value);

			val = jit_insn_convert(func, val, type->jitType, 0);
			jit_insn_store_relative(func, data, member->offset, val);
		}
		else
		{
			ptrs_error(node, "Property %s of struct %s is not a valid lvalue", member->name, struc->name);
		}
	}
	else
	{
		ptrs_jit_reusableCallVoid(func, ptrs_struct_set,
			(
				jit_type_void_ptr,
				jit_type_long,
				jit_type_ulong,
				jit_type_void_ptr,
				jit_type_long,
				jit_type_ulong
			), (
				jit_const_int(func, void_ptr, (uintptr_t)node),
				base.val,
				base.meta,
				keyVal,
				ptrs_jit_reinterpretCast(func, value.val, jit_type_long),
				value.meta
			)
		);
	}
}

ptrs_var_t ptrs_struct_addressOf(ptrs_ast_t *ast, void *instance, ptrs_meta_t meta, const char *key)
{
	if(meta.type != PTRS_TYPE_STRUCT)
		ptrs_error(ast, "Cannot get the address of property %s of value of type %t", key, meta.type);
	ptrs_struct_t *struc = ptrs_meta_getPointer(meta);

	struct ptrs_structmember *member = ptrs_struct_find(struc, key, -1, ast);
	if(member == NULL)
	{
		jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_addressof_member, instance != NULL);
		if(overload == NULL)
			ptrs_error(ast, "Struct %s has no property named %s", struc->name, key);

		ptrs_var_t result;
		uint64_t nameMeta = ptrs_const_arrayMeta(PTRS_TYPE_NATIVE, true, 0);
		ptrs_jit_applyNested(overload, &result, struc->parentFrame, instance, (&key, &nameMeta));
		return result;
	}

	return ptrs_struct_addressOfMember(ast, instance, struc, member);
}
ptrs_jit_var_t ptrs_jit_struct_addressof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal)
{
	if(jit_value_is_constant(base.meta) && jit_value_is_constant(keyVal))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		const char *key = (const char *)jit_value_get_nint_constant(keyVal);

		if(meta.type != PTRS_TYPE_STRUCT)
			ptrs_error(node, "Cannot get property %s of value of type %t", key, meta.type);

		struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, node);
		if(member == NULL)
		{
			bool isInstance = !jit_value_is_constant(base.val) || jit_value_is_true(base.val);
			jit_function_t overload = ptrs_struct_getOverload(struc, ptrs_handle_addressof_member, isInstance);
			if(overload == NULL)
				ptrs_error(node, "Struct %s has no property named %s", struc->name, key);

			ptrs_jit_var_t arg = {
				.val = keyVal,
				.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, true, 0),
				.constType = PTRS_TYPE_NATIVE
			};
			return ptrs_jit_ncallnested(func, base.val, overload, 1, &arg);
		}

		jit_value_t data;
		if(member->isStatic)
		{
			data = jit_const_int(func, void_ptr, (uintptr_t)struc->staticData);
		}
		else if(jit_value_is_constant(base.val) && !jit_value_is_true(base.val))
		{
			ptrs_error(node, "Property %s of struct %s is only available on instances",
				member->name, struc->name);
		}
		else
		{
			data = base.val;
		}

		ptrs_jit_var_t result;
		if(member->type == PTRS_STRUCTMEMBER_VAR)
		{
			result.constType = PTRS_TYPE_POINTER;
			result.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_POINTER, false, 1);
			result.val = jit_insn_add_relative(func, data, member->offset);
		}
		else if(member->type == PTRS_STRUCTMEMBER_TYPED)
		{
			result.constType = PTRS_TYPE_NATIVE;
			result.meta = ptrs_jit_const_arrayMeta(func, PTRS_TYPE_NATIVE, false, member->value.type->size);
			result.val = jit_insn_add_relative(func, data, member->offset);
		}
		else
		{
			ptrs_error(node, "Property %s of struct %s is not a valid lvalue", member->name, struc->name);
		}
		return result;
	}
	else
	{
		jit_value_t ret;
		jit_value_t astVal = jit_const_int(func, void_ptr, (uintptr_t)node);
		ptrs_jit_reusableCall(func, ptrs_struct_addressOf, ret, ptrs_jit_getVarType(),
			(jit_type_void_ptr, jit_type_long, jit_type_ulong, jit_type_void_ptr),
			(astVal, base.val, base.meta, keyVal)
		);

		return ptrs_jit_valToVar(func, ret);
	}
}

ptrs_jit_var_t ptrs_jit_struct_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t base, jit_value_t keyVal, ptrs_nativetype_info_t *retType, struct ptrs_astlist *args)
{
	if(jit_value_is_constant(base.meta) && jit_value_is_constant(keyVal))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(base.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		const char *key = (const char *)jit_value_get_nint_constant(keyVal);

		if(meta.type != PTRS_TYPE_STRUCT)
			ptrs_error(node, "Cannot call property %s of value of type %t", key, meta.type);

		struct ptrs_structmember *member = ptrs_struct_find(struc, key, PTRS_STRUCTMEMBER_SETTER, node);
		if(member == NULL || member->type != PTRS_STRUCTMEMBER_FUNCTION)
		{
			ptrs_jit_var_t callee = ptrs_jit_struct_get(node, func, scope, base, keyVal);
			return ptrs_jit_call(node, func, scope, retType, base.val, callee, args);
		}
		else if(!member->isStatic && jit_value_is_constant(base.val) && !jit_value_is_true(base.val))
		{
			ptrs_error(node, "Property %s of struct %s is only available on instances", member->name, struc->name);
		}
		else //member->type == PTRS_STRUCTMEMBER_FUNCTION
		{
			return ptrs_jit_callnested(func, scope, base.val, member->value.function.func, args);
		}
	}
	else
	{
		
		ptrs_jit_var_t callee = ptrs_jit_struct_get(node, func, scope, base, keyVal);
		return ptrs_jit_call(node, func, scope, retType, base.val, callee, args);
	}
}

ptrs_jit_var_t ptrs_struct_construct(ptrs_ast_t *ast, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t constructor, struct ptrs_astlist *arguments, bool allocateOnStack)
{
	ptrs_jit_typeCheck(ast, func, scope, constructor, PTRS_TYPE_STRUCT,
		"Value of type %t is not a constructor");

	jit_value_t instance;
	if(jit_value_is_constant(constructor.meta))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(constructor.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);

		jit_value_t size = jit_const_int(func, nuint, struc->size);
		instance = ptrs_jit_allocate(func, size, allocateOnStack, false);

		jit_function_t ctor = ptrs_struct_getOverload(struc, ptrs_handle_new, true);
		if(ctor != NULL)
			ptrs_jit_callnested(func, scope, instance, ctor, arguments);
	}
	else
	{
		jit_value_t struc = ptrs_jit_getMetaPointer(func, constructor.meta);
		jit_value_t size = jit_insn_load_relative(func, struc,
			offsetof(ptrs_struct_t, size), jit_type_uint);

		instance = ptrs_jit_allocate(func, size, allocateOnStack, false);

		ptrs_jit_var_t ctor;
		ptrs_jit_reusableCall(func, ptrs_struct_getOverloadClosure, ctor.val,
			jit_type_void_ptr, (jit_type_void_ptr, jit_type_void_ptr, jit_type_int),
			(struc, jit_const_int(func, void_ptr, (uintptr_t)ptrs_handle_new), jit_const_int(func, int, 1))
		);

		jit_label_t noCtor = jit_label_undefined;
		jit_insn_branch_if_not(func, ctor.val, &noCtor);

		ctor.constType = PTRS_TYPE_FUNCTION;
		ctor.meta = ptrs_jit_pointerMeta(func,
			jit_const_long(func, ulong, PTRS_TYPE_FUNCTION),
			jit_insn_get_frame_pointer(func)
		);
		ptrs_jit_call(ast, func, scope, NULL, instance, ctor, arguments);

		jit_insn_label(func, &noCtor);
	}

	ptrs_jit_var_t ret = {instance, constructor.meta, PTRS_TYPE_STRUCT};
	return ret;
}
