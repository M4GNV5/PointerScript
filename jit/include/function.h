#ifndef _PTRS_FUNCTION
#define _PTRS_FUNCTION

jit_type_t ptrs_jit_getVarType();

ptrs_jit_var_t ptrs_jit_valToVar(jit_function_t func, jit_value_t val);
jit_value_t ptrs_jit_varToVal(jit_function_t func, ptrs_jit_var_t var);

jit_function_t ptrs_jit_createTrampoline(ptrs_function_t *funcAst, jit_function_t func, jit_type_t *funcArgDef);

#endif
