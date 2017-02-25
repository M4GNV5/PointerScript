#ifndef _PTRS_AST
#define _PTRS_AST

#include <stdbool.h>
#include <pthread.h>

struct ptrs_ast;
struct ptrs_astlist;
typedef struct ptrs_symboltable ptrs_symboltable_t;

#include "common.h"
#include "../interpreter/include/scope.h"

struct ptrs_ast_define
{
	ptrs_symbol_t symbol;
	struct ptrs_ast *value;
	union
	{
		struct ptrs_astlist *initVal;
		struct ptrs_ast *initExpr;
	};
	uint8_t isInitExpr : 1;
	uint8_t onStack : 1;
};

struct ptrs_ast_tls
{
	ptrs_var_t startVal;
	pthread_key_t key;
};

struct ptrs_ast_lazy
{
	ptrs_symbol_t symbol;
	struct ptrs_ast *value;
};

struct ptrs_ast_member
{
	struct ptrs_ast *base;
	char *name;
	int namelen;
};

struct ptrs_ast_thismember
{
	ptrs_symbol_t base;
	struct ptrs_structlist *member;
};

struct ptrs_ast_import
{
	ptrs_symbol_t wildcards;
	int wildcardCount;
	struct ptrs_importlist *imports;
	struct ptrs_ast *from;
};

struct ptrs_ast_wildcard
{
	ptrs_symbol_t symbol;
	int index;
};

struct ptrs_ast_trycatch
{
	struct ptrs_ast *tryBody;
	struct ptrs_ast *catchBody;
	struct ptrs_ast *finallyBody;
	unsigned catchStackOffset;
	int argc;
	ptrs_symbol_t *args;
	ptrs_symbol_t retVal;
};

struct ptrs_ast_asm
{
	int importCount;
	int exportCount;

	const char **imports;
	struct ptrs_ast **importAsts;

	void **exports;
	ptrs_symbol_t *exportSymbols;

	intptr_t (*asmFunc)();
	struct jitas_context *context;
};

struct ptrs_ast_function
{
	ptrs_symbol_t symbol;
	bool isAnonymous;
	ptrs_function_t func;
};

struct ptrs_ast_scopestmt
{
	struct ptrs_ast *body;
	unsigned stackOffset;
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
	bool onStack;
};

struct ptrs_ast_call
{
	ptrs_nativetype_info_t *retType;
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

struct ptrs_ast_with
{
	struct ptrs_ast *base;
	struct ptrs_ast *body;
	ptrs_symbol_t symbol;
	int count;
	unsigned memberBuff;
};

struct ptrs_ast_withmember
{
	const char *name;
	ptrs_symbol_t base;
	int index;
};

struct ptrs_ast_lock
{
	union
	{
		pthread_mutex_t mutex;
		struct ptrs_ast *value;
	};
	struct ptrs_ast *body;
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
	ptrs_symbol_t *varsymbols;
	struct ptrs_ast *value;
	struct ptrs_ast *body;
	unsigned stackOffset;
};

struct ptrs_ast_yield
{
	ptrs_symbol_t yieldVal;
	union
	{
		struct ptrs_astlist *values;
		struct ptrs_ast *value;
	};
};

union ptrs_ast_arg
{
	char *strval;
	ptrs_symbol_t varval;
	ptrs_var_t constval;
	ptrs_struct_t structval;

	struct ptrs_ast *astval;
	struct ptrs_astlist *astlist;
	struct ptrs_algorithmlist *algolist;

	struct ptrs_ast_define define;
	struct ptrs_ast_tls tls;
	struct ptrs_ast_lazy lazy;
	struct ptrs_ast_member member;
	struct ptrs_ast_thismember thismember;
	struct ptrs_ast_import import;
	struct ptrs_ast_wildcard wildcard;
	struct ptrs_ast_trycatch trycatch;
	struct ptrs_ast_asm asmstmt;
	struct ptrs_ast_function function;
	struct ptrs_ast_scopestmt scopestatement;
	struct ptrs_ast_strformat strformat;
	struct ptrs_ast_cast cast;
	struct ptrs_ast_binary binary;
	struct ptrs_ast_ternary ternary;
	struct ptrs_ast_slice slice;
	struct ptrs_ast_call call;
	struct ptrs_ast_new newexpr;
	struct ptrs_ast_with with;
	struct ptrs_ast_withmember withmember;
	struct ptrs_ast_lock lock;
	struct ptrs_ast_ifelse ifelse;
	struct ptrs_ast_switch switchcase;
	struct ptrs_ast_control control;
	struct ptrs_ast_for forstatement;
	struct ptrs_ast_forin forin;
	struct ptrs_ast_yield yield;
};

typedef ptrs_var_t *(*ptrs_asthandler_t)(struct ptrs_ast *, ptrs_var_t *, ptrs_scope_t *);
typedef ptrs_var_t *(*ptrs_callhandler_t)(struct ptrs_ast *, ptrs_var_t *, ptrs_scope_t *,
	ptrs_nativetype_info_t *, struct ptrs_ast *, struct ptrs_astlist *);

struct ptrs_ast
{
	union ptrs_ast_arg arg;
	ptrs_asthandler_t handler;
	ptrs_asthandler_t setHandler;
	ptrs_asthandler_t addressHandler;
	ptrs_callhandler_t callHandler;
	int codepos;
	char *code;
	const char *file;
};
typedef struct ptrs_ast ptrs_ast_t;

struct ptrs_astlist
{
	struct ptrs_ast *entry;
	struct ptrs_astlist *next;
	uint8_t expand : 1;
	uint8_t lazy : 1;
};

struct ptrs_algorithmlist
{
	struct ptrs_ast *entry;
	struct ptrs_algorithmlist *next;
	uint8_t flags : 2; //0 = filter, 1 = negated filter, 2 = value replacer, 3 = index filter
	uint8_t orCombine : 1; //is || combined with next one
};

struct ptrs_stringformat
{
	struct ptrs_ast *entry;
	struct ptrs_stringformat *next;
	uint8_t convert : 1;
};

struct ptrs_importlist
{
	union
	{
		int wildcardIndex;
		ptrs_symbol_t symbol;
	};
	char *name;
	struct ptrs_importlist *next;
};

ptrs_ast_t *ptrs_parse(char *src, const char *filename, ptrs_symboltable_t **symbols);
int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_ast_t **node);

#endif
