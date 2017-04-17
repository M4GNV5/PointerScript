#ifndef _PTRS_SCOPE
#define _PTRS_SCOPE

#include <jitlib.h>
#include "../../parser/common.h"
#include "../../parser/ast.h"

typedef struct
{
	uintptr_t continueLabel;
	uintptr_t breakLabel;
	ptrs_patchlist_t *continuePatches;
	ptrs_patchlist_t *breakPatches;
} ptrs_patchstore_t;

void ptrs_scope_storePatches(ptrs_patchstore_t *store, ptrs_scope_t *scope);
void ptrs_scope_restorePatches(ptrs_patchstore_t *store, ptrs_scope_t *scope);

void ptrs_scope_patch(jit_state_t *jit, ptrs_patchlist_t *curr);

#endif
