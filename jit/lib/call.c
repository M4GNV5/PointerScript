#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"
#include "../include/util.h"
#include "../include/run.h"
#include "../include/astlist.h"
#include "../include/conversion.h"
#include "../include/call.h"

void *ptrs_jit_createCallback(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, void *closure);

void ptrs_arglist_handle(jit_function_t func, ptrs_scope_t *scope,
	struct ptrs_astlist *curr, ptrs_jit_var_t *buff)
{
	for(int i = 0; curr != NULL; i++)
	{
		//if(list->expand) //TODO

		buff[i] = curr->entry->vtable->get(curr->entry, func, scope);

		curr = curr->next;
	}
}

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args)
{
	int narg = ptrs_astlist_length(args);
	jit_type_t paramDef[narg * 2 + 1];
	ptrs_jit_var_t evaledArgs[narg];
	jit_value_t _args[narg * 2 + 1];

	jit_value_t zero = jit_const_int(func, long, 0);
	jit_value_t undefined = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	struct ptrs_astlist *curr = args;
	for(int i = 0; i < narg; i++)
	{
		//if(curr->expand) //TODO

		ptrs_jit_var_t val;
		if(curr->entry == NULL)
		{
			evaledArgs[i].val = zero;
			evaledArgs[i].meta = undefined;
			evaledArgs[i].constType = PTRS_TYPE_UNDEFINED;
		}
		else
		{
			evaledArgs[i] = curr->entry->vtable->get(curr->entry, func, scope);
		}

		curr = curr->next;
	}

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

				for(int i = 0; i < narg; i++)
				{
					paramDef[i * 2 + 1] = jit_type_long;
					paramDef[i * 2 + 2] = jit_type_ulong;

					_args[i * 2 + 1] = ptrs_jit_reinterpretCast(func, evaledArgs[i].val, jit_type_long);
					_args[i * 2 + 2] = evaledArgs[i].meta;
				}

				jit_value_t parentFrame = ptrs_jit_getMetaPointer(func, callee.meta);
				jit_type_t signature = jit_type_create_signature(jit_abi_cdecl,
					ptrs_jit_getVarType(), paramDef, narg * 2 + 1, 0);

				jit_value_t retVal = jit_insn_call_nested_indirect(func, callee.val,
					parentFrame, signature, _args, narg * 2 + 1, 0);
				jit_type_free(signature);

				ptrs_jit_var_t _ret = ptrs_jit_valToVar(func, retVal);
				jit_insn_store(func, ret.val, _ret.val);
				jit_insn_store(func, ret.meta, _ret.meta);
			}
			break;

		case PTRS_TYPE_NATIVE:
			{
				for(int i = 0; i < narg; i++)
				{
					switch(evaledArgs[i].constType)
					{
						case PTRS_TYPE_UNDEFINED:
							paramDef[i] = jit_type_long;
							_args[i] = jit_const_int(func, long, 0);
							break;
						case -1:
							//TODO this should get special care
							/* fallthrough */
						case PTRS_TYPE_INT:
							paramDef[i] = jit_type_long;
							_args[i] = evaledArgs[i].val;
							break;
						case PTRS_TYPE_FLOAT:
							paramDef[i] = jit_type_float64;
							_args[i] = ptrs_jit_reinterpretCast(func, evaledArgs[i].val, jit_type_float64);
							break;
						case PTRS_TYPE_FUNCTION:
							if(jit_value_is_constant(evaledArgs[i].val))
							{
								void *closure = ptrs_jit_value_getValConstant(evaledArgs[i].val).nativeval;
								jit_function_t funcArg = jit_function_from_closure(ptrs_jit_context, closure);
								if(funcArg && jit_function_get_nested_parent(funcArg) == scope->rootFunc)
								{
									void *callback = ptrs_jit_createCallback(node, funcArg, scope, closure);
									paramDef[i] = jit_type_void_ptr;
									_args[i] = jit_const_int(func, void_ptr, (uintptr_t)callback);
									break;
								}
								//TODO create callbacks on the fly
							}
							/* fallthrough */
						default: //pointer type
							paramDef[i] = jit_type_void_ptr;
							_args[i] = evaledArgs[i].val;
							break;
					}
				}

				jit_type_t _retType;
				if(retType == NULL)
					_retType = jit_type_long;
				else
					_retType = retType->jitType;

				jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, _retType, paramDef, narg, 0);
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
	int narg = ptrs_astlist_length(args) * 2 + 1;
	if(minArgs > narg)
		narg = minArgs;

	jit_value_t _args[narg];
	_args[0] = thisPtr;

	for(int i = 0; i < narg / 2; i++)
	{
		//if(args->expand) //TODO
		ptrs_jit_var_t val;
		if(args == NULL || args->entry == NULL)
		{
			val.val = jit_const_int(func, long, 0);
			val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		}
		else
		{
			val = args->entry->vtable->get(args->entry, func, scope);
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

	if(name != NULL)
		jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_NAME, (char *)name, NULL, 0);
	if(node != NULL)
		jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_AST, node, NULL, 0);

	return func;
}

