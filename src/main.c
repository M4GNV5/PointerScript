#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int main(int argc, char **argv)
{
	FILE *fd = fopen(argv[1], "rb");
	fseek(fd, 0, SEEK_END);
	long fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char *src = malloc(fsize + 1);
	fread(src, fsize, 1, fd);
	fclose(fd);
	src[fsize] = 0;

	ptrs_ast_t *ast = parse(src);
	ast->handler(ast);

	return 0;
}
