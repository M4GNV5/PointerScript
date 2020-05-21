#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"
#include "../include/util.h"
#include "../include/run.h"
#include "../include/astlist.h"
#include "../include/conversion.h"
#include "../include/call.h"

int ptrs_optimizationLevel = -1;

void *ptrs_jit_createCallback(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, void *closure);

ptrs_jit_var_t ptrs_jit_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_typing_t *retType, jit_value_t thisPtr, ptrs_jit_var_t callee, struct ptrs_astlist *args)
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
							}

							if(!ptrs_compileAot) // XXX hacky to not throw an error with --asmdump --no-aot
							{
								paramDef[i] = jit_type_void_ptr;
								_args[i] = evaledArgs[i].val;
								break;
							}

							//TODO create callbacks at runtime
							curr = args;
							while(i-- > 0)
								curr = curr->next;
							ptrs_error(curr->entry, "Cannot pass nested PointerScript function"
								" as an argument to a native function");

							break;
						default: //pointer type
							paramDef[i] = jit_type_void_ptr;
							_args[i] = evaledArgs[i].val;
							break;
					}
				}

				jit_type_t _retType = ptrs_jit_jitTypeFromTyping(retType);
				ptrs_meta_t retMeta = {0};
				if(retType == NULL || retType->meta.type == (uint8_t)-1)
					retMeta.type = PTRS_TYPE_INT;
				else
					memcpy(&retMeta, &retType->meta, sizeof(ptrs_meta_t));

				jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, _retType, paramDef, narg, 0);
				jit_value_t retVal = jit_insn_call_indirect(func, callee.val, signature, _args, narg, 0);
				jit_type_free(signature);

				retVal = ptrs_jit_normalizeForVar(func, retVal);
				jit_value_t retMetaVal = jit_const_long(func, ulong, *(uint64_t *)&retMeta);

				if(callee.constType == PTRS_TYPE_NATIVE)
				{
					ret.constType = retMeta.type;
					ret.val = retVal;
					ret.meta = retMetaVal;
				}
				else
				{
					jit_insn_store(func, ret.val, retVal);
					jit_insn_store(func, ret.meta, retMetaVal);
				}
			}
			break;
	);

	return ret;
}

static size_t getParameterCount(ptrs_function_t *ast)
{
	size_t count = 0;
	for(ptrs_funcparameter_t *curr = ast->args; curr != NULL; curr = curr->next)
		count++;

	return count;
}
static void checkFunctionParameter(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_jit_var_t *args)
{
	ptrs_funcparameter_t *curr = ast->args;

	for(int i = 0; curr != NULL; i++)
	{
		ptrs_jit_var_t param;
		if(args != NULL)
			param = args[i];
		else
			param = curr->arg;

		jit_value_t paramType = NULL;
		if(param.constType != -1)
			paramType = jit_const_long(func, ulong, param.constType);

		if(curr->argv != NULL)
		{
			if(paramType == NULL)
				paramType = ptrs_jit_getType(func, param.meta);

			if(param.constType == -1)
			{
				jit_label_t given = jit_label_undefined;
				jit_value_t isGiven = jit_insn_ne(func, paramType, jit_const_int(func, ubyte, PTRS_TYPE_UNDEFINED));
				jit_insn_branch_if(func, isGiven, &given);

				ptrs_jit_var_t val = curr->argv->vtable->get(curr->argv, func, scope);
				val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long); // TODO is this needed / a problem?

				jit_insn_store(func, param.val, val.val);
				jit_insn_store(func, param.meta, val.meta);

				jit_insn_label(func, &given);
			}
			else if(param.constType == PTRS_TYPE_UNDEFINED)
			{
				param = curr->argv->vtable->get(curr->argv, func, scope);
			}
		}

		if(ptrs_enableSafety && curr->typing.meta.type != (uint8_t)-1)
		{
			jit_value_t fakeCondition = jit_const_int(func, ubyte, 1);
			jit_value_t funcName = jit_const_int(func, void_ptr, (uintptr_t)ast->name);
			jit_value_t iPlus1 = jit_const_int(func, int, i + 1);
			jit_value_t metaJit = jit_const_long(func, ulong, *(uint64_t *)&curr->typing.meta);
			struct ptrs_assertion *assertion = ptrs_jit_assert(node, func, scope, fakeCondition,
				4, "Function %s requires the %d. parameter to be a of type %m but a variable of type %m was given",
				funcName, iPlus1, metaJit, param.meta);

			ptrs_jit_assertMetaCompatibility(func, assertion, curr->typing.meta, param.meta, paramType);

			param.constType = curr->typing.meta.type;
			// TODO also set param.meta for undefined, int and float params
		}
		else
		{
			param.val = ptrs_jit_reinterpretCast(func, param.val, jit_type_long);
		}

		if(args != NULL)
			args[i] = param;
		else
			curr->arg = param;

		curr = curr->next;
	}
}

