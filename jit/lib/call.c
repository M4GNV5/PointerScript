#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"
#include "../include/util.h"
#include "../include/run.h"
#include "../include/conversion.h"

static int ptrs_arglist_length(struct ptrs_astlist *curr)
{
	int len = 0;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	return len;
}

static void ptrs_arglist_handle(jit_function_t func, ptrs_scope_t *scope, struct ptrs_astlist *curr, ptrs_jit_var_t *buff)
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
		switch(args[i].constType)
		{
			case PTRS_TYPE_UNDEFINED:
				paramDef[i] = jit_type_long;
				_args[i] = jit_const_int(func, long, 0);
				break;
			case -1:
			case PTRS_TYPE_INT:
				paramDef[i] = jit_type_long;
				_args[i] = args[i].val;
				break;
			case PTRS_TYPE_FLOAT:
				paramDef[i] = jit_type_float64;
				_args[i] = ptrs_jit_reinterpretCast(func, args[i].val, jit_type_float64);
				break;
			default: //pointer type
				paramDef[i] = jit_type_void_ptr;
				_args[i] = args[i].val;
				break;
		}
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, retType, paramDef, narg, 1);
	jit_value_t ret = jit_insn_call_indirect(func, val, signature, _args, narg, 0);
	jit_type_free(signature);

	return ret;
}

jit_value_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_type_t retType, jit_value_t val, jit_value_t meta, struct ptrs_astlist *args)
{
	int len = ptrs_arglist_length(args);
	ptrs_jit_var_t buff[len];
	ptrs_arglist_handle(func, scope, args, buff);

	return ptrs_jit_call(func, retType, val, len, buff);
}

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, jit_function_t callee, size_t narg, ptrs_jit_var_t *args)
{
	jit_value_t _args[narg * 2 + 1];
	_args[0] = jit_const_int(func, nuint, 0); //reserved

	for(int i = 0; i < narg; i++)
	{
		_args[i * 2 + 1] = args[i].val;
		_args[i * 2 + 2] = args[i].meta;
	}

	const char *name = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_NAME);
	jit_value_t ret = jit_insn_call(func, name, callee, NULL, _args, narg * 2 + 1, 0);

	return ptrs_jit_valToVar(func, ret);
}

ptrs_jit_var_t ptrs_jit_vcallnested(jit_function_t func, ptrs_scope_t *scope,
	jit_function_t callee, struct ptrs_astlist *args)
{
	int len = ptrs_arglist_length(args);
	ptrs_jit_var_t buff[len];
	ptrs_arglist_handle(func, scope, args, buff);

	return ptrs_jit_callnested(func, callee, len, buff);
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
	jit_value_t retVal = jit_insn_call(closure, funcAst->name, func, signature, args, funcAst->argc * 2, 0);
	jit_type_free(signature);

	ptrs_jit_var_t ret = ptrs_jit_valToVar(closure, retVal);
	jit_insn_return(closure, ptrs_jit_vartoi(closure, ret));

	return closure;
}