jit_function_t ptrs_jit_createFunctionFromAst(ptrs_ast_t *node, jit_function_t parent,
	ptrs_function_t *ast)
{
	size_t argc = 0;
	ptrs_funcparameter_t *curr = ast->args;
	for(; curr != NULL; argc++)
		curr = curr->next;

	jit_type_t paramDef[argc * 2 + 1];
	paramDef[0] = jit_type_void_ptr; //this pointer
	for(int i = 1; i < argc * 2 + 1;)
	{
		paramDef[i++] = jit_type_long;
		paramDef[i++] = jit_type_ulong;
	}

	if(ast->vararg != NULL)
		ptrs_error(ast->body, "Support for variadic argument functions is not implemented");

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl,
		ptrs_jit_getVarType(), paramDef, argc * 2 + 1, 0);
	return ptrs_jit_createFunction(node, parent, signature, ast->name);
}

void *ptrs_jit_createCallback(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, void *closure)
{
	void *callbackClosure = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_CALLBACK);
	if(callbackClosure != NULL)
		return callbackClosure;

	char *funcName = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
	if(funcName == NULL)
		funcName = "?";

	char callbackName[strlen("(callback )") + strlen(funcName) + 1];
	sprintf(callbackName, "(callback %s)", funcName);

	jit_type_t signature = jit_function_get_signature(func);
	unsigned argc = jit_type_num_params(signature);
	unsigned callbackArgc = (argc - 1) / 2;

	jit_type_t argDef[callbackArgc];
	for(unsigned i = 0; i < callbackArgc; i++)
		argDef[i] = jit_type_long;

	jit_type_t callbackSignature = jit_type_create_signature(jit_abi_cdecl, jit_type_long, argDef, callbackArgc, 0);
	jit_function_t callback = ptrs_jit_createFunction(node, NULL, callbackSignature, strdup(callbackName));

	jit_value_t parentFrame = jit_insn_load_relative(callback,
		jit_const_int(callback, void_ptr, (uintptr_t)scope->rootFrame),
		0, jit_type_void_ptr
	);

	jit_value_t args[argc];
	args[0] = jit_const_int(func, void_ptr, 0);

	jit_value_t meta = ptrs_jit_const_meta(callback, PTRS_TYPE_INT);
	for(unsigned i = 0; i < callbackArgc; i++)
	{
		args[i * 2 + 1] = jit_value_get_param(callback, i);
		args[i * 2 + 2] = meta;
	}

	jit_value_t ret = jit_insn_call_nested_indirect(callback,
		jit_const_int(callback, void_ptr, (uintptr_t)closure), parentFrame,
		signature, args, argc, 0
	);
	ptrs_jit_var_t retVar = ptrs_jit_valToVar(callback, ret);
	jit_insn_return(callback, retVar.val);

	if(ptrs_compileAot && jit_function_compile(callback) == 0)
		ptrs_error(node, "Failed compiling function %s", callbackName);

	callbackClosure = jit_function_to_closure(callback);
	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_CALLBACK, callbackClosure, NULL, 0);
	return callbackClosure;
}

void ptrs_jit_buildFunction(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType)
{
	ptrs_scope_t funcScope;
	ptrs_initScope(&funcScope, scope);

	jit_insn_mark_offset(func, node->codepos);

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

	ptrs_funcparameter_t *curr = ast->args;
	ptrs_jit_var_t param;
	for(int i = 0; curr != NULL; i++)
	{
		param.val = jit_value_get_param(func, i * 2 + 1);
		param.meta = jit_value_get_param(func, i * 2 + 2);
		param.constType = -1;
		param.addressable = 0;

		if(curr->argv != NULL)
		{
			jit_label_t given = jit_label_undefined;
			jit_value_t isGiven = ptrs_jit_hasType(func, param.meta, PTRS_TYPE_UNDEFINED);
			jit_insn_branch_if_not(func, isGiven, &given);

			ptrs_jit_var_t val = curr->argv->vtable->get(curr->argv, func, &funcScope);
			val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
			jit_insn_store(func, param.val, val.val);
			jit_insn_store(func, param.meta, val.meta);

			jit_insn_label(func, &given);
		}

		if(curr->arg.addressable)
		{
			curr->arg.val = jit_value_create(func, ptrs_jit_getVarType());
			jit_value_t ptr = jit_insn_address_of(func, curr->arg.val);
			jit_insn_store_relative(func, ptr, 0, param.val);
			jit_insn_store_relative(func, ptr, sizeof(ptrs_val_t), param.meta);
		}
		else
		{
			curr->arg = param;
		}

		curr = curr->next;
	}

	ast->body->vtable->get(ast->body, func, &funcScope);
	jit_insn_default_return(func);

	ptrs_jit_placeAssertions(func, &funcScope);

	if(ptrs_compileAot && jit_function_compile(func) == 0)
		ptrs_error(node, "Failed compiling function %s", ast->name);
}