static bool retrieveParameterArray(ptrs_function_t *ast, jit_function_t func)
{
	size_t argPos = 1;
	bool usesCustomAbi = false;

	ptrs_funcparameter_t *argDef = ast->args;
	for(; argDef != NULL; argDef = argDef->next)
	{
		ptrs_jit_var_t param;
		param.addressable = false;

		if(argDef != NULL && argDef->typing.meta.type != (uint8_t)-1)
		{
			param.constType = argDef->typing.meta.type;
			usesCustomAbi = true;

			switch(argDef->typing.meta.type)
			{
				case PTRS_TYPE_UNDEFINED:
					param.val = jit_const_long(func, long, 0);
					param.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
					break;
				case PTRS_TYPE_INT:
					param.val = jit_value_get_param(func, argPos);
					param.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
					argPos++;
					break;
				case PTRS_TYPE_FLOAT:
					param.val = jit_value_get_param(func, argPos);
					param.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
					argPos++;
					break;
				case PTRS_TYPE_STRUCT:
					if(ptrs_meta_getPointer(argDef->typing.meta) != NULL)
					{
						param.val = jit_value_get_param(func, argPos);
						param.meta = jit_const_long(func, ulong, *(uint64_t *)&argDef->typing.meta);
						argPos++;
					}
					// else falthrough
				case PTRS_TYPE_NATIVE:
				case PTRS_TYPE_POINTER:
				case PTRS_TYPE_FUNCTION:
					param.val = jit_value_get_param(func, argPos);
					param.meta = jit_value_get_param(func, argPos + 1);
					argPos += 2;
					break;
			}
		}
		else
		{
			param.val = jit_value_get_param(func, argPos);
			param.meta = jit_value_get_param(func, argPos + 1);
			param.constType = -1;
			argPos += 2;
		}

		argDef->arg = param;
	}

	return usesCustomAbi;
}
static size_t fillCustomAbiArgumentArray(ptrs_function_t *ast, jit_type_t *typeDef, jit_value_t *jitArgs,
	jit_value_t thisArg, size_t narg, ptrs_jit_var_t *args)
{
	size_t argCount = 1;
	if(jitArgs != NULL) // TODO are there cases when we dont need this?
		jitArgs[0] = thisArg;
	if(typeDef != NULL)
		typeDef[0] = jit_type_void_ptr;

	ptrs_funcparameter_t *argDef = ast->args;
	for(int i = 0; argDef != NULL; i++)
	{
		if(i >= narg)
			ptrs_error(NULL, "Internal error: parameter counts dont match");

		if(argDef != NULL && argDef->typing.meta.type != (uint8_t)-1)
		{
			switch(argDef->typing.meta.type)
			{
				case PTRS_TYPE_UNDEFINED:
					// nothing
					break;
				case PTRS_TYPE_INT:
					if(jitArgs != NULL && args != NULL)
						jitArgs[argCount] = args[i].val;
					if(typeDef != NULL)
						typeDef[argCount] = jit_type_long;
					argCount++;
					break;
				case PTRS_TYPE_FLOAT:
					if(jitArgs != NULL && args != NULL)
						jitArgs[argCount] = args[i].val;
					if(typeDef != NULL)
						typeDef[argCount] = jit_type_float64;
					argCount++;
					break;
				case PTRS_TYPE_STRUCT:
					if(ptrs_meta_getPointer(argDef->typing.meta) != NULL)
					{
						if(jitArgs != NULL && args != NULL)
							jitArgs[argCount] = args[i].val;
						if(typeDef != NULL)
							typeDef[argCount] = jit_type_long; // TODO set void_ptr?
						argCount++;
						break;
					}
					// else falthrough
				case PTRS_TYPE_NATIVE:
				case PTRS_TYPE_POINTER:
				case PTRS_TYPE_FUNCTION:
					if(jitArgs != NULL && args != NULL)
					{
						jitArgs[argCount] = args[i].val;
						jitArgs[argCount + 1] = args[i].meta;
					}
					if(typeDef != NULL)
					{
						typeDef[argCount] = jit_type_long; // TODO set void_ptr?
						typeDef[argCount + 1] = jit_type_ulong;
					}
					argCount += 2;
					break;
			}
		}
		else
		{
			if(jitArgs != NULL && args != NULL)
			{
				jitArgs[argCount] = args[i].val;
				jitArgs[argCount + 1] = args[i].meta;
			}
			if(typeDef != NULL)
			{
				typeDef[argCount] = jit_type_long;
				typeDef[argCount + 1] = jit_type_ulong;
			}
			argCount += 2;
		}

		argDef = argDef->next;
	}

	return argCount;
}
static ptrs_jit_var_t handleCustomAbiReturn(jit_function_t func, ptrs_function_t *ast, jit_value_t jitRet)
{
	ptrs_jit_var_t ret;
	ret.constType = ast->retType.meta.type;
	ret.addressable = false;

	switch(ast->retType.meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			ret.val = jit_const_long(func, long, 0);
			ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
			break;
		case PTRS_TYPE_INT:
			ret.val = jitRet;
			ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
			break;
		case PTRS_TYPE_FLOAT:
			ret.val = jitRet;
			ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
			break;
		case PTRS_TYPE_STRUCT:
			if(ptrs_meta_getPointer(ast->retType.meta) != NULL)
			{
				ret.val = jitRet;
				ret.meta = jit_const_long(func, ulong, *(uint64_t *)&ast->retType.meta);
				break;
			}
			// else fallthrough
		default:
			ret = ptrs_jit_valToVar(func, jitRet);
			break;
	}

	return ret;
}
static jit_type_t getCustomAbiReturnType(ptrs_function_t *ast)
{
	switch(ast->retType.meta.type)
	{
		case PTRS_TYPE_UNDEFINED:
			return jit_type_void;
		case PTRS_TYPE_INT:
			return jit_type_long;
		case PTRS_TYPE_FLOAT:
			return jit_type_float64;
		case PTRS_TYPE_STRUCT:
			if(ptrs_meta_getPointer(ast->retType.meta) != NULL)
			{
				return jit_type_void_ptr;
			}
			// else fallthrough
		default:
			return ptrs_jit_getVarType();
	}
}
static ptrs_jit_var_t callWithCustomAbi(jit_function_t func, jit_function_t uncheckedCallee, jit_value_t calleeParentFrame,
	ptrs_function_t *ast, jit_value_t thisArg, size_t narg, ptrs_jit_var_t *args, int callflags)
{
	size_t customAbiArgCount = fillCustomAbiArgumentArray(ast, NULL, NULL, thisArg, narg, args);
	jit_type_t typeDef[customAbiArgCount];
	jit_value_t jitArgs[customAbiArgCount];
	fillCustomAbiArgumentArray(ast, typeDef, jitArgs, thisArg, narg, args);

	jit_value_t jitRet;
	if(calleeParentFrame == NULL)
	{
		jitRet = jit_insn_call(func, ast->name, uncheckedCallee, NULL,
			jitArgs, customAbiArgCount, callflags);
	}
	else
	{
		jit_type_t retType = getCustomAbiReturnType(ast);
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, retType, typeDef, customAbiArgCount, 0);

		jit_value_t closure = jit_const_int(func, void_ptr, (uintptr_t)jit_function_to_closure(uncheckedCallee));

		jitRet = jit_insn_call_nested_indirect(func, closure, calleeParentFrame, signature,
			jitArgs, customAbiArgCount, callflags);

		jit_type_free(signature);
	}

	return handleCustomAbiReturn(func, ast, jitRet);
}

