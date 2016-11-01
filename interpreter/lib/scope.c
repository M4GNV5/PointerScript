#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../../parser/common.h"
#include "../include/scope.h"
#include "../include/error.h"

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

size_t ptrs_stacksize = PTRS_STACK_SIZE;
bool ptrs_zeroMemory = false;

void *ptrs_alloc(ptrs_scope_t *scope, size_t size)
{
	if(scope->sp == NULL)
	{
		scope->sp = malloc(ptrs_stacksize);
		scope->bp = scope->sp;
		scope->stackstart = scope->sp;
	}

	if(scope->sp + size >= scope->stackstart + ptrs_stacksize)
		ptrs_error(NULL, scope, "Out of memory");

	void *ptr = scope->sp;
	scope->sp += ((size - 1) & ~15) + 16;

	if(ptrs_zeroMemory)
		memset(ptr, 0, size);

	return ptr;
}
