#ifndef _PTRS_INTERPRETER
#define _PTRS_INTERPRETER

#include "../parser/common.h"
#include "../parser/ast.h"
#include "include/scope.h"
#include "include/error.h"

#define PTRS_HANDLE_ASTERROR(ast, ...) \
	ptrs_error(ast, NULL, __VA_ARGS__)

ptrs_var_t *ptrs_handle_body(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_define(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
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
ptrs_var_t *ptrs_handle_arrayexpr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_vararrayexpr(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_yield_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_stringformat(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_new(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_slice(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_as(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_cast_builtin(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_cast(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_identifier(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_constant(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_assign_identifier(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_dereference(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_index(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_member(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_assign_thismember(ptrs_ast_t *node, ptrs_var_t *value, ptrs_scope_t *scope);

ptrs_var_t *ptrs_handle_call_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_vartype_t retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);
ptrs_var_t *ptrs_handle_call_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_vartype_t retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);
ptrs_var_t *ptrs_handle_call_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_vartype_t retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments);

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



#define PTRS_HANDLE_BODY ptrs_handle_body
#define PTRS_HANDLE_DEFINE ptrs_handle_define
#define PTRS_HANDLE_ARRAY ptrs_handle_array
#define PTRS_HANDLE_VARARRAY ptrs_handle_vararray
#define PTRS_HANDLE_STRUCTVAR ptrs_handle_structvar
#define PTRS_HANDLE_IMPORT ptrs_handle_import
#define PTRS_HANDLE_RETURN ptrs_handle_return
#define PTRS_HANDLE_BREAK ptrs_handle_break
#define PTRS_HANDLE_CONTINUE ptrs_handle_continue
#define PTRS_HANDLE_YIELD ptrs_handle_yield
#define PTRS_HANDLE_DELETE ptrs_handle_delete
#define PTRS_HANDLE_THROW ptrs_handle_throw
#define PTRS_HANDLE_TRYCATCH ptrs_handle_trycatch
#define PTRS_HANDLE_ASM ptrs_handle_asm
#define PTRS_HANDLE_FUNCTION ptrs_handle_function
#define PTRS_HANDLE_STRUCT ptrs_handle_struct
#define PTRS_HANDLE_IF ptrs_handle_if
#define PTRS_HANDLE_SWITCH ptrs_handle_switch
#define PTRS_HANDLE_WHILE ptrs_handle_while
#define PTRS_HANDLE_DOWHILE ptrs_handle_dowhile
#define PTRS_HANDLE_FOR ptrs_handle_for
#define PTRS_HANDLE_FORIN ptrs_handle_forin
#define PTRS_HANDLE_FILE ptrs_handle_file
#define PTRS_HANDLE_SCOPESTATEMENT ptrs_handle_scopestatement
#define PTRS_HANDLE_EXPRSTATEMENT ptrs_handle_exprstatement

#define PTRS_HANDLE_CALL ptrs_handle_call
#define PTRS_HANDLE_ARRAYEXPR ptrs_handle_arrayexpr
#define PTRS_HANDLE_VARARRAYEXPR ptrs_handle_vararrayexpr
#define PTRS_HANDLE_ALGORITHM ptrs_handle_algorithm
#define PTRS_HANDLE_YIELD_ALGORITHM ptrs_handle_yield_algorithm
#define PTRS_HANDLE_STRINGFORMAT ptrs_handle_stringformat
#define PTRS_HANDLE_NEW ptrs_handle_new
#define PTRS_HANDLE_MEMBER ptrs_handle_member
#define PTRS_HANDLE_THISMEMBER ptrs_handle_thismember
#define PTRS_HANDLE_INDEX ptrs_handle_index
#define PTRS_HANDLE_SLICE ptrs_handle_slice
#define PTRS_HANDLE_AS ptrs_handle_as
#define PTRS_HANDLE_CAST_BUILTIN ptrs_handle_cast_builtin
#define PTRS_HANDLE_CAST ptrs_handle_cast
#define PTRS_HANDLE_IDENTIFIER ptrs_handle_identifier
#define PTRS_HANDLE_CONSTANT ptrs_handle_constant

#define PTRS_HANDLE_ASSIGN_IDENTIFIER ptrs_handle_assign_identifier
#define PTRS_HANDLE_ASSIGN_DEREFERENCE ptrs_handle_assign_dereference
#define PTRS_HANDLE_ASSIGN_INDEX ptrs_handle_assign_index
#define PTRS_HANDLE_ASSIGN_MEMBER ptrs_handle_assign_member
#define PTRS_HANDLE_ASSIGN_THISMEMBER ptrs_handle_assign_thismember

#define PTRS_HANDLE_CALL_INDEX ptrs_handle_call_index
#define PTRS_HANDLE_CALL_MEMBER ptrs_handle_call_member
#define PTRS_HANDLE_CALL_THISMEMBER ptrs_handle_call_thismember

#define PTRS_HANDLE_OP_TERNARY ptrs_handle_op_ternary
#define PTRS_HANDLE_OP_INSTANCEOF ptrs_handle_op_instanceof
#define PTRS_HANDLE_OP_IN ptrs_handle_op_in
#define PTRS_HANDLE_OP_TYPEEQUAL ptrs_handle_op_typeequal
#define PTRS_HANDLE_OP_TYPEINEQUAL ptrs_handle_op_typeinequal
#define PTRS_HANDLE_OP_EQUAL ptrs_handle_op_equal
#define PTRS_HANDLE_OP_INEQUAL ptrs_handle_op_inequal
#define PTRS_HANDLE_OP_LESSEQUAL ptrs_handle_op_lessequal
#define PTRS_HANDLE_OP_GREATEREQUAL ptrs_handle_op_greaterequal
#define PTRS_HANDLE_OP_LESS ptrs_handle_op_less
#define PTRS_HANDLE_OP_GREATER ptrs_handle_op_greater
#define PTRS_HANDLE_OP_ASSIGN ptrs_handle_op_assign
#define PTRS_HANDLE_OP_ADDASSIGN ptrs_handle_op_addassign
#define PTRS_HANDLE_OP_SUBASSIGN ptrs_handle_op_subassign
#define PTRS_HANDLE_OP_MULASSIGN ptrs_handle_op_mulassign
#define PTRS_HANDLE_OP_DIVASSIGN ptrs_handle_op_divassign
#define PTRS_HANDLE_OP_MODASSIGN ptrs_handle_op_modassign
#define PTRS_HANDLE_OP_SHRASSIGN ptrs_handle_op_shrassign
#define PTRS_HANDLE_OP_SHLASSIGN ptrs_handle_op_shlassign
#define PTRS_HANDLE_OP_ANDASSIGN ptrs_handle_op_andassign
#define PTRS_HANDLE_OP_XORASSIGN ptrs_handle_op_xorassign
#define PTRS_HANDLE_OP_ORASSIGN ptrs_handle_op_orassign
#define PTRS_HANDLE_OP_LOGICOR ptrs_handle_op_logicor
#define PTRS_HANDLE_OP_LOGICXOR ptrs_handle_op_logicxor
#define PTRS_HANDLE_OP_LOGICAND ptrs_handle_op_logicand
#define PTRS_HANDLE_OP_OR ptrs_handle_op_or
#define PTRS_HANDLE_OP_XOR ptrs_handle_op_xor
#define PTRS_HANDLE_OP_AND ptrs_handle_op_and
#define PTRS_HANDLE_OP_SHR ptrs_handle_op_shr
#define PTRS_HANDLE_OP_SHL ptrs_handle_op_shl
#define PTRS_HANDLE_OP_ADD ptrs_handle_op_add
#define PTRS_HANDLE_OP_SUB ptrs_handle_op_sub
#define PTRS_HANDLE_OP_MUL ptrs_handle_op_mul
#define PTRS_HANDLE_OP_DIV ptrs_handle_op_div
#define PTRS_HANDLE_OP_MOD ptrs_handle_op_mod

#define PTRS_HANDLE_OP_TYPEOF ptrs_handle_prefix_typeof
#define PTRS_HANDLE_PREFIX_INC ptrs_handle_prefix_inc
#define PTRS_HANDLE_PREFIX_DEC ptrs_handle_prefix_dec
#define PTRS_HANDLE_PREFIX_LOGICNOT ptrs_handle_prefix_logicnot
#define PTRS_HANDLE_PREFIX_LENGTH ptrs_handle_prefix_length
#define PTRS_HANDLE_PREFIX_NOT ptrs_handle_prefix_not
#define PTRS_HANDLE_PREFIX_ADDRESS ptrs_handle_prefix_address
#define PTRS_HANDLE_PREFIX_DEREFERENCE ptrs_handle_prefix_dereference
#define PTRS_HANDLE_PREFIX_PLUS ptrs_handle_prefix_plus
#define PTRS_HANDLE_PREFIX_MINUS ptrs_handle_prefix_minus

#define PTRS_HANDLE_SUFFIX_INC ptrs_handle_suffix_inc
#define PTRS_HANDLE_SUFFIX_DEC ptrs_handle_suffix_dec

#define PTRS_HANDLE_NATIVE_GETINT ptrs_handle_native_getInt
#define PTRS_HANDLE_NATIVE_SETINT ptrs_handle_native_setInt
#define PTRS_HANDLE_NATIVE_GETUINT ptrs_handle_native_getUInt
#define PTRS_HANDLE_NATIVE_SETUINT ptrs_handle_native_setUInt
#define PTRS_HANDLE_NATIVE_GETNATIVE ptrs_handle_native_getNative
#define PTRS_HANDLE_NATIVE_SETNATIVE ptrs_handle_native_setPointer
#define PTRS_HANDLE_NATIVE_GETPOINTER ptrs_handle_native_getPointer
#define PTRS_HANDLE_NATIVE_SETPOINTER ptrs_handle_native_setPointer
#define PTRS_HANDLE_NATIVE_GETFLOAT ptrs_handle_native_getFloat
#define PTRS_HANDLE_NATIVE_SETFLOAT ptrs_handle_native_setFloat


#endif
