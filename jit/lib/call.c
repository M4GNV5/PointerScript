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

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_jit_var_t callee, struct ptrs_astlist *args)
{
	int narg = ptrs_arglist_length(args);
	jit_type_t paramDef[narg * 2 + 1];
	jit_value_t _args[narg * 2 + 1];

	ptrs_jit_var_t ret = {
		jit_value_create(func, jit_type_long),
		jit_value_create(func, jit_type_ulong),
		-1
	};

	ptrs_jit_typeSwitch(node, func, scope, callee,
		(1, "Cannot call variable of type %t", TYPESWITCH_TYPE),
		(PTRS_TYPE_FUNCTION, PTRS_TYPE_NATIVE),
		case PTRS_TYPE_FUNCTION:
			{
				paramDef[0] = jit_type_int; //reserved
				_args[0] = jit_const_int(func, nuint, 0);

				for(int i = 0; args != NULL; i++)
				{
					//if(args->expand) //TODO
					//if(args->lazy) //TODO
					ptrs_jit_var_t val = args->entry->handler(args->entry, func, scope);

					paramDef[i * 2 + 1] = jit_type_long;
					paramDef[i * 2 + 2] = jit_type_ulong;

					_args[i * 2 + 1] = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
					_args[i * 2 + 2] = val.meta;

					args = args->next;
				}

				jit_value_t parentFrame = ptrs_jit_getMetaPointer(func, callee.meta);
				jit_type_t signature = jit_type_create_signature(jit_abi_cdecl,
					ptrs_jit_getVarType(), paramDef, narg * 2 + 1, 1);

				jit_value_t retVal = jit_insn_call_nested_indirect(func, callee.val,
					parentFrame, signature, _args, narg * 2 + 1, 0);
				jit_type_free(signature);

				ptrs_jit_var_t _ret = ptrs_jit_valToVar(func, retVal);
				jit_insn_store(func, ret.val, _ret.val);
				jit_insn_store(func, ret.val, _ret.meta);
			}
			break;

		case PTRS_TYPE_NATIVE:
			{
				for(int i = 0; args != NULL; i++)
				{
					//if(args->expand) //TODO
					//if(args->lazy) //TODO
					ptrs_jit_var_t val = args->entry->handler(args->entry, func, scope);
					args = args->next;

					switch(val.constType)
					{
						case PTRS_TYPE_UNDEFINED:
							paramDef[i] = jit_type_long;
							_args[i] = jit_const_int(func, long, 0);
							break;
						case -1:
						case PTRS_TYPE_INT:
							paramDef[i] = jit_type_long;
							_args[i] = val.val;
							break;
						case PTRS_TYPE_FLOAT:
							paramDef[i] = jit_type_float64;
							_args[i] = ptrs_jit_reinterpretCast(func, val.val, jit_type_float64);
							break;
						default: //pointer type
							paramDef[i] = jit_type_void_ptr;
							_args[i] = val.val;
							break;
					}
				}

				jit_type_t _retType;
				if(retType == NULL)
					_retType = jit_type_long;
				else
					_retType = retType->jitType;

				jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, _retType, paramDef, narg, 1);
				jit_value_t retVal = jit_insn_call_indirect(func, callee.val, signature, _args, narg, 0);
				jit_type_free(signature);

				ptrs_vartype_t retVarType;
				if(retType == NULL)
				{
					retVarType = PTRS_TYPE_INT;
				}
				else
				{
					retVarType = retType->varType;
					retVal = ptrs_jit_normalizeForVar(func, retVal);
				}

				if(callee.constType == PTRS_TYPE_NATIVE)
				{
					ret.constType = retVarType;
					ret.val = retVal;
					ret.meta = ptrs_jit_const_meta(func, retVarType);
				}
				else
				{
					jit_insn_store(func, ret.val, retVal);
					jit_insn_store(func, ret.meta, ptrs_jit_const_meta(func, retVarType));
				}
			}
			break;
	);

	return ret;
}

ptrs_jit_var_t ptrs_jit_callnested(jit_function_t func, ptrs_scope_t *scope,
	jit_function_t callee, struct ptrs_astlist *args)
{
	int narg = ptrs_arglist_length(args);
	jit_value_t _args[narg * 2 + 1];
	_args[0] = jit_const_int(func, nuint, 0); //reserved

	for(int i = 0; args != NULL; i++)
	{
		//if(args->expand) //TODO
		//if(args->lazy) //TODO
		ptrs_jit_var_t val = args->entry->handler(args->entry, func, scope);

		_args[i * 2 + 1] = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
		_args[i * 2 + 2] = val.meta;

		args = args->next;
	}

	const char *name = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_NAME);
	jit_value_t ret = jit_insn_call(func, name, callee, NULL, _args, narg * 2 + 1, 0);

	return ptrs_jit_valToVar(func, ret);
}

jit_function_t ptrs_jit_createTrampoline(ptrs_ast_t *node, ptrs_scope_t *scope,
	ptrs_function_t *funcAst, jit_function_t func)
{
	if(jit_function_get_nested_parent(func) != scope->rootFunc)
		ptrs_error(node, "Cannot create closure of multi-nested function"); //TODO

	jit_type_t argDef[funcAst->argc];
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

	int argCount = funcAst->argc * 2 + 1;
	jit_value_t args[argCount];
	jit_value_t meta = ptrs_jit_const_meta(closure, PTRS_TYPE_INT);

	args[0] = jit_const_int(func, void_ptr, 0);
	for(int i = 0; i < funcAst->argc; i++)
	{
		args[i * 2 + 1] = jit_value_get_param(closure, i);
		args[i * 2 + 2] = meta;
	}

	void *targetClosure = jit_function_to_closure(func);
	jit_value_t target = jit_const_int(closure, void_ptr, (uintptr_t)targetClosure);
	jit_value_t frame = jit_insn_load_relative(closure,
		jit_const_int(closure, void_ptr, (uintptr_t)scope->rootFrame),
		0, jit_type_void_ptr
	);

	signature = jit_function_get_signature(func);
	jit_value_t retVal =  jit_insn_call_nested_indirect(closure, target, frame, signature, args, argCount, 0);
	jit_type_free(signature);

	ptrs_jit_var_t ret = ptrs_jit_valToVar(closure, retVal);
	jit_insn_return(closure, ptrs_jit_vartoi(closure, ret));

	return closure;
}
