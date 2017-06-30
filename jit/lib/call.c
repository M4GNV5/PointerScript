#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"

static int ptrs_astlist_length(struct ptrs_astlist *curr)
{
	int len = 0;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	return len;
}

static void ptrs_astlist_handle(jit_function_t func, ptrs_scope_t *scope, struct ptrs_astlist *curr, ptrs_jit_var_t *buff)
{
	for(int i = 0; curr != NULL; i++)
	{
		//if(list->expand) //TODO
		//if(list->lazy) //TODO

		buff[i] = curr->entry->handler(curr->entry, func, scope);

		curr = curr->next;
	}
}

jit_value_t ptrs_jit_call(jit_function_t func,
	jit_type_t retType, jit_value_t val, size_t narg, ptrs_jit_var_t *args)
{
	jit_type_t paramDef[narg];
	jit_value_t _args[narg];
	for(int i = 0; i < narg; i++)
	{
		paramDef[i] = jit_type_long;
		_args[i] = args[i].val;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, retType, paramDef, narg, 1);
	jit_value_t ret = jit_insn_call_indirect(func, val, signature, _args, narg, 0);
	jit_type_free(signature);

	return ret;
}

jit_value_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_type_t retType, jit_value_t val, jit_value_t meta, struct ptrs_astlist *args)
{
	int len = ptrs_astlist_length(args);
	ptrs_jit_var_t buff[len];
	ptrs_astlist_handle(func, scope, args, buff);

	return ptrs_jit_call(func, retType, val, len, buff);
}

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, jit_function_t callee, size_t narg, ptrs_jit_var_t *args)
{
	jit_type_t paramDef[narg * 2];
	jit_value_t _args[narg * 2];
	for(int i = 0; i < narg; i++)
	{
		paramDef[i * 2] = jit_type_long;
		paramDef[i * 2 + 1] = jit_type_long;

		_args[i * 2] = args[i].val;
		_args[i * 2 + 1] = args[i].meta;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, /* TODO */ jit_type_void, paramDef, narg * 2, 1);
	const char *name = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_NAME);

	jit_insn_call(func, name, callee, signature, _args, narg * 2, 0);

	jit_type_free(signature);

	ptrs_jit_var_t ret;
	return ret;
}

ptrs_jit_var_t ptrs_jit_vcallnested(jit_function_t func, ptrs_scope_t *scope,
	jit_function_t callee, struct ptrs_astlist *args)
{
	int len = ptrs_astlist_length(args);
	ptrs_jit_var_t buff[len];
	ptrs_astlist_handle(func, scope, args, buff);

	return ptrs_jit_callnested(func, callee, len, buff);
}
