#include "../parser/common.h"
#include "../parser/ast.h"
#include "jit.h"

#define _hasToName_1(name) name
#define _hasToName_0(name) NULL
#define hasToName(has, name) _hasToName_##has (name)

#define VTABLE(_name, hasGet, hasSet, hasAddressof, hasCall) \
	ptrs_ast_vtable_t ptrs_ast_vtable_##_name = { \
		.name = #_name, \
		.get = hasToName(hasGet, ptrs_handle_##_name), \
		.set = hasToName(hasSet, ptrs_assign_##_name),\
		.addressof = hasToName(hasAddressof, ptrs_addressof_##_name), \
		.call = hasToName(hasCall, ptrs_call_##_name) \
	};

#define GETONLY(name) VTABLE(name, true, false, false, false)

GETONLY(initroot)
GETONLY(body)
GETONLY(define)
GETONLY(array)
GETONLY(import)
GETONLY(return)
GETONLY(break)
GETONLY(continue)
GETONLY(continue_label)
GETONLY(delete)
GETONLY(throw)
GETONLY(trycatch)
GETONLY(function)
GETONLY(struct)
GETONLY(if)
GETONLY(switch)
GETONLY(loop)
GETONLY(forin_setup)
GETONLY(forin_step)
GETONLY(scopestatement)
GETONLY(exprstatement)

GETONLY(call)
GETONLY(stringformat)
GETONLY(new)
GETONLY(indexlength)
GETONLY(slice)
GETONLY(as)
GETONLY(as_struct)
GETONLY(cast_builtin)
GETONLY(tostring)
GETONLY(constant)

VTABLE(identifier, true, true, true, false)
VTABLE(functionidentifier, true, false, false, true)
VTABLE(member, true, true, true, true)
VTABLE(index, true, true, true, true)
VTABLE(importedsymbol, true, true, true, true)

GETONLY(op_ternary)
GETONLY(op_instanceof)
GETONLY(op_in)
GETONLY(op_typeequal)
GETONLY(op_typeinequal)
GETONLY(op_equal)
GETONLY(op_inequal)
GETONLY(op_lessequal)
GETONLY(op_greaterequal)
GETONLY(op_less)
GETONLY(op_greater)
GETONLY(op_assign)
GETONLY(op_logicor)
GETONLY(op_logicxor)
GETONLY(op_logicand)
GETONLY(op_or)
GETONLY(op_xor)
GETONLY(op_and)
GETONLY(op_ushr)
GETONLY(op_sshr)
GETONLY(op_shl)
GETONLY(op_add)
GETONLY(op_sub)
GETONLY(op_mul)
GETONLY(op_div)
GETONLY(op_mod)

GETONLY(prefix_typeof)
GETONLY(prefix_inc)
GETONLY(prefix_dec)
GETONLY(prefix_logicnot)
GETONLY(prefix_sizeof)
GETONLY(prefix_not)
GETONLY(prefix_address)
VTABLE(prefix_dereference, true, true, false, false)
GETONLY(prefix_plus)
GETONLY(prefix_minus)

GETONLY(suffix_inc)
GETONLY(suffix_dec)