#ifndef _PTRS_AST
#define _PTRS_AST

#include <stdbool.h>

struct ptrs_ast;
struct ptrs_astlist;
typedef struct ptrs_symboltable ptrs_symboltable_t;

#include "common.h"
#include "../interpreter/include/scope.h"

struct ptrs_ast_define
{
	ptrs_symbol_t symbol;
	struct ptrs_ast *value;
	struct ptrs_astlist *initVal;
	struct ptrs_ast *initExpr;
};

struct ptrs_ast_member
{
	struct ptrs_ast *base;
	char *name;
};

struct ptrs_ast_import
{
	int count;
	char **fields;
	ptrs_symbol_t *symbols;
	struct ptrs_ast *from;
};

struct ptrs_ast_trycatch
{
	struct ptrs_ast *tryBody;
	struct ptrs_ast *catchBody;
	unsigned tryStackOffset;
	unsigned catchStackOffset;
	int argc;
	ptrs_symbol_t *args;
};

struct ptrs_ast_function
{
	ptrs_symbol_t symbol;
	bool isAnonymous;
	int argc;
	char *name;
	ptrs_symbol_t vararg;
	ptrs_symbol_t *args;
	struct ptrs_ast **argv;
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_scopestmt
{
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_strformat
{
	char *str;
	struct ptrs_astlist *args;
};

struct ptrs_ast_binary
{
	struct ptrs_ast *left;
	struct ptrs_ast *right;
};

struct ptrs_ast_ternary
{
	struct ptrs_ast *condition;
	struct ptrs_ast *trueVal;
	struct ptrs_ast *falseVal;
};

struct ptrs_ast_slice
{
	struct ptrs_ast *base;
	struct ptrs_ast *start;
	struct ptrs_ast *end;
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

struct ptrs_ast_case
{
	int64_t value;
	struct ptrs_ast *body;
	struct ptrs_ast_case *next;
};
struct ptrs_ast_switch
{
	struct ptrs_ast *condition;
	struct ptrs_ast_case *cases;
	struct ptrs_ast *defaultCase;
};

struct ptrs_ast_ifelse
{
	struct ptrs_ast *condition;
	struct ptrs_ast *ifBody;
	struct ptrs_ast *elseBody;
	unsigned ifStackOffset;
	unsigned elseStackOffset;
};

struct ptrs_ast_control
{
	struct ptrs_ast *condition;
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_for
{
	struct ptrs_ast *init;
	struct ptrs_ast *condition;
	struct ptrs_ast *step;
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_forin
{
	int varcount;
	ptrs_symbol_t *varsymbols;
	struct ptrs_ast *value;
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_yield
{
	ptrs_symbol_t yieldVal;
	struct ptrs_astlist *values;
};

union ptrs_ast_arg
{
	char *strval;
	ptrs_symbol_t varval;
	ptrs_var_t constval;
	ptrs_struct_t structval;

	struct ptrs_ast *astval;
	struct ptrs_astlist *astlist;

	struct ptrs_ast_define define;
	struct ptrs_ast_member member;
	struct ptrs_ast_import import;
	struct ptrs_ast_trycatch trycatch;
	struct ptrs_ast_function function;
	struct ptrs_ast_scopestmt scopestatement;
	struct ptrs_ast_strformat strformat;
	struct ptrs_ast_cast cast;
	struct ptrs_ast_binary binary;
	struct ptrs_ast_ternary ternary;
	struct ptrs_ast_slice slice;
	struct ptrs_ast_call call;
	struct ptrs_ast_ifelse ifelse;
	struct ptrs_ast_switch switchcase;
	struct ptrs_ast_control control;
	struct ptrs_ast_for forstatement;
	struct ptrs_ast_forin forin;
	struct ptrs_ast_yield yield;
};

typedef ptrs_var_t* (*ptrs_asthandler_t)(struct ptrs_ast *, ptrs_var_t *, ptrs_scope_t *);

struct ptrs_ast
{
	union ptrs_ast_arg arg;
	ptrs_asthandler_t handler;
	ptrs_asthandler_t setHandler;
	int codepos;
	char *code;
	const char *file;
};
typedef struct ptrs_ast ptrs_ast_t;

struct ptrs_astlist
{
	struct ptrs_ast *entry;
	struct ptrs_astlist *next;
	bool expand;
};

ptrs_ast_t *ptrs_parse(char *src, const char *filename, ptrs_symboltable_t **symbols);
int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_ast_t **node);

#endif
