#ifndef _PTRS_FUNCTION
#define _PTRS_FUNCTION

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"

#define ptrs_util_pasteTuple(...) __VA_ARGS__

char *ptrs_readFile(const char *path);

void ptrs_initScope(ptrs_scope_t *scope, ptrs_scope_t *parent);

jit_type_t ptrs_jit_getVarType();

ptrs_jit_var_t ptrs_jit_valToVar(jit_function_t func, jit_value_t val);
jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var);

jit_value_t ptrs_jit_reinterpretCast(jit_function_t func, jit_value_t val, jit_type_t newType);
jit_value_t ptrs_jit_normalizeForVar(jit_function_t func, jit_value_t val);
void ptrs_jit_assignTypedFromVar(jit_function_t func,
	jit_value_t target, jit_type_t type, ptrs_jit_var_t val);

jit_type_t ptrs_jit_jitTypeFromTyping(ptrs_typing_t *typing);

ptrs_val_t ptrs_jit_value_getValConstant(jit_value_t val);
ptrs_meta_t ptrs_jit_value_getMetaConstant(jit_value_t meta);
ptrs_jit_var_t ptrs_jit_varFromConstant(jit_function_t func, ptrs_var_t val);

jit_value_t ptrs_jit_allocate(jit_function_t func, jit_value_t size, bool onStack, bool allowReuse);

jit_value_t ptrs_jit_import(ptrs_ast_t *node, jit_function_t func, jit_value_t val, bool asPtr);

#define ptrs_jit_reusableSignature(func, name, retType, types) \
	static jit_type_t name = NULL; \
	if(name == NULL) \
	{ \
		jit_type_t argDef[] = { \
			ptrs_util_pasteTuple types \
		}; \
		\
		name = jit_type_create_signature(jit_abi_cdecl, retType, argDef, \
			sizeof(argDef) / sizeof(jit_type_t), 0); \
	}

#define ptrs_jit_reusableCall(func, callee, retVal, retType, types, args) \
	do \
	{ \
		ptrs_jit_reusableSignature(func, signature, retType, types); \
		\
		jit_value_t params[] = { \
			ptrs_util_pasteTuple args \
		}; \
		retVal = jit_insn_call_native(func, #callee, callee, signature, params, \
			sizeof(params) / sizeof(jit_value_t), JIT_CALL_NOTHROW); \
	} while(0)

#define ptrs_jit_reusableCallVoid(func, callee, types, args) \
	ptrs_jit_reusableCall(func, callee, jit_value_t dummy, jit_type_void, types, args)

//TODO libjit needs jit_function_apply_nested and jit_apply_nested
#define ptrs_jit_applyNested(func, ret, parentFrame, thisArg, argPtrs) \
	do \
	{ \
		void *closure = jit_function_to_closure(func); \
		void *args[] = {&parentFrame, &thisArg, ptrs_util_pasteTuple argPtrs}; \
		\
		jit_type_t argDef[sizeof(args) / sizeof(void *)]; \
		for(int i = 0; i < sizeof(argDef) / sizeof(jit_type_t); i++) \
			argDef[i] = jit_type_ulong; \
		\
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, \
			jit_type_get_return(jit_function_get_signature(func)), argDef, \
			sizeof(argDef) / sizeof(jit_type_t), 0); \
		\
		jit_apply(signature, closure, args, sizeof(args) / sizeof(void *), ret); \
		jit_type_free(signature); \
	} while(0)


#define ptrs_jit_typeCheck(node, func, scope, val, type, msg) \
	do \
	{ \
		if(val.constType == -1) \
		{ \
			if(ptrs_enableSafety) \
			{ \
				jit_value_t actualType = ptrs_jit_getType(func, val.meta); \
				ptrs_jit_assert(node, func, scope, \
					jit_insn_eq(func, actualType, jit_const_int(func, sbyte, type)), \
					1, msg, actualType \
				); \
			} \
		} \
		else if(val.constType != type) \
		{ \
			ptrs_error(node, msg, val.constType); \
		} \
	} while(0)

void ptrs_jit_typeSwitch_setup(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t *val, jit_value_t valType,
	int *count, uint8_t *types, jit_label_t *labels,
	size_t argCount, const char *msg, ...);
#define ptrs_jit_typeSwitch(node, func, scope, val, errorTuple, typeTuple, cases) \
	{ \
		uint8_t types[] = {ptrs_util_pasteTuple typeTuple}; \
		jit_label_t labels[sizeof(types) / sizeof(uint8_t)]; \
		int count = sizeof(types) / sizeof(uint8_t); \
		jit_label_t end = jit_label_undefined; \
		jit_value_t TYPESWITCH_TYPE; \
		\
		if(val.constType == -1) \
			TYPESWITCH_TYPE = ptrs_jit_getType(func, val.meta); \
		\
		ptrs_jit_typeSwitch_setup(node, func, scope, \
			&val, TYPESWITCH_TYPE, &count, types, labels, \
			ptrs_util_pasteTuple errorTuple \
		); \
		\
		for(int i = 0; i < count; i++) \
		{ \
			if(val.constType == -1) \
				jit_insn_label(func, labels + i); \
			\
			switch(types[i]) \
			{ \
				cases \
			} \
			\
			if(val.constType == -1 && i < count - 1) \
				jit_insn_branch(func, &end); \
		} \
		\
		jit_insn_label(func, &end); \
	}

#endif