void ptrs_jit_returnFromFunction(jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val)
{
	int8_t retType = scope->returnType.type;
	if(retType == PTRS_TYPE_UNDEFINED)
		jit_insn_default_return(func);
	else if(retType == PTRS_TYPE_INT || retType == PTRS_TYPE_FLOAT)
		jit_insn_return(func, val.val);
	else
		jit_insn_return_struct_from_values(func, val.val, val.meta);
}

void ptrs_jit_returnPtrFromFunction(jit_function_t func, ptrs_scope_t *scope, jit_value_t addr)
{
	ptrs_jit_var_t ret;
	ret.val = jit_insn_load_relative(func, addr, 0, jit_type_long);
	ret.meta = jit_insn_load_relative(func, addr, sizeof(ptrs_val_t), jit_type_ulong);
	ret.constType = -1;
	ret.addressable = false;
	ptrs_jit_returnFromFunction(func, scope, ret);
}

ptrs_jit_var_t ptrs_jit_ncallnested(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, size_t narg, ptrs_jit_var_t *args)
{
	ptrs_function_t *calleeAst = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_FUNCAST);
	if(calleeAst == NULL)
		ptrs_error(node, "Internal error: Could not get function ast for unchecked entry point of target function");

	checkFunctionParameter(node, func, scope, calleeAst, args);
	return callWithCustomAbi(func, callee, NULL, calleeAst, thisPtr, narg, args, 0);
}

