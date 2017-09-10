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

#endif
