#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../parser/common.h"
#include "../../parser/ast.h"

void print_pos(ptrs_ast_t *ast)
{
	if(ast != NULL)
	{
		int line = 1;
		int column = 1;
		char *currLine = ast->code;
		for(int i = 0; i < ast->codepos; i++)
		{
			if(ast->code[i] == '\n')
			{
				line++;
				column = 1;
				currLine = &(ast->code[i + 1]);
			}
			else
			{
				column++;
			}
		}

		int linelen = strchr(currLine, '\n') - currLine;
		fprintf(stderr, " at line %d column %d\n%.*s\n", line, column, linelen, currLine);
		
		int linePos = (ast->code + ast->codepos) - currLine;
		for(int i = 0; i < linePos; i++)
		{
			fprintf(stderr, currLine[i] == '\t' ? "\t" : " ");
		}
		fprintf(stderr, "^\n");
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