ptrs_jit_var_t ptrs_jit_callnested(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	jit_value_t thisPtr, jit_function_t callee, struct ptrs_astlist *args)
{
	ptrs_function_t *calleeAst = jit_function_get_meta(callee, PTRS_JIT_FUNCTIONMETA_FUNCAST);
	if(calleeAst == NULL)
		ptrs_error(node, "Internal error: Could not get function ast for unchecked entry point of target function");

	int minArgs = getParameterCount(calleeAst);
	int narg = ptrs_astlist_length(args);
	if(minArgs > narg)
		narg = minArgs;

	ptrs_jit_var_t _args[narg];

	for(int i = 0; i < narg; i++)
	{
		if(args == NULL || args->entry == NULL)
		{
			_args[i].val = jit_const_int(func, long, 0);
			_args[i].meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
			_args[i].constType = PTRS_TYPE_UNDEFINED;
			_args[i].addressable = false;
		}
		else
		{
			_args[i] = args->entry->vtable->get(args->entry, func, scope);
		}

		if(args != NULL)
			args = args->next;
	}

	return ptrs_jit_ncallnested(node, func, scope, thisPtr, callee, narg, _args);
}

jit_function_t ptrs_jit_createFunction(ptrs_ast_t *node, jit_function_t parent,
	jit_type_t signature, const char *name)
{
	jit_function_t func;
	if(parent == NULL)
		func = jit_function_create(ptrs_jit_context, signature);
	else
		func = jit_function_create_nested(ptrs_jit_context, signature, parent);

	if(ptrs_optimizationLevel == -1)
		jit_function_set_optimization_level(func, jit_function_get_max_optimization_level());
	else
		jit_function_set_optimization_level(func, ptrs_optimizationLevel);

	if(name != NULL)
		jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_NAME, (char *)name, NULL, 0);
	if(node != NULL)
		jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_AST, node, NULL, 0);

	return func;
}

jit_function_t ptrs_jit_createFunctionFromAst(ptrs_ast_t *node, jit_function_t parent,
	ptrs_function_t *ast)
{
	size_t argc = getParameterCount(ast);
	size_t count = fillCustomAbiArgumentArray(ast, NULL, NULL, NULL, argc, NULL);
	jit_type_t paramDef[count];
	fillCustomAbiArgumentArray(ast, paramDef, NULL, NULL, argc, NULL);
	
	jit_type_t retType = getCustomAbiReturnType(ast);

	if(ast->vararg != NULL)
		ptrs_error(ast->body, "Support for variadic argument functions is not implemented");

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl,
		retType, paramDef, count, 0);

	jit_function_t func = ptrs_jit_createFunction(node, parent, signature, ast->name);
	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_FUNCAST, ast, NULL, 0);

	return func;
}

