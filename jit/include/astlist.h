#ifndef _PTRS_ASTLIST
#define _PTRS_ASTLIST

int ptrs_astlist_length(struct ptrs_astlist *curr);
void ptrs_astlist_handle(struct ptrs_astlist *list, jit_function_t func, ptrs_scope_t *scope, jit_value_t val, jit_value_t size, ptrs_nativetype_info_t *type);

#endif
