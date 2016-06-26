#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/stack.h"
#include "../include/scope.h"

const ptrs_symbol_t ptrs_thisSymbol = {0, 0};

void ptrs_scope_set(ptrs_scope_t *scope, ptrs_symbol_t symbol, ptrs_var_t *value)
{
	for(int i = 0; i < symbol.scope; i++)
		scope = scope->outer;
	memcpy(scope->bp + symbol.offset, value, sizeof(ptrs_var_t));
}

ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, ptrs_symbol_t symbol)
{
	for(int i = 0; i < symbol.scope; i++)
		scope = scope->outer;
	return scope->bp + symbol.offset;
}

ptrs_scope_t *ptrs_scope_increase(ptrs_scope_t *outer, unsigned stackOffset)
{
	void *sp = outer->sp;
	ptrs_scope_t *scope = ptrs_alloc(outer, sizeof(ptrs_scope_t));
	memcpy(scope, outer, sizeof(ptrs_scope_t));

	scope->outer = outer;
	scope->bp = scope->sp;
	outer->sp = sp;
	scope->sp += stackOffset;

	return scope;
}