void *ptrs_jit_createCallback(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, void *closure)
{
	jit_function_t unchecked = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_UNCHECKED);
	if(unchecked != NULL)
		func = unchecked; // `func` is actually a type checking closure for `unchecked` 

	void *callbackClosure = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_CALLBACK);
	if(callbackClosure != NULL)
		return callbackClosure;

	char *funcName = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_NAME);
	if(funcName == NULL)
		funcName = "?";

	char callbackName[strlen(".callback") + strlen(funcName) + 1];
	sprintf(callbackName, "%s.callback", funcName);

	ptrs_function_t *ast = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_FUNCAST);
	ptrs_funcparameter_t *curr;
	if(ast == NULL)
		ptrs_error(node, "Cannot create callback for function %s, failed to get function AST", funcName);

	size_t argc = getParameterCount(ast);
	jit_type_t argDef[argc];

	curr = ast->args;
	for(int i = 0; i < argc; i++)
	{
		if(curr->typing.nativetype != NULL)
			argDef[i] = curr->typing.nativetype->jitType;
		else if(curr->typing.meta.type == PTRS_TYPE_FLOAT)
			argDef[i] = jit_type_float64;
		else
			argDef[i] = jit_type_long;

		curr = curr->next;
	}

	jit_type_t callbackReturnType;
	if(ast->retType.nativetype != NULL)
		callbackReturnType = ast->retType.nativetype->jitType;
	else
		callbackReturnType = jit_type_long;

	jit_type_t callbackSignature = jit_type_create_signature(jit_abi_cdecl, callbackReturnType, argDef, argc, 0);
	jit_function_t callback = ptrs_jit_createFunction(node, NULL, callbackSignature, strdup(callbackName));

	// TODO currently this is hardcoded to the root frame
	jit_value_t parentFrame = jit_insn_load_relative(callback,
		jit_const_int(callback, void_ptr, (uintptr_t)scope->rootFrame),
		0, jit_type_void_ptr
	);

	ptrs_jit_var_t args[argc];

	curr = ast->args;
	for(int i = 0; i < argc; i++)
	{
		jit_value_t param = jit_value_get_param(callback, i);
		if(curr->typing.nativetype != NULL)
		{
			param = ptrs_jit_normalizeForVar(callback, param);
			if(curr->typing.nativetype->varType == PTRS_TYPE_FLOAT)
				param = ptrs_jit_reinterpretCast(callback, param, jit_type_long);
		}

		jit_value_t meta;
		if(curr->typing.nativetype != NULL)
			meta = ptrs_jit_const_meta(callback, curr->typing.nativetype->varType);
		else if(curr->typing.meta.type != (uint8_t)-1)
			meta = jit_const_long(callback, ulong, *(uint64_t *)&curr->typing.meta);
		else
			meta = ptrs_jit_const_meta(callback, PTRS_TYPE_INT);

		args[i].val = param;
		args[i].meta = meta;
		args[i].constType = curr->typing.meta.type;
		args[i].addressable = false;

		curr = curr->next;
	}

	jit_value_t thisArg = jit_const_int(callback, void_ptr, 0);
	ptrs_jit_var_t retVar = callWithCustomAbi(callback, func, parentFrame, ast, thisArg, argc, args, 0);
	jit_value_t ret = retVar.val;

	if(ast->retType.nativetype != NULL)
	{
		ptrs_nativetype_info_t *retType = ast->retType.nativetype;
		if(retType->varType == PTRS_TYPE_FLOAT)
			ret = ptrs_jit_reinterpretCast(callback, ret, jit_type_float64);

		ret = jit_insn_convert(callback, ret, retType->jitType, 0);
	}

	jit_insn_return(callback, ret);

	if(ptrs_compileAot && jit_function_compile(callback) == 0)
		ptrs_error(node, "Failed compiling function %s", callbackName);

	callbackClosure = jit_function_to_closure(callback);
	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_CALLBACK, callbackClosure, NULL, 0);
	return callbackClosure;
}

