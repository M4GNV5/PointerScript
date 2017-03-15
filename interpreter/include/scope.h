#ifndef _PTRS_SCOPE
#define _PTRS_SCOPE

#include <stdbool.h>
#include "../../parser/common.h"

#define PTRS_STACK_SIZE (64 * 1024)

extern const ptrs_symbol_t ptrs_thisSymbol;
extern size_t ptrs_stacksize;


ptrs_var_t *ptrs_scope_get(ptrs_scope_t *scope, ptrs_symbol_t symbol);
void ptrs_scope_set(ptrs_scope_t *scope, ptrs_symbol_t symbol, ptrs_var_t *value);
ptrs_scope_t *ptrs_scope_increase(ptrs_scope_t *outer, unsigned stackOffset);
void *ptrs_alloc(ptrs_scope_t *scope, size_t size);

#endif
