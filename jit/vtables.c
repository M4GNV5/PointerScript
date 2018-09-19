#include "../parser/common.h"
#include "../parser/ast.h"
#include "jit.h"

#define VTABLE(name, _get, _set, _addressof, _call) \
	ptrs_ast_vtable_t ptrs_ast_vtable_##name = { \
		.get = _get, \
		.set = _set,\
		.addressof = _addressof, \
		.call = _call, \
	};

#define GETONLY(name) \
	VTABLE(name, ptrs_handle_##name, NULL, NULL, NULL)

#define ALL(name) \
	VTABLE(name, ptrs_handle_##name, ptrs_handle_assign_##name, \
		ptrs_handle_addressof_##name, ptrs_handle_call_##name)

GETONLY(body)
GETONLY(define)
GETONLY(typeddefine)
GETONLY(array)
GETONLY(vararray)
GETONLY(import)
GETONLY(return)
GETONLY(break)
GETONLY(continue)
GETONLY(yield)
GETONLY(delete)
GETONLY(throw)
GETONLY(trycatch)
GETONLY(function)
GETONLY(struct)
GETONLY(if)
GETONLY(switch)
GETONLY(while)
GETONLY(dowhile)
GETONLY(for)
GETONLY(forin)
GETONLY(scopestatement)
GETONLY(exprstatement)

GETONLY(call)
GETONLY(stringformat)
GETONLY(new)
GETONLY(indexlength)
GETONLY(slice)
GETONLY(as)
GETONLY(cast_builtin)
GETONLY(tostring)
GETONLY(cast)
GETONLY(constant)

VTABLE(identifier, ptrs_handle_identifier, ptrs_handle_assign_identifier,
	ptrs_handle_addressof_identifier, NULL)
VTABLE(functionidentifier, ptrs_handle_functionidentifier, NULL, NULL,
	ptrs_handle_call_functionidentifier)
ALL(member)
ALL(index)
ALL(importedsymbol)

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
GETONLY(op_shr)
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
VTABLE(prefix_dereference, ptrs_handle_prefix_dereference,
	ptrs_handle_assign_dereference, NULL, NULL)
GETONLY(prefix_plus)
GETONLY(prefix_minus)

GETONLY(suffix_inc)
GETONLY(suffix_dec)