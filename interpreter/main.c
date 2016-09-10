#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/stack.h"
#include "include/scope.h"
#include "include/debug.h"
#include "include/conversion.h"
#include "include/run.h"
#include "include/error.h"

static bool handleSignals = true;
extern size_t ptrs_arraymax;
extern bool ptrs_overflowError;
extern bool ptrs_zeroMemory;

static struct option options[] = {
	{"stack-size", required_argument, 0, 1},
	{"array-max", required_argument, 0, 2},
	{"overflow", no_argument, 0, 3},
	{"no-sig", no_argument, 0, 4},
	{"zero-mem", no_argument, 0, 5},
	{"debug", no_argument, 0, 6},
	{"help", no_argument, 0, 7},
	{0, 0, 0, 0}
};

static int parseOptions(int argc, char **argv)
{
	for(;;)
	{
		int ret = getopt_long_only(argc, argv, "+", options, NULL);

		switch(ret)
		{
			case -1:
				return optind;
			case 1:
				ptrs_stacksize = strtoul(optarg, NULL, 0);
				break;
			case 2:
				ptrs_arraymax = strtoul(optarg, NULL, 0);
				break;
			case 3:
				ptrs_overflowError = true;
				break;
			case 4:
				handleSignals = false;
				break;
			case 5:
				ptrs_zeroMemory = true;
				break;
			case 6:
				ptrs_debugEnabled = true;
				break;
			case 7:
				printf("Usage: ptrs [options ...] <file> [script options ...]\n"
					"Valid Options:\n"
						"\t--help               Show this information\n"
						"\t--stack-size <size>  Set stack size to 'size' bytes. Default: 0x%X\n"
						"\t--array-max <size>   Set maximal allowed array size to 'size' bytes. Default: 0x%X\n"
						"\t--overflow           Throw an error when trying to assign a non fitting value.\n"
						"\t--no-sig             Do not listen to signals.\n"
						"\t--zero-mem           Zero memory of arrays when created on the stack\n"
					"Source code can be found at https://github.com/M4GNV5/PointerScript", PTRS_STACK_SIZE, PTRS_STACK_SIZE);
				exit(EXIT_SUCCESS);
			default:
				fprintf(stderr, "Try '--help' for more information.\n");
				exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	int i = parseOptions(argc, argv);

	if(i == argc)
	{
		fprintf(stderr, "No input file specified\n");
		return EXIT_FAILURE;
	}

	char *file = argv[i++];

	if(handleSignals)
		ptrs_handle_signals();

	ptrs_var_t result;
	ptrs_scope_t *scope = calloc(1, sizeof(ptrs_scope_t));
	ptrs_alloc(scope, 0);

	int len = argc - i;
	ptrs_var_t arguments[len + 1];
	for(int j = 0; j < len; j++)
	{
		arguments[j].type = PTRS_TYPE_NATIVE;
		arguments[j].value.strval = argv[i++];
		arguments[j].meta.array.readOnly = false;
	}
	arguments[len].type = PTRS_TYPE_NATIVE;
	arguments[len].value.nativeval = NULL;

	ptrs_symbol_t argumentsSymbol = {0, 0};
	result.type = PTRS_TYPE_POINTER;
	result.value.ptrval = arguments;
	result.meta.array.size = len;
	ptrs_scope_set(scope, argumentsSymbol, &result);

	if(ptrs_debugEnabled)
		ptrs_debug_mainLoop(NULL, scope, true);
	ptrs_dofile(file, &result, scope, NULL);

	return EXIT_SUCCESS;
}
