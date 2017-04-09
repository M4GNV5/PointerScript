#ifndef _PTRS_ASTLIST
#define _PTRS_ASTLIST

void ptrs_astlist_handle(struct ptrs_astlist *list, unsigned fpOffset, jit_state_t *jit, ptrs_scope_t *scope);
void ptrs_astlist_handleByte(struct ptrs_astlist *list, unsigned fpOffset, jit_state_t *jit, ptrs_scope_t *scope);

#endif
