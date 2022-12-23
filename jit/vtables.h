#ifndef _PTRS_VTABLES
#define _PTRS_VTABLES

#include "../parser/common.h"
#include "../parser/ast.h"

extern ptrs_ast_vtable_t ptrs_ast_vtable_initroot;
extern ptrs_ast_vtable_t ptrs_ast_vtable_body;
extern ptrs_ast_vtable_t ptrs_ast_vtable_define;
extern ptrs_ast_vtable_t ptrs_ast_vtable_array;
extern ptrs_ast_vtable_t ptrs_ast_vtable_import;
extern ptrs_ast_vtable_t ptrs_ast_vtable_return;
extern ptrs_ast_vtable_t ptrs_ast_vtable_break;
extern ptrs_ast_vtable_t ptrs_ast_vtable_continue;
extern ptrs_ast_vtable_t ptrs_ast_vtable_continue_label;
extern ptrs_ast_vtable_t ptrs_ast_vtable_delete;
extern ptrs_ast_vtable_t ptrs_ast_vtable_throw;
extern ptrs_ast_vtable_t ptrs_ast_vtable_trycatch;
extern ptrs_ast_vtable_t ptrs_ast_vtable_function;
extern ptrs_ast_vtable_t ptrs_ast_vtable_struct;
extern ptrs_ast_vtable_t ptrs_ast_vtable_if;
extern ptrs_ast_vtable_t ptrs_ast_vtable_switch;
extern ptrs_ast_vtable_t ptrs_ast_vtable_loop;
extern ptrs_ast_vtable_t ptrs_ast_vtable_forin_setup;
extern ptrs_ast_vtable_t ptrs_ast_vtable_forin_step;
extern ptrs_ast_vtable_t ptrs_ast_vtable_exprstatement;

extern ptrs_ast_vtable_t ptrs_ast_vtable_call;
extern ptrs_ast_vtable_t ptrs_ast_vtable_stringformat;
extern ptrs_ast_vtable_t ptrs_ast_vtable_new;
extern ptrs_ast_vtable_t ptrs_ast_vtable_indexlength;
extern ptrs_ast_vtable_t ptrs_ast_vtable_slice;
extern ptrs_ast_vtable_t ptrs_ast_vtable_as;
extern ptrs_ast_vtable_t ptrs_ast_vtable_as_struct;
extern ptrs_ast_vtable_t ptrs_ast_vtable_toint;
extern ptrs_ast_vtable_t ptrs_ast_vtable_tofloat;
extern ptrs_ast_vtable_t ptrs_ast_vtable_tostring;
extern ptrs_ast_vtable_t ptrs_ast_vtable_constant;
extern ptrs_ast_vtable_t ptrs_ast_vtable_typed;

extern ptrs_ast_vtable_t ptrs_ast_vtable_identifier;
extern ptrs_ast_vtable_t ptrs_ast_vtable_functionidentifier;
extern ptrs_ast_vtable_t ptrs_ast_vtable_member;
extern ptrs_ast_vtable_t ptrs_ast_vtable_index;
extern ptrs_ast_vtable_t ptrs_ast_vtable_importedsymbol;

extern ptrs_ast_vtable_t ptrs_ast_vtable_op_ternary;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_instanceof;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_in;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_typeequal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_typeinequal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_equal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_inequal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_lessequal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_greaterequal;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_less;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_greater;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_assign;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_logicor;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_logicxor;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_logicand;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_or;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_xor;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_and;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_ushr;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_sshr;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_shl;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_add;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_sub;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_mul;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_div;
extern ptrs_ast_vtable_t ptrs_ast_vtable_op_mod;

extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_typeof;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_inc;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_dec;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_logicnot;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_sizeof;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_not;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_address;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_dereference;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_plus;
extern ptrs_ast_vtable_t ptrs_ast_vtable_prefix_minus;

extern ptrs_ast_vtable_t ptrs_ast_vtable_suffix_inc;
extern ptrs_ast_vtable_t ptrs_ast_vtable_suffix_dec;

#endif
