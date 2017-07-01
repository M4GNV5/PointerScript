#include <jit/jit.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/run.h"

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

	signature = jit_type_create_signature(jit_abi_cdecl, jit_type_long, argDef, funcAst->argc * 2, 1);
	jit_insn_return(closure, jit_insn_call(closure, funcAst->name, func, signature, args, funcAst->argc * 2, JIT_CALL_TAIL));
	jit_type_free(signature);

	return closure;
}
