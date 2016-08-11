#ifndef _PTRS_DEBUG
#define _PTRS_DEBUG

#include "../../parser/common.h"

extern bool ptrs_debugEnabled;

void ptrs_debug_mainLoop(ptrs_ast_t *ast, ptrs_scope_t *scope, bool hasStarted);
void ptrs_debug_break(ptrs_ast_t *ast, ptrs_scope_t *scope, char *reason, ...);
void ptrs_debug_update(ptrs_ast_t *ast, ptrs_scope_t *scope);

#endif