void *ptrs_jit_function_to_closure(ptrs_ast_t *node, jit_function_t func)
{
	jit_function_t closureFunc = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_CLOSURE);
	if(closureFunc != NULL)
		return jit_function_to_closure(closureFunc);

	if(node == NULL)
		node = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_AST);
	if(node == NULL)
		ptrs_error(NULL, "Cannot create a closure for a function, failed to get AST");

	ptrs_function_t *ast = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_FUNCAST);
	if(ast == NULL)
		ptrs_error(node, "Cannot create a closure for function, failed to get function AST");

	size_t argc = getParameterCount(ast);
	int jitArgc = argc * 2 + 1;

	jit_type_t argDef[jitArgc];
	argDef[0] = jit_type_void_ptr;
	for(int i = 0; i < argc; i++)
	{
		argDef[i * 2 + 1] = jit_type_long;
		argDef[i * 2 + 2] = jit_type_ulong;
	}
	jit_type_t checkerSig = jit_type_create_signature(jit_abi_cdecl, ptrs_jit_getVarType(),
		argDef, jitArgc, 0);

	char checkerName[strlen(".checked") + strlen(ast->name) + 1];
	sprintf(checkerName, "%s.checked", ast->name);

	jit_function_t funcParent = jit_function_get_nested_parent(func);
	jit_function_t checker = ptrs_jit_createFunction(node, funcParent, checkerSig, strdup(checkerName));
	jit_function_set_meta(checker, PTRS_JIT_FUNCTIONMETA_UNCHECKED, func, NULL, 0);

	jit_value_t thisArg = jit_value_get_param(checker, 0);
	ptrs_jit_var_t args[argc];
	for(int i = 0; i < argc; i++)
	{
		args[i].val = jit_value_get_param(checker, i * 2 + 1);
		args[i].meta = jit_value_get_param(checker, i * 2 + 2);
		args[i].constType = -1;
		args[i].addressable = false;
	}

	ptrs_scope_t checkerScope;
	ptrs_initScope(&checkerScope, NULL);

	checkFunctionParameter(node, checker, &checkerScope, ast, args);
	ptrs_jit_var_t ret = callWithCustomAbi(checker, func, NULL, ast, thisArg, argc, args, JIT_CALL_TAIL);

	jit_insn_return_struct_from_values(checker, ret.val, ret.meta);

	jit_insn_default_return(checker);
	ptrs_jit_placeAssertions(checker, &checkerScope);

	if(ptrs_compileAot && jit_function_compile(checker) == 0)
		ptrs_error(node, "Failed compiling function %s", checkerName);

	jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_CLOSURE, checker, NULL, 0);
	return jit_function_to_closure(checker);
}

void ptrs_jit_buildFunction(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_function_t *ast, ptrs_struct_t *thisType)
{
	ptrs_scope_t funcScope;
	ptrs_initScope(&funcScope, scope);
	funcScope.returnType = ast->retType.meta;

	jit_insn_mark_offset(func, node->codepos);

	if(thisType == NULL)
	{
		ast->thisVal.val = jit_const_long(func, long, 0);
		ast->thisVal.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		ast->thisVal.constType = PTRS_TYPE_UNDEFINED;
		ast->thisVal.addressable = false;
	}
	else
	{
		ast->thisVal.val = jit_value_get_param(func, 0);
		ast->thisVal.meta = ptrs_jit_const_pointerMeta(func, PTRS_TYPE_STRUCT, thisType);
		ast->thisVal.constType = PTRS_TYPE_STRUCT;
		ast->thisVal.addressable = false;
	}

	bool usesCustomAbi = retrieveParameterArray(ast, func);

	if(!usesCustomAbi)
	{
		// the function uses the default ABI, we prevent having a custom .checked
		// function by checking parameters here and setting the function to be
		// its own closure
		checkFunctionParameter(node, func, &funcScope, ast, NULL);
		jit_function_set_meta(func, PTRS_JIT_FUNCTIONMETA_CLOSURE, func, NULL, 0);
	}

	for(ptrs_funcparameter_t *curr = ast->args; curr != NULL; curr = curr->next)
	{
		if(curr->arg.addressable)
		{
			ptrs_jit_var_t param = curr->arg;
			curr->arg.val = jit_value_create(func, ptrs_jit_getVarType());
			jit_value_t ptr = jit_insn_address_of(func, curr->arg.val);
			jit_insn_store_relative(func, ptr, 0, param.val);
			jit_insn_store_relative(func, ptr, sizeof(ptrs_val_t), param.meta);
		}
	}

	ast->body->vtable->get(ast->body, func, &funcScope);

	if(ast->retType.meta.type != (uint8_t)-1
		&& ast->retType.meta.type != PTRS_TYPE_UNDEFINED)
	{
		jit_value_t funcName = jit_const_int(func, void_ptr, (uintptr_t)ast->name);
		ptrs_jit_assert(node, func, &funcScope, jit_const_int(func, ubyte, 0),
			2, "Function %s defines a return type %m, but no value was returned",
			funcName, jit_const_long(func, ulong, *(uint64_t *)&ast->retType.meta));
	}

	jit_insn_default_return(func);

	ptrs_jit_placeAssertions(func, &funcScope);

	if(ptrs_compileAot && jit_function_compile(func) == 0)
		ptrs_error(node, "Failed compiling function %s", ast->name);
}
