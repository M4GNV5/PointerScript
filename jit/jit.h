#ifndef _PTRS_JIT
#define _PTRS_JIT

#include "../parser/common.h"
#include "../parser/ast.h"
#include "vtables.h"
#include "include/struct.h"
#include "include/error.h"

#define PTRS_HANDLE_ASTERROR(ast, ...) \
	ptrs_error(ast, __VA_ARGS__)

ptrs_jit_var_t ptrs_handle_initroot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_body(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_define(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_array(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_import(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_return(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_break(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_continue(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_continue_label(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_delete(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_throw(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_trycatch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_function(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_struct(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_if(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_switch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_loop(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_forin_setup(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_forin_step(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_scopestatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_exprstatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

ptrs_jit_var_t ptrs_handle_call(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_stringformat(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_new(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_indexlength(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_slice(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_as(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_as_struct(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_toint(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_tofloat(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_tostring(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_constant(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_typed(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

void ptrs_assign_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val);
void ptrs_assign_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val);
void ptrs_assign_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val);
void ptrs_assign_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope, ptrs_jit_var_t val);
void ptrs_assign_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_jit_var_t val);

ptrs_jit_var_t ptrs_addressof_identifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_addressof_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_addressof_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_addressof_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

ptrs_jit_var_t ptrs_call_functionidentifier(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments);
ptrs_jit_var_t ptrs_call_index(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments);
ptrs_jit_var_t ptrs_call_member(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments);
ptrs_jit_var_t ptrs_call_importedsymbol(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t *caller, ptrs_typing_t *typing, struct ptrs_astlist *arguments);

ptrs_jit_var_t ptrs_handle_op_ternary(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_instanceof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_in(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_typeequal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_typeinequal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_equal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_inequal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_lessequal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_greaterequal(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_less(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_greater(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_assign(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_logicor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_logicxor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_logicand(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_or(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_xor(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_and(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_ushr(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_sshr(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_shl(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_add(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_sub(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_mul(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_div(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_op_mod(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

ptrs_jit_var_t ptrs_handle_prefix_typeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_inc(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_dec(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_logicnot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_sizeof(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_not(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_address(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_plus(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_prefix_minus(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

ptrs_jit_var_t ptrs_handle_suffix_inc(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);
ptrs_jit_var_t ptrs_handle_suffix_dec(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope);

void ptrs_handle_native_getInt(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_setInt(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_getUInt(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_setUInt(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_getPointer(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_setPointer(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_getFloat(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_setFloat(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_getVar(void *target, size_t size, ptrs_var_t *value);
void ptrs_handle_native_setVar(void *target, size_t size, ptrs_var_t *value);

#endif
