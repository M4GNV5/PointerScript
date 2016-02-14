#ifndef _PTRS_AST
#define _PTRS_AST

struct ptrs_ast;
struct ptrs_astlist;

struct ptrs_ast_define
{
	char *name;
	struct ptrs_ast *value;
};

struct ptrs_ast_binary
{
	struct ptrs_ast *left;
	struct ptrs_ast *right;
};

struct ptrs_ast_cast
{
	ptrs_vartype_t type;
	struct ptrs_ast *value;
};

struct ptrs_ast_call
{
	struct ptrs_ast *value;
	struct ptrs_astlist *arguments;
};

struct ptrs_ast_body
{
	struct ptrs_astlist *nodes;
};

struct ptrs_ast_controlstatement
{
	struct ptrs_ast *condition;
	struct ptrs_ast *body;
};

struct ptrs_ast_forstatement
{
	struct ptrs_ast *init;
	struct ptrs_ast *condition;
	struct ptrs_ast *step;
	struct ptrs_ast *body;
};

union ptrs_ast_arg
{
	char *strval;
	int64_t intval;
	double floatval;
	struct ptrs_ast *astval;

	struct ptrs_ast_define define;
	struct ptrs_ast_cast cast;
	struct ptrs_ast_binary binary;
	struct ptrs_ast_call call;
	struct ptrs_ast_body body;
	struct ptrs_ast_controlstatement control;
	struct ptrs_ast_forstatement forstatement;
};

typedef ptrs_var_t* (*ptrs_asthandler_t)(struct ptrs_ast*);

struct ptrs_ast
{
	union ptrs_ast_arg arg;
	ptrs_asthandler_t handler;
};
typedef struct ptrs_ast ptrs_ast_t;

struct ptrs_astlist
{
	struct ptrs_ast *entry;
	struct ptrs_astlist *next;
};

ptrs_ast_t *parse(char *code);

#endif
