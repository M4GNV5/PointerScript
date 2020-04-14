#ifndef _PTRS_AST
#define _PTRS_AST

#include <stdbool.h>
#include <jit/jit.h>

struct ptrs_ast;
struct ptrs_astlist;
typedef struct ptrs_symboltable ptrs_symboltable_t;

#include "common.h"

struct ptrs_ast_define
{
	ptrs_jit_var_t location;
	struct ptrs_ast *value;
	union
	{
		struct ptrs_astlist *initVal;
		struct ptrs_ast *initExpr;
		ptrs_vartype_t type;
	};
	uint8_t isInitExpr : 1;
	uint8_t onStack : 1;
	uint8_t isArrayExpr : 1;
};

struct ptrs_ast_identifier
{
	ptrs_jit_var_t *location;
	ptrs_val_t valuePrediction;
	ptrs_meta_t metaPrediction;
	uint8_t typePredicted : 1;
	uint8_t valuePredicted : 1;
	uint8_t metaPredicted : 1;
};

struct ptrs_ast_member
{
	struct ptrs_ast *base;
	char *name;
	int namelen;
};

struct ptrs_ast_import
{
	union
	{
		ptrs_var_t *symbols;
		struct ptrs_ast **expressions;
	};
	int count;
	bool isScriptImport;
	struct ptrs_importlist *imports;
	struct ptrs_importlist *lastImport;
	struct ptrs_ast *from;
};

struct ptrs_ast_importedsymbol
{
	ptrs_nativetype_info_t *type; //optional
	struct ptrs_ast *import;
	int index;
};

struct ptrs_ast_trycatch
{
	struct ptrs_ast *tryBody;
	struct ptrs_ast *catchBody;
	struct ptrs_ast *finallyBody;
	ptrs_funcparameter_t *args;
	ptrs_jit_var_t retVal;
};

struct ptrs_ast_function
{
	jit_function_t symbol;
	ptrs_function_t func;
	uint8_t isExpression : 1;
};

struct ptrs_ast_strformat
{
	char *str;
	struct ptrs_stringformat *insertions;
	int insertionCount;
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
	union
	{
		ptrs_vartype_t builtinType;
		struct ptrs_ast *type;
	};
	struct ptrs_ast *value;
};

struct ptrs_ast_call
{
	ptrs_typing_t typing;
	struct ptrs_ast *value;
	struct ptrs_astlist *arguments;
};

struct ptrs_ast_new
{
	bool onStack;
	struct ptrs_ast *value;
	struct ptrs_astlist *arguments;
};

struct ptrs_ast_case
{
	int64_t min;
	int64_t max;
	struct ptrs_ast *body;
	struct ptrs_ast_case *next;
};
struct ptrs_ast_switch
{
	struct ptrs_ast *condition;
	int64_t min;
	int64_t max;
	size_t caseCount;
	struct ptrs_ast_case *cases;
	struct ptrs_ast *defaultCase;
};

struct ptrs_ast_ifelse
{
	struct ptrs_ast *condition;
	struct ptrs_ast *ifBody;
	struct ptrs_ast *elseBody;
};

struct ptrs_ast_control
{
	struct ptrs_ast *condition;
	struct ptrs_ast *body;
};

struct ptrs_ast_for
{
	struct ptrs_ast *init;
	struct ptrs_ast *condition;
	struct ptrs_ast *step;
	struct ptrs_ast *body;
};

struct ptrs_ast_forin
{
	int varcount;
	ptrs_jit_var_t *varsymbols;
	struct ptrs_ast *value;
	struct ptrs_ast *body;
};

struct ptrs_ast_yield
{
	ptrs_jit_var_t *body;
	ptrs_jit_var_t *returnInfo;
	struct ptrs_astlist *values;
};

union ptrs_ast_arg
{
	char *strval;
	ptrs_jit_var_t *varval;
	ptrs_var_t constval;
	ptrs_struct_t structval;
	struct ptrs_ast_function *funcval;

	struct ptrs_ast *astval;
	struct ptrs_astlist *astlist;
	struct ptrs_algorithmlist *algolist;

	struct ptrs_ast_define define;
	struct ptrs_ast_identifier identifier;
	struct ptrs_ast_member member;
	struct ptrs_ast_import import;
	struct ptrs_ast_importedsymbol importedsymbol;
	struct ptrs_ast_trycatch trycatch;
	struct ptrs_ast_function function;
	struct ptrs_ast_strformat strformat;
	struct ptrs_ast_cast cast;
	struct ptrs_ast_binary binary;
	struct ptrs_ast_ternary ternary;
	struct ptrs_ast_slice slice;
	struct ptrs_ast_call call;
	struct ptrs_ast_new newexpr;
	struct ptrs_ast_ifelse ifelse;
	struct ptrs_ast_switch switchcase;
	struct ptrs_ast_control control;
	struct ptrs_ast_for forstatement;
	struct ptrs_ast_forin forin;
	struct ptrs_ast_yield yield;
};

typedef ptrs_jit_var_t (*ptrs_asthandler_t)(struct ptrs_ast *, jit_function_t, ptrs_scope_t *);
typedef void (*ptrs_sethandler_t)(struct ptrs_ast *, jit_function_t, ptrs_scope_t *, ptrs_jit_var_t);
typedef ptrs_jit_var_t (*ptrs_callhandler_t)(struct ptrs_ast *, jit_function_t, ptrs_scope_t *,
	struct ptrs_ast *, ptrs_typing_t *retType, struct ptrs_astlist *);

typedef struct
{
	ptrs_asthandler_t get;
	ptrs_sethandler_t set;
	ptrs_asthandler_t addressof;
	ptrs_callhandler_t call;
} ptrs_ast_vtable_t;

struct ptrs_ast
{
	union ptrs_ast_arg arg;
	ptrs_ast_vtable_t *vtable;
	size_t codepos;
	char *code;
	const char *file;
};
typedef struct ptrs_ast ptrs_ast_t;

struct ptrs_astlist
{
	struct ptrs_ast *entry;
	struct ptrs_astlist *next;
};

struct ptrs_stringformat
{
	struct ptrs_ast *entry;
	struct ptrs_stringformat *next;
	uint8_t convert : 1;
};

struct ptrs_importlist
{
	char *name;
	union
	{
		ptrs_jit_var_t location;
		int wildcardIndex;
	};
	struct ptrs_importlist *next;
};

ptrs_ast_t *ptrs_parse(char *src, const char *filename, ptrs_symboltable_t **symbols);
int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_ast_t **node);

#endif
