#include <assert.h>
#include <jit/jit.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/run.h"

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

	jit_value_t addr = jit_insn_address_of(func, val);

	ptrs_jit_var_t ret = {
		.val = jit_insn_load_relative(func, addr, 0, jit_type_long),
		.meta = jit_insn_load_relative(func, addr, sizeof(ptrs_val_t), jit_type_ulong),
	};

	return ret;
}

jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var)
{
	jit_value_t val = jit_value_create(func, ptrs_jit_getVarType());
	jit_value_t addr = jit_insn_address_of(func, val);

	jit_insn_store_relative(func, addr, 0, var.val);
	jit_insn_store_relative(func, addr, sizeof(ptrs_val_t), var.meta);

	return val;
}

jit_function_t ptrs_jit_createTrampoline(ptrs_function_t *funcAst, jit_function_t func)
{
	jit_type_t argDef[funcAst->argc * 2];
	for(int i = 0; i < funcAst->argc; i++)
	{
		argDef[i] = jit_type_long;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_long, argDef, funcAst->argc, 1);
	jit_function_t closure = jit_function_create(ptrs_jit_context, signature);
	jit_type_free(signature);

	jit_function_set_meta(closure, PTRS_JIT_FUNCTIONMETA_NAME, "(trampoline)", NULL, 0);
	jit_function_set_meta(closure, PTRS_JIT_FUNCTIONMETA_FILE, "(builtin)", NULL, 0);
	jit_function_set_meta(closure, PTRS_JIT_FUNCTIONMETA_CLOSURE, NULL, NULL, 0);

	jit_value_t args[funcAst->argc * 2];
	jit_value_t meta = ptrs_jit_const_meta(closure, PTRS_TYPE_INT);

	for(int i = 0; i < funcAst->argc; i++)
	{
		argDef[i * 2] = jit_type_long;
		argDef[i * 2 + 1] = jit_type_ulong;

		args[i * 2] = jit_value_get_param(closure, i);
		args[i * 2 + 1] = meta;
	}

	signature = jit_type_create_signature(jit_abi_cdecl, ptrs_jit_getVarType(), argDef, funcAst->argc * 2, 1);
	jit_insn_return(closure, jit_insn_call(closure, funcAst->name, func, signature, args, funcAst->argc * 2, JIT_CALL_TAIL));
	jit_type_free(signature);

	return closure;
}
