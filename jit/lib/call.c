#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"

ptrs_jit_var_t ptrs_jit_callfunc(jit_function_t func, jit_value_t val, size_t narg, ptrs_jit_var_t *args)
{
	jit_type_t paramDef[narg * 2];
	for(int i = 0; i < narg; i++)
	{
		paramDef[i * 2] = jit_type_long;
		paramDef[i * 2 + 1] = jit_type_ulong;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, paramDef, narg * 2, 1);
	jit_insn_call_indirect(func, val, signature, (jit_value_t *)args, narg * 2, 0);
	jit_type_free(signature);

	//TODO return value
}

ptrs_jit_var_t ptrs_jit_callnative(jit_function_t func, jit_value_t val, size_t narg, ptrs_jit_var_t *args)
{
	jit_type_t paramDef[narg];
	jit_value_t _args[narg];
	for(int i = 0; i < narg; i++)
	{
		paramDef[i] = jit_type_long;
		_args[i] = args[i].val;
	}

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, paramDef, narg, 1);
	jit_insn_call_indirect(func, val, signature, _args, narg, 0);
	jit_type_free(signature);

	//TODO return value
}

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func,
	jit_value_t val, jit_value_t meta, size_t narg, ptrs_jit_var_t *args)
{
	jit_label_t isNative = jit_label_undefined;

	jit_value_t type = ptrs_jit_getType(func, meta);
	jit_insn_branch_if(func, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_NATIVE)), &isNative);

	ptrs_jit_assert(node, func, jit_insn_eq(func, type, jit_const_long(func, ulong, PTRS_TYPE_FUNCTION)),
		1, "Cannot call value of type %t", type);

	//calling a pointerscript function
	ptrs_callfunc(func, val, narg, args);

	//calling a native function
	jit_insn_label(func, &isNative);
	ptrs_callnative(func, val, narg, args);
}

ptrs_jit_var_t ptrs_jit_vcall(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t val, jit_value_t meta, struct ptrs_astlist *args)
{
	int len = 0;
	struct ptrs_astlist *curr = args;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	ptrs_jit_var_t _args[len];

	curr = args;
	for(int i = 0; curr != NULL; i++)
	{
		//if(list->expand) //TODO
		//if(list->lazy) //TODO

		_args[i] = curr->entry->handler(curr->entry, func, scope);

		curr = curr->next;
	}

	ptrs_call(node, func, val, meta, len, _args);
}
