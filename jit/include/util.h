#ifndef _PTRS_FUNCTION
#define _PTRS_FUNCTION

#include "../../parser/common.h"
#include "../../parser/ast.h"
#include "../include/error.h"

void ptrs_initScope(ptrs_scope_t *scope);

jit_type_t ptrs_jit_getVarType();

ptrs_jit_var_t ptrs_jit_valToVar(jit_function_t func, jit_value_t val);
jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var);

jit_value_t ptrs_jit_reinterpretCast(jit_function_t, jit_value_t val, jit_type_t newType);
jit_value_t ptrs_jit_normalizeForVar(jit_function_t func, jit_value_t val);

ptrs_val_t ptrs_jit_value_getValConstant(jit_value_t val);
ptrs_meta_t ptrs_jit_value_getMetaConstant(jit_value_t meta);

#define ptrs_jit_typeCheck(node, func, scope, val, type, argCount, msg, ...) \
	do \
	{ \
		if(val.constType == -1) \
		{ \
			jit_value_t TYPECHECK_TYPE = ptrs_jit_getType(func, val.meta); \
			ptrs_jit_assert(node, func, scope, \
				jit_insn_eq(func, TYPECHECK_TYPE, jit_const_int(func, sbyte, type)), \
				argCount, msg, __VA_ARGS__ \
			); \
		} \
		else if(val.constType != type) \
		{ \
			ptrs_vartype_t TYPECHECK_TYPE = val.constType; \
			ptrs_error(node, msg, __VA_ARGS__); \
		} \
	} while(0)

#define ptrs_jit_typeRangeCheck(node, func, scope, val, start, end, argCount, msg, ...) \
	do \
	{ \
		if(val.constType == -1) \
		{ \
			jit_value_t TYPECHECK_TYPE = ptrs_jit_getType(func, val.meta); \
			struct ptrs_assertion *assertion = ptrs_jit_assert(node, func, scope, \
				jit_insn_ge(func, TYPECHECK_TYPE, jit_const_int(func, sbyte, start)), \
				argCount, msg, __VA_ARGS__ \
			); \
			ptrs_jit_appendAssert(func, assertion, \
				jit_insn_le(func, TYPECHECK_TYPE, jit_const_int(func, sbyte, end))); \
		} \
		else if(val.constType < start || val.constType > end) \
		{ \
			ptrs_vartype_t TYPECHECK_TYPE = val.constType; \
			ptrs_error(node, msg, __VA_ARGS__); \
		} \
	} while(0)

void ptrs_jit_typeSwitch_setup(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t *val, jit_value_t valType,
	int *count, uint8_t *types, jit_label_t *labels,
	size_t argCount, const char *msg, ...);
#define _ptrs_jit_typeSwitch_paste(...) __VA_ARGS__
#define ptrs_jit_typeSwitch(node, func, scope, val, errorTuple, typeTuple, cases) \
	{ \
		uint8_t types[] = {_ptrs_jit_typeSwitch_paste typeTuple}; \
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
			_ptrs_jit_typeSwitch_paste errorTuple \
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
	} \

#endif
