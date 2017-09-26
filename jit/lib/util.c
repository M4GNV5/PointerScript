#include <stdarg.h>
#include <assert.h>
#include <jit/jit.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/run.h"
#include "../include/conversion.h"
#include "../include/error.h"

void ptrs_initScope(ptrs_scope_t *scope)
{
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;
	scope->firstAssertion = NULL;
	scope->lastAssertion = NULL;
	scope->indexSize = NULL;
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

	return vartype;
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
	jit_long _constVal = jit_value_get_long_constant(val);
	return *(ptrs_val_t *)&_constVal;
}

ptrs_meta_t ptrs_jit_value_getMetaConstant(jit_value_t meta)
{
	jit_ulong _constMeta = jit_value_get_long_constant(meta);
	return *(ptrs_meta_t *)&_constMeta;
}

ptrs_jit_var_t ptrs_jit_varFromConstant(jit_function_t func, ptrs_var_t val)
{
	ptrs_jit_var_t ret;
	ret.val = jit_const_long(func, long, *(jit_long *)&val.value);
	ret.meta = jit_const_long(func, ulong, *(jit_long *)&val.meta);
	ret.constType = val.meta.type;
	return ret;
}

jit_value_t ptrs_jit_reinterpretCast(jit_function_t func, jit_value_t val, jit_type_t newType)
{
	if(jit_value_get_type(val) == newType)
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
	{
		val = jit_insn_convert(func, val, convertTo, 0);

		if(convertTo != jit_type_long)
			val = ptrs_jit_reinterpretCast(func, val, jit_type_long);
	}

	return val;
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
