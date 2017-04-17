#include "../include/scope.h"

void ptrs_scope_storePatches(ptrs_patchstore_t *store, ptrs_scope_t *scope)
{
	store->breakLabel = scope->breakLabel;
	store->continueLabel = scope->continueLabel;
	store->breakPatches = scope->breakPatches;
	store->continuePatches = scope->continuePatches;

	scope->breakLabel = 0;
	scope->continueLabel = 0;
	scope->breakPatches = NULL;
	scope->continuePatches = NULL;
}

void ptrs_scope_restorePatches(ptrs_patchstore_t *store, ptrs_scope_t *scope)
{
	scope->breakLabel = store->breakLabel;
	scope->continueLabel = store->continueLabel;
	scope->breakPatches = store->breakPatches;
	scope->continuePatches = store->continuePatches;
}

void ptrs_scope_patch(jit_state_t *jit, ptrs_patchlist_t *curr)
{
	while(curr != NULL)
	{
		jit_patch(jit, curr->patch);

		ptrs_patchlist_t *prev = curr;
		curr = curr->next;
		free(prev);
	}
}
