#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/stack.h"
#include "include/scope.h"
#include "include/conversion.h"
#include "include/run.h"
#include "include/error.h"

static bool handleSignals = true;
extern size_t ptrs_arraymax;
extern bool ptrs_overflowError;

static struct option options[] = {
	{"stack-size", required_argument, 0, 1},
	{"array-max", required_argument, 0, 2},
	{"overflow", no_argument, 0, 3},
	{"no-sig", no_argument, 0, 4},
	{"help", no_argument, 0, 5},
	{0, 0, 0, 0}
};

static int parseOptions(int argc, char **argv)
{
	for(;;)
	{
		int ret = getopt_long_only(argc, argv, "", options, NULL);

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
				printf("Usage: ptrs [options ...] <file> [script options ...]\n"
					"Valid Options:\n"
						"\t--help               Show this information\n"
						"\t--stack-size <size>  Set stack size to 'size' bytes. Default: 0x%X\n"
						"\t--array-max <size>   Set maximal allowed array size to 'size' bytes. Default: 0x%X\n"
						"\t--overflow           Throw an error when trying to assign a non fitting value. Default: false\n"
						"\t--no-sig             Do not listen to signals.\n"
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

	int len = argc - i + 1;
	ptrs_var_t arguments[len];
	for(int j = 0; j < len; j++)
	{
		arguments[j].type = PTRS_TYPE_NATIVE;
		arguments[j].value.strval = argv[i++];
	}
	arguments[len].type = PTRS_TYPE_NATIVE;
	arguments[len].value.nativeval = NULL;

	result.type = PTRS_TYPE_POINTER;
	result.value.ptrval = arguments;
	ptrs_scope_set(scope, "arguments", &result);

	ptrs_dofile(file, &result, scope);

	return EXIT_SUCCESS;
}
