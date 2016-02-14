#ifndef _PTRS_ASTDUMP
#define _PTRS_ASTDUMP

ptrs_var_t *ptrs_dump_body(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_define(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_if(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_exprstatement(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_call(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_index(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_identifier(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_string(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_int(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_float(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_cast(ptrs_ast_t *node);

ptrs_var_t *ptrs_dump_op_equal(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_inequal(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_lessequal(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_greaterequal(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_less(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_greater(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_assign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_addassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_subassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_mulassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_divassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_modassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_shrassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_shlassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_andassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_xorassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_orassign(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_logicor(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_logicand(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_or(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_xor(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_and(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_shr(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_shl(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_add(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_sub(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_mul(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_div(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_op_mod(ptrs_ast_t *node);

ptrs_var_t *ptrs_dump_prefix_sizeof(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_typeof(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_inc(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_dec(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_logicnot(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_not(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_address(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_dereference(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_plus(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_prefix_minus(ptrs_ast_t *node);

ptrs_var_t *ptrs_dump_suffix_inc(ptrs_ast_t *node);
ptrs_var_t *ptrs_dump_suffix_dec(ptrs_ast_t *node);

#define PTRS_HANDLE_BODY ptrs_dump_body
#define PTRS_HANDLE_DEFINE ptrs_dump_define
#define PTRS_HANDLE_IF ptrs_dump_if
#define PTRS_HANDLE_EXPRSTATEMENT ptrs_dump_exprstatement
#define PTRS_HANDLE_CALL ptrs_dump_call
#define PTRS_HANDLE_INDEX ptrs_dump_index
#define PTRS_HANDLE_CAST ptrs_dump_cast
#define PTRS_HANDLE_IDENTIFIER ptrs_dump_identifier
#define PTRS_HANDLE_STRING ptrs_dump_string
#define PTRS_HANDLE_INTEGER ptrs_dump_int
#define PTRS_HANDLE_FLOAT ptrs_dump_float

#define PTRS_HANDLE_OP_EQUAL ptrs_dump_op_equal
#define PTRS_HANDLE_OP_INEQUAL ptrs_dump_op_inequal
#define PTRS_HANDLE_OP_LESSEQUAL ptrs_dump_op_lessequal
#define PTRS_HANDLE_OP_GREATEREQUAL ptrs_dump_op_greaterequal
#define PTRS_HANDLE_OP_LESS ptrs_dump_op_less
#define PTRS_HANDLE_OP_GREATER ptrs_dump_op_greater
#define PTRS_HANDLE_OP_ASSIGN ptrs_dump_op_assign
#define PTRS_HANDLE_OP_ADDASSIGN ptrs_dump_op_addassign
#define PTRS_HANDLE_OP_SUBASSIGN ptrs_dump_op_subassign
#define PTRS_HANDLE_OP_MULASSIGN ptrs_dump_op_mulassign
#define PTRS_HANDLE_OP_DIVASSIGN ptrs_dump_op_divassign
#define PTRS_HANDLE_OP_MODASSIGN ptrs_dump_op_modassign
#define PTRS_HANDLE_OP_SHRASSIGN ptrs_dump_op_shrassign
#define PTRS_HANDLE_OP_SHLASSIGN ptrs_dump_op_shlassign
#define PTRS_HANDLE_OP_ANDASSIGN ptrs_dump_op_andassign
#define PTRS_HANDLE_OP_XORASSIGN ptrs_dump_op_xorassign
#define PTRS_HANDLE_OP_ORASSIGN ptrs_dump_op_orassign
#define PTRS_HANDLE_OP_LOGICOR ptrs_dump_op_logicor
#define PTRS_HANDLE_OP_LOGICAND ptrs_dump_op_logicand
#define PTRS_HANDLE_OP_OR ptrs_dump_op_or
#define PTRS_HANDLE_OP_XOR ptrs_dump_op_xor
#define PTRS_HANDLE_OP_AND ptrs_dump_op_and
#define PTRS_HANDLE_OP_SHR ptrs_dump_op_shr
#define PTRS_HANDLE_OP_SHL ptrs_dump_op_shl
#define PTRS_HANDLE_OP_ADD ptrs_dump_op_add
#define PTRS_HANDLE_OP_SUB ptrs_dump_op_sub
#define PTRS_HANDLE_OP_MUL ptrs_dump_op_mul
#define PTRS_HANDLE_OP_DIV ptrs_dump_op_div
#define PTRS_HANDLE_OP_MOD ptrs_dump_op_mod

#define PTRS_HANDLE_OP_SIZEOF ptrs_dump_prefix_sizeof
#define PTRS_HANDLE_OP_TYPEOF ptrs_dump_prefix_typeof
#define PTRS_HANDLE_PREFIX_INC ptrs_dump_prefix_inc
#define PTRS_HANDLE_PREFIX_DEC ptrs_dump_prefix_dec
#define PTRS_HANDLE_PREFIX_LOGICNOT ptrs_dump_prefix_logicnot
#define PTRS_HANDLE_PREFIX_NOT ptrs_dump_prefix_not
#define PTRS_HANDLE_PREFIX_ADDRESS ptrs_dump_prefix_address
#define PTRS_HANDLE_PREFIX_DEREFERENCE ptrs_dump_prefix_dereference
#define PTRS_HANDLE_PREFIX_PLUS ptrs_dump_prefix_plus
#define PTRS_HANDLE_PREFIX_MINUS ptrs_dump_prefix_minus

#define PTRS_HANDLE_SUFFIX_INC ptrs_dump_suffix_inc
#define PTRS_HANDLE_SUFFIX_DEC ptrs_dump_suffix_dec


#endif
