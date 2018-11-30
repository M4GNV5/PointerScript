#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <jit/jit.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/run.h"
#include "../include/conversion.h"
#include "../include/error.h"
#include "../include/util.h"

char *ptrs_readFile(const char *path)
{
	FILE *fd = fopen(path, "r");

	if(fd == NULL)
		return NULL;

	fseek(fd, 0, SEEK_END);
	long fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char *content = malloc(fsize + 1);
	fread(content, fsize, 1, fd);
	fclose(fd);
	content[fsize] = 0;

	return content;
}

void ptrs_initScope(ptrs_scope_t *scope, ptrs_scope_t *parent)
{
	scope->loopControlAllowed = false;
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;
	scope->firstAssertion = NULL;
	scope->lastAssertion = NULL;
	scope->returnAddr = NULL;
	scope->indexSize = NULL;

	if(parent != NULL)
	{
		scope->rootFunc = parent->rootFunc;
		scope->rootFrame = parent->rootFrame;
	}
}

jit_type_t ptrs_jit_getVarType()
{
	static jit_type_t vartype = NULL;
	if(vartype == NULL)
	{
		jit_type_t fields[] = {
			jit_type_long,
			jit_type_ulong,
		};

		vartype = jit_type_create_struct(fields, 2, 0);
	}

	return jit_type_copy(vartype);
}

ptrs_jit_var_t ptrs_jit_valToVar(jit_function_t func, jit_value_t val)
{
	assert(jit_value_get_type(val) == ptrs_jit_getVarType());
	jit_value_set_addressable(val);

	jit_value_t addr = jit_insn_address_of(func, val);

	ptrs_jit_var_t ret = {
		.val = jit_insn_load_relative(func, addr, 0, jit_type_long),
		.meta = jit_insn_load_relative(func, addr, sizeof(ptrs_val_t), jit_type_ulong),
		.constType = -1,
	};

	return ret;
}

jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var)
{
	jit_value_t val = jit_value_create(func, ptrs_jit_getVarType());
	jit_value_set_addressable(val);

	jit_value_t addr = jit_insn_address_of(func, val);

	jit_insn_store_relative(func, addr, 0, var.val);
	jit_insn_store_relative(func, addr, sizeof(ptrs_val_t), var.meta);

	return val;
}

ptrs_val_t ptrs_jit_value_getValConstant(jit_value_t val)
{
	switch(jit_type_get_kind(jit_value_get_type(val)))
	{
		case JIT_TYPE_FLOAT32: //this should not happen and will probably not work anyways
			;
			jit_float64 float32Val = jit_value_get_float32_constant(val);
			return *(ptrs_val_t *)&float32Val;
		case JIT_TYPE_FLOAT64:
			;
			jit_float64 float64Val = jit_value_get_float64_constant(val);
			return *(ptrs_val_t *)&float64Val;
		default: //int, uint, long, ulong, pointer
			;
			jit_long longVal = jit_value_get_long_constant(val);
			return *(ptrs_val_t *)&longVal;
	}
}

ptrs_meta_t ptrs_jit_value_getMetaConstant(jit_value_t meta)
{
	jit_ulong _constMeta = jit_value_get_long_constant(meta);
	return *(ptrs_meta_t *)&_constMeta;
}

ptrs_jit_var_t ptrs_jit_varFromConstant(jit_function_t func, ptrs_var_t val)
{
	ptrs_jit_var_t ret;
	if(val.meta.type == PTRS_TYPE_FLOAT)
		ret.val = jit_const_float(func, val.value.floatval);
	else
		ret.val = jit_const_long(func, long, *(jit_long *)&val.value);

	ret.meta = jit_const_long(func, ulong, *(jit_long *)&val.meta);
	ret.constType = val.meta.type;
	return ret;
}

static bool isFloatKind(int kind)
{
	switch(kind)
	{
		case JIT_TYPE_FLOAT32:
		case JIT_TYPE_FLOAT64:
		case JIT_TYPE_NFLOAT:
			return true;

		default:
			return false;
	}
}

void ptrs_jit_assignTypedFromVar(jit_function_t func,
	jit_value_t target, jit_type_t type, ptrs_jit_var_t val)
{
	jit_type_t normalized = jit_type_normalize(jit_type_promote_int(type));
	if(isFloatKind(jit_type_get_kind(normalized)))
		val.val = ptrs_jit_vartof(func, val);
	else
		val.val = ptrs_jit_vartoi(func, val);

	val.val = jit_insn_convert(func, val.val, type, 0);
	jit_insn_store_relative(func, target, 0, val.val);
}

