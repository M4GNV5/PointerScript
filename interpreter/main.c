#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/scope.h"
#include "include/debug.h"
#include "include/conversion.h"
#include "include/run.h"
#include "include/error.h"

static bool handleSignals = true;
extern size_t ptrs_arraymax;
extern bool ptrs_zeroMemory;
extern int ptrs_asmSize;

static struct option options[] = {
	{"stack-size", required_argument, 0, 1},
	{"array-max", required_argument, 0, 2},
	{"no-sig", no_argument, 0, 3},
	{"zero-mem", no_argument, 0, 4},
	{"debug", no_argument, 0, 5},
	{"asm-size", required_argument, 0, 6},
	{"error", required_argument, 0, 7},
	{"help", no_argument, 0, 8},
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
				handleSignals = false;
				break;
			case 4:
				ptrs_zeroMemory = true;
				break;
			case 5:
				ptrs_debugEnabled = true;
				break;
			case 6:
				ptrs_asmSize = strtoul(optarg, NULL, 0);
				break;
			case 7:
				ptrs_errorfile = fopen(optarg, "w");
				if(ptrs_errorfile == NULL)
				{
					fprintf(stderr, "Could not open %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 8:
				printf("Usage: ptrs [options ...] <file> [script options ...]\n"
					"Valid Options:\n"
						"\t--help               Show this information\n"
						"\t--stack-size <size>  Set stack size to 'size' bytes. Default: 0x%X\n"
						"\t--array-max <size>   Set maximal allowed array size to 'size' bytes. Default: 0x%X\n"
						"\t--asm-size <size>    Set size of memory region containing inline assembly. Default: 0x1000\n"
						"\t--error <file>       Set where error messages are written to. Default: /dev/stderr\n"
						"\t--no-sig             Do not listen to signals.\n"
						"\t--zero-mem           Zero memory of arrays when created on the stack\n"
					"Source code can be found at https://github.com/M4GNV5/PointerScript\n", PTRS_STACK_SIZE, PTRS_STACK_SIZE);
				exit(EXIT_SUCCESS);
			default:
				fprintf(stderr, "Try '--help' for more information.\n");
				exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	ptrs_errorfile = stderr;
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
