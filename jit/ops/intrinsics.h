#ifndef _PTRS_INTRINSICS
#define _PTRS_INTRINSICS

#include "../../parser/common.h"
#include "../../parser/ast.h"

ptrs_var_t ptrs_intrinsic_typeequal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_typeinequal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_equal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_inequal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_lessequal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_greaterequal(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_less(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_greater(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_or(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_xor(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_and(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_ushr(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_sshr(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_shl(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_add(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_sub(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_mul(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_div(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);
ptrs_var_t ptrs_intrinsic_mod(ptrs_ast_t *node,
    ptrs_val_t left, ptrs_meta_t leftMeta, ptrs_val_t right, ptrs_meta_t rightMeta);

#endif