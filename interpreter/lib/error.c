#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"

void print_pos(ptrs_ast_t *ast)
{
	if(ast != NULL)
	{
		int line = 1;
		int column = 1;
		for(int i = 0; i < ast->codepos; i++)
		{
			if(ast->code[i] == '\n')
			{
				line++;
				column = 1;
			}
			else
			{
				column++;
			}
		}

		printf(" at line %d column %d\n", line, column);
	}
	else
	{
		printf("\n");
	}
}

void ptrs_warn(ptrs_ast_t *ast, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	print_pos(ast);
}

void ptrs_error(ptrs_ast_t *ast, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	print_pos(ast);
	exit(3);
}