jit_value_t ptrs_jit_reinterpretCast(jit_function_t func, jit_value_t val, jit_type_t newType)
{
	int oldKind = jit_type_get_kind(jit_value_get_type(val));
	int newKind = jit_type_get_kind(newType);

	if(oldKind == newKind)
		return val;

	if(jit_value_is_constant(val))
	{
		jit_constant_t constVal = jit_value_get_constant(val);
		jit_type_t type = jit_type_normalize(jit_type_promote_int(newType));

		if(type == jit_type_int || type == jit_type_uint)
			return jit_value_create_nint_constant(func, newType, constVal.un.int_value);
		else if(type == jit_type_long || type == jit_type_ulong)
			return jit_value_create_long_constant(func, newType, constVal.un.long_value);
		else if(type == jit_type_float64)
			return jit_value_create_float64_constant(func, newType, constVal.un.float64_value);
	}

	bool oldIsFloat = isFloatKind(oldKind);
	bool newIsFloat = isFloatKind(newKind);
	if(oldIsFloat && newIsFloat)
		return jit_insn_convert(func, val, newType, 0);
	else if(!oldIsFloat && !newIsFloat)
		return jit_insn_convert(func, val, newType, 0);

	if(jit_value_is_addressable(val))
	{
		return jit_insn_load_relative(func, jit_insn_address_of(func, val), 0, newType);
	}
	else
	{
		jit_value_t ret = jit_value_create(func, newType);
		jit_value_set_addressable(ret);
		jit_insn_store_relative(func, jit_insn_address_of(func, ret), 0, val);
		return ret;
	}
}

jit_value_t ptrs_jit_normalizeForVar(jit_function_t func, jit_value_t val)
{
	jit_type_t type = jit_value_get_type(val);
	jit_type_t convertTo = jit_type_promote_int(jit_type_normalize(type));

	switch(jit_type_get_kind(convertTo))
	{
		case JIT_TYPE_INT:
			convertTo = jit_type_long;
			break;
		case JIT_TYPE_UINT:
			convertTo = jit_type_ulong;
			break;
		case JIT_TYPE_FLOAT32:
		case JIT_TYPE_FLOAT64:
			convertTo = jit_type_float64;
			break;
		case JIT_TYPE_PTR:
		case JIT_TYPE_LONG:
		case JIT_TYPE_ULONG:
			convertTo = NULL;
			break;
	}

	if(convertTo != NULL)
		val = jit_insn_convert(func, val, convertTo, 0);

	return val;
}

jit_value_t ptrs_jit_allocate(jit_function_t func, jit_value_t size, bool onStack, bool allowReuse)
{
	jit_value_t ret;
	if(onStack)
	{
		if(allowReuse && jit_value_is_constant(size))
			ret = jit_insn_array(func, jit_value_get_nint_constant(size));
		else
			ret = jit_insn_alloca(func, size);
	}
	else
	{
		ptrs_jit_reusableCall(func, malloc, ret,
			jit_type_void_ptr, (jit_type_nuint),
			(size)
		);
	}

	return ret;
}

jit_value_t ptrs_jit_import(ptrs_ast_t *node, jit_function_t func, jit_value_t val, bool asPtr)
{
	if(jit_value_is_constant(val))
	{
		if(asPtr)
			ptrs_error(node, "Cannot get the address of a constant value");
		else
			return val;
	}

	jit_function_t targetFunc = jit_value_get_function(val);
	if(targetFunc == func)
	{
		if(asPtr)
			return jit_insn_address_of(func, val);
		else
			return val;
	}

	jit_value_t ptr = jit_insn_import(func, val);
	if(ptr == NULL)
	{
		const char *funcName = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
		const char *targetFuncName = jit_function_get_meta(targetFunc, PTRS_JIT_FUNCTIONMETA_NAME);
		ptrs_error(node, "Cannot access value of function %s from function %s", targetFuncName, funcName);
	}

	if(asPtr)
		return ptr;
	else
		return jit_insn_load_relative(func, ptr, 0, jit_value_get_type(val));
}

void ptrs_jit_typeSwitch_setup(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t *val, jit_value_t valType,
	int *count, uint8_t *types, jit_label_t *labels,
	size_t argCount, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(val->constType == -1)
	{
		for(int i = *count - 1; i > 0; i--)
		{
			labels[i] = jit_label_undefined;
			jit_insn_branch_if(func,
				jit_insn_eq(func, valType, jit_const_int(func, int, types[i])),
				labels + i
			);
		}

		labels[0] = jit_label_undefined;
		ptrs_jit_vassert(node, func, scope,
			jit_insn_eq(func, valType, jit_const_int(func, int, types[0])),
			argCount, msg, ap
		);
	}
	else
	{
		types[0] = val->constType;
		*count = 1;
	}
}
