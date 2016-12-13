#ifndef _PTRS_ASTLIST
#define _PTRS_ASTLIST

int ptrs_astlist_length(struct ptrs_astlist *list, ptrs_ast_t *node, ptrs_scope_t *scope);
void ptrs_astlist_handle(struct ptrs_astlist *list, int len, ptrs_var_t *out, ptrs_scope_t *scope);
void ptrs_astlist_handleByte(struct ptrs_astlist *list, int len, uint8_t *out, ptrs_scope_t *scope);

#endif
