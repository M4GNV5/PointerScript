#ifndef _PTRS_INTERPRETER
#define _PTRS_INTERPRETER

#include "../parser/common.h"
#include "../parser/ast.h"
#include "include/scope.h"
#include "include/struct.h"
#include "include/error.h"

#define PTRS_HANDLE_ASTERROR(ast, ...) \
	ptrs_error(ast, NULL, __VA_ARGS__)

ptrs_var_t *ptrs_handle_body(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_define(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_lazyinit(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_array(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_vararray(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_structvar(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_import(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_return(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_break(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_continue(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_yield(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_delete(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_throw(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_trycatch(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_asm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_function(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_struct(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_if(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_switch(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_while(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_dowhile(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_for(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_forin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_file(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_scopestatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_exprstatement(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_call(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_stringformat(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_new(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_indexlength(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_slice(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_as(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_cast_builtin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_tostring(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_wildcardsymbol(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_constant(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_typed(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_lazy(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_assign_identifier(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_typed(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_dereference(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_index(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_member(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_thismember(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_addressof_index(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_addressof_member(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_addressof_thismember(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_call_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);
ptrs_var_t *ptrs_handle_call_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);
ptrs_var_t *ptrs_handle_call_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);

ptrs_var_t *ptrs_handle_op_ternary(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_instanceof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_in(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_typeequal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_typeinequal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_equal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_inequal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_lessequal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_greaterequal(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_less(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_greater(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_assign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_addassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_subassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_mulassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_divassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_modassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_shrassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_shlassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_andassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_xorassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_orassign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_logicor(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_logicxor(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_logicand(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_or(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_xor(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_and(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_shr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_shl(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_add(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_sub(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_mul(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_div(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_op_mod(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_prefix_typeof(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_inc(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_dec(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_logicnot(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_length(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_not(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_address(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_dereference(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_plus(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_prefix_minus(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_suffix_inc(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_suffix_dec(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_native_getInt(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_setInt(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_getUInt(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_setUInt(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_getNative(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_getPointer(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_setPointer(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_getFloat(void *target, size_t size, ptrs_var_t *value);
ptrs_var_t *ptrs_handle_native_setFloat(void *target, size_t size, ptrs_var_t *value);

#endif
