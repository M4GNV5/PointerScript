#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "../include/error.h"

typedef struct ptrs_breakpoint
{
	char *file;
	int line;
	int startPos;
	int endPos;
	struct ptrs_breakpoint *next;
} ptrs_breakpoint_t;
static ptrs_breakpoint_t *breakPoints = NULL;

bool ptrs_debugEnabled = false;
static const char *lastFile = NULL;
static bool breakAfterStep = false;

static void calculateFilePos(ptrs_ast_t *ast, ptrs_breakpoint_t *entry)
{
	int line = 1;
	for(int i = 0; ast->code[i] != 0; i++)
	{
		if(ast->code[i] == '\n')
			line++;

		if(line == entry->line)
		{
			entry->startPos = ++i;
			for(; ast->code[i] != 0; i++)
			{
				if(ast->code[i] == '\n')
				{
					entry->endPos = i;
					return;
				}
			}
			break;
		}
	}

	printf("WARNING: Invalid breakpoint file '%s' has no line %d\n", entry->file, entry->line);
}

void ptrs_debug_mainLoop(ptrs_ast_t *ast, ptrs_scope_t *scope, bool hasStarted)
{
	breakAfterStep = false;
	char buff[1024];

	while(true)
	{
		printf("> ");
		fflush(stdout);
		fgets(buff, 1024, stdin);

		if((hasStarted && strncmp(buff, "run", 3) == 0) || (!hasStarted && strncmp(buff, "continue", 8) == 0))
		{
			return;
		}
		else if(strncmp(buff, "break", 5) == 0)
		{
			char *fileEnd = strchr(buff + 6, ' ');
			int line;
			if(fileEnd == NULL || (line = atoi(fileEnd + 1)) <= 0)
			{
				printf("Usage: break <file> <line>\ne.g. break main.ptrs 42\n");
			}
			else
			{
				*fileEnd = 0;
				ptrs_breakpoint_t *entry = malloc(sizeof(ptrs_breakpoint_t));
				entry->file = strdup(buff + 6);
				entry->line = line;

				if(ast != NULL && strcmp(entry->file, ast->file) == 0)
					calculateFilePos(ast, entry);
				else
					entry->endPos = -1;

				entry->next = breakPoints;
				breakPoints = entry;

				printf("Added breakpoint in '%s' line %d\n", entry->file, line);
			}
		}
		else if(strncmp(buff, "clear", 5) == 0)
		{
			char *fileEnd = strchr(buff + 6, ' ');
			int line;
			if(fileEnd == NULL || (line = atoi(fileEnd + 1)) <= 0)
			{
				printf("Usage: break <file> <line>\ne.g. break main.ptrs 42\n");
			}
			else
			{
				*fileEnd = 0;
				ptrs_breakpoint_t *prev = NULL;
				ptrs_breakpoint_t *curr = breakPoints;
				while(curr != NULL)
				{
					if(curr->line == line && strcmp(curr->file, buff + 6) == 0)
					{
						if(prev == NULL)
							breakPoints = curr->next;
						else
							prev->next = curr->next;
						free(curr->file);
						free(curr);

						printf("Removed breakpoint from '%s' line %d\n", buff + 6, line);
						break;
					}

					prev = curr;
					curr = curr->next;
				}

				if(curr == NULL)
					printf("No breakpoint at '%s' line %d\n", buff + 6, line);
			}
		}
		else if(strncmp(buff, "step", 4) == 0)
		{
			breakAfterStep = true;
			return;
		}
		else if(strncmp(buff, "print", 5) == 0)
		{
			//TODO
		}
		else
		{
			printf("Unknown command!\n");
		}
	}
}

void ptrs_debug_break(ptrs_ast_t *ast, ptrs_scope_t *scope, char *reason, ...)
{
	va_list ap;
	va_start(ap, reason);
	vprintf(reason, ap);

	ptrs_showpos(stdout, ast);

	ptrs_debug_mainLoop(ast, scope, false);
}

void ptrs_debug_update(ptrs_ast_t *ast, ptrs_scope_t *scope)
{
	if(breakAfterStep)
		ptrs_debug_break(ast, scope, "");

	ptrs_breakpoint_t *curr = breakPoints;
	while(curr != NULL)
	{
		if(curr->endPos == -1 && lastFile != ast->file && strcmp(ast->file, curr->file) == 0)
			calculateFilePos(ast, curr);

		if(curr->startPos < ast->codepos && curr->endPos > ast->codepos)
			ptrs_debug_break(ast, scope, "Reached breakpoint");

		curr = curr->next;
	}
	lastFile = ast->file;
}
