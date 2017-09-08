#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <jit/jit-dump.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/run.h"
#include "include/error.h"
#include "include/conversion.h"

static bool handleSignals = true;
static bool interactive = false;
static bool dumpOps = false;

extern size_t ptrs_arraymax;
extern bool ptrs_compileAot;

static struct option options[] = {
	{"help", no_argument, 0, 1},
	{"array-max", required_argument, 0, 2},
	{"no-sig", no_argument, 0, 3},
	{"no-aot", no_argument, 0, 4},
	{"asmdump", no_argument, 0, 5},
	{"error", required_argument, 0, 6},
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
				printf("Usage: ptrs [options ...] <file> [script options ...]\n"
					"Valid Options:\n"
						"\t--help               Show this information\n"
						"\t--array-max <size>   Set maximal allowed array size to 'size' bytes. Default: 0x%X\n"
						"\t--error <file>       Set where error messages are written to. Default: /dev/stderr\n"
						"\t--no-sig             Do not listen to signals.\n"
						"\t--no-aot             Disable AOT compilation\n"
						"\t--asmdump            Output disassembly of generated instructions\n"
					"Source code can be found at https://github.com/M4GNV5/PointerScript\n", UINT32_MAX);
				exit(EXIT_SUCCESS);
			case 2:
				ptrs_arraymax = strtoul(optarg, NULL, 0);
				break;
			case 3:
				handleSignals = false;
				break;
			case 4:
				ptrs_compileAot = false;
				break;
			case 5:
				dumpOps = true;
				break;
			case 6:
				ptrs_errorfile = fopen(optarg, "w");
				if(ptrs_errorfile == NULL)
				{
					fprintf(stderr, "Could not open %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			default:
				fprintf(stderr, "Try '--help' for more information.\n");
				exit(EXIT_FAILURE);
		}
	}
}

void exitOnError()
{
	ptrs_error_t *error = jit_exception_get_last();
	if(error != NULL)
	{
		ptrs_printError(error);
		exit(EXIT_FAILURE);
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

	int len = argc - i;
	ptrs_var_t arguments[len];
	for(int j = 0; j < len; j++)
	{
		arguments[j].value.strval = argv[i++];
		arguments[j].meta.type = PTRS_TYPE_NATIVE;
		arguments[j].meta.array.readOnly = false;
	}

	jit_init();
	ptrs_result_t *result = ptrs_compilefile(file);
	exitOnError();

	if(dumpOps)
	{
		jit_function_t curr = jit_function_next(ptrs_jit_context, NULL);
		while(curr != NULL)
		{
			const char *name = jit_function_get_meta(curr, PTRS_JIT_FUNCTIONMETA_NAME);
			jit_dump_function(stdout, curr, name);
			curr = jit_function_next(ptrs_jit_context, curr);
		}

		return EXIT_SUCCESS;
	}
	else
	{
		ptrs_enableExceptions = true;

		ptrs_var_t ret;
		void *arg = arguments;
		jit_function_apply(result->func, &arg, &ret);

		exitOnError();

		return ptrs_vartoi(ret.value, ret.meta);
	}

}
