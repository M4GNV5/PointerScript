#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"

unsigned ptrs_handle_call(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_stringformat(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_new(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_member(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
unsigned ptrs_handle_assign_member(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	//TODO
}
unsigned ptrs_handle_addressof_member(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_var_t *ptrs_handle_call_member(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

unsigned ptrs_handle_thismember(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_thismember(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	//TODO
}
unsigned ptrs_handle_addressof_thismember(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_var_t *ptrs_handle_call_thismember(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

unsigned ptrs_handle_prefix_length(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_prefix_address(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_prefix_dereference(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_dereference(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	//TODO
}

unsigned ptrs_handle_indexlength(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_index(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_index(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	//TODO
}
unsigned ptrs_handle_addressof_index(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
ptrs_var_t *ptrs_handle_call_index(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope,
	ptrs_nativetype_info_t *retType, ptrs_ast_t *caller, struct ptrs_astlist *arguments)
{
	//TODO
}

unsigned ptrs_handle_slice(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_as(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_cast_builtin(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_tostring(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_cast(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_wildcardsymbol(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_identifier(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	unsigned val = scope->usedRegCount;
	ptrs_scope_load(jit, scope, node->arg.varval, R(val), R(val + 1));
	return val;
}
void ptrs_handle_assign_identifier(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	ptrs_scope_store(jit, scope, node->arg.varval, val, meta);
}

unsigned ptrs_handle_typed(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
void ptrs_handle_assign_typed(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope, long val, long meta)
{
	//TODO
}

unsigned ptrs_handle_constant(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	unsigned val = scope->usedRegCount;
	jit_movi(jit, R(val), *(uintptr_t *)&node->arg.constval.value);
	jit_movi(jit, R(val + 1), *(uintptr_t *)&node->arg.constval.meta);
	return val;
}

unsigned ptrs_handle_lazy(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_prefix_typeof(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_op_ternary(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_op_instanceof(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_op_in(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_yield(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
