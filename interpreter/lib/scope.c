#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/stack.h"
#include "../include/scope.h"

const ptrs_symbol_t ptrs_argumentsSymbol = {0, sizeof(ptrs_var_t)};
const ptrs_symbol_t ptrs_thisSymbol = {0, 2 * sizeof(ptrs_var_t)};

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

	scope->current = NULL;
	scope->outer = outer;
	scope->bp = scope->sp;
	scope->sp += stackOffset;
	outer->sp = sp;

	return scope;
}
