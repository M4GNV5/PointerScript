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

static void ptrs_arglist_handle(jit_function_t func, ptrs_scope_t *scope,
	struct ptrs_astlist *curr, ptrs_jit_var_t *buff)
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
	ptrs_nativetype_info_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args)
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
				paramDef[0] = jit_type_void_ptr;
				_args[0] = thisPtr;

				for(int i = 0; args != NULL; i++)
				{
					//if(args->expand) //TODO
					//if(args->lazy) //TODO

					ptrs_jit_var_t val;
					if(args->entry == NULL)
					{
						val.val = jit_const_int(func, long, 0);
						val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
					}
					else
					{
						val = args->entry->handler(args->entry, func, scope);
					}

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
	jit_value_t thisPtr, jit_function_t callee, struct ptrs_astlist *args)
{
	int minArgs = jit_type_num_params(jit_function_get_signature(callee));
	int narg = ptrs_arglist_length(args) * 2 + 1;
	if(minArgs > narg)
		narg = minArgs;

	jit_value_t _args[narg];
	_args[0] = thisPtr;

	for(int i = 0; i < narg / 2; i++)
	{
		//if(args->expand) //TODO
		//if(args->lazy) //TODO
		ptrs_jit_var_t val;
		if(args == NULL || args->entry == NULL)
		{
			val.val = jit_const_int(func, long, 0);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		}
		else
		{
			val = args->entry->handler(args->entry, func, scope);
		}

		_args[i * 2 + 1] = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
		_args[i * 2 + 2] = val.meta;

		if(args != NULL)
			args = args->next;
	}

	const char *name = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_NAME);
	jit_value_t ret = jit_insn_call(func, name, callee, NULL, _args, narg, 0);

	return ptrs_jit_valToVar(func, ret);
}

ptrs_jit_var_t ptrs_jit_ncallnested(jit_function_t func,
	jit_value_t thisPtr, jit_function_t callee, size_t narg, ptrs_jit_var_t *args)
{
	jit_value_t _args[narg * 2 + 1];
	_args[0] = thisPtr;
	for(int i = 0; i < narg; i++)
	{
		_args[i * 2 + 1] = args[i].val;
		_args[i * 2 + 2] = args[i].meta;
	}

	const char *name = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_NAME);
	jit_value_t ret = jit_insn_call(func, name, callee, NULL, _args, narg * 2 + 1, 0);

	return ptrs_jit_valToVar(func, ret);
}

jit_function_t ptrs_jit_createFunction(ptrs_ast_t *node, jit_function_t parent,
	jit_type_t signature, const char *name)
{
	jit_function_t func;
	if(parent == NULL)
		func = jit_function_create(ptrs_jit_context, signature);
	else
		func = jit_function_create_nested(ptrs_jit_context, signature, parent);

	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_NAME, (char *)name, NULL, 0);
	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_AST, node, NULL, 0);
	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_CLOSURE, NULL, NULL, 0);

	return func;
}

jit_function_t ptrs_jit_compileFunction(ptrs_ast_t *node, jit_function_t parent, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType)
{
	jit_type_t paramDef[ast->argc * 2 + 1];
	paramDef[0] = jit_type_void_ptr; //reserved
	for(int i = 1; i < ast->argc * 2 + 1;)
	{
		paramDef[i++] = jit_type_long;
		paramDef[i++] = jit_type_ulong;
	}

	//TODO variadic functions

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl,
		ptrs_jit_getVarType(), paramDef, ast->argc * 2 + 1, 0);
	jit_function_t func = ptrs_jit_createFunction(node, parent, signature, ast->name);

	ptrs_scope_t funcScope;
	ptrs_initScope(&funcScope, scope);

	if(thisType == NULL)
	{
		ast->thisVal.val = jit_const_long(func, long, 0);
		ast->thisVal.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		ast->thisVal.constType = PTRS_TYPE_UNDEFINED;
	}
	else
	{
		ast->thisVal.val = jit_value_get_param(func, 0);
		ast->thisVal.meta = ptrs_jit_const_pointerMeta(func, PTRS_TYPE_STRUCT, thisType);
		ast->thisVal.constType = PTRS_TYPE_STRUCT;
	}

	for(int i = 0; i < ast->argc; i++)
	{
		ast->args[i].val = jit_value_get_param(func, i * 2 + 1);
		ast->args[i].meta = jit_value_get_param(func, i * 2 + 2);
		ast->args[i].constType = -1;

		if(ast->argv != NULL && ast->argv[i] != NULL)
		{
			jit_label_t given = jit_label_undefined;
			jit_value_t isGiven = ptrs_jit_hasType(func, ast->args[i].meta, PTRS_TYPE_UNDEFINED);
			jit_insn_branch_if_not(func, isGiven, &given);

			ptrs_jit_var_t val = ast->argv[i]->handler(ast->argv[i], func, &funcScope);
			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
			jit_insn_store(func, ast->args[i].val, val.val);
			jit_insn_store(func, ast->args[i].meta, val.meta);

			jit_insn_label(func, &given);
		}
	}

	ast->body->handler(ast->body, func, &funcScope);
	jit_insn_default_return(func);

	ptrs_jit_placeAssertions(func, &funcScope);

	if(ptrs_compileAot && jit_function_compile(func) == 0)
		ptrs_error(node, "Failed compiling function %s", ast->name);

	return func;
}
