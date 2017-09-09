#ifndef _PTRS_FUNCTION
#define _PTRS_FUNCTION

void ptrs_initScope(ptrs_scope_t *scope);

jit_type_t ptrs_jit_getVarType();

ptrs_jit_var_t ptrs_jit_valToVar(jit_function_t func, jit_value_t val);
jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var);

jit_value_t ptrs_jit_reinterpretCast(jit_function_t, jit_value_t val, jit_type_t newType);
jit_value_t ptrs_jit_normalizeForVar(jit_function_t func, jit_value_t val);

ptrs_val_t ptrs_jit_value_getValConstant(jit_value_t val);
ptrs_meta_t ptrs_jit_value_getMetaConstant(jit_value_t meta);

#endif
