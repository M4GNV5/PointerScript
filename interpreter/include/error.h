#ifndef _PTRS_ERROR
#define _PTRS_ERROR

#include "../../parser/ast.h"
#include "../../parser/common.h"

void ptrs_warn(ptrs_ast_t *ast, const char *msg, ...);
void ptrs_error(ptrs_ast_t *ast, const char *msg, ...);

#endif
