#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include JIT_INCLUDE
#include "ast.h"

#define talloc(type) calloc(sizeof(type), 1)

typedef struct code code_t;
struct symbollist;

typedef enum
{
	PTRS_SYMBOL_DEFAULT,
	PTRS_SYMBOL_FUNCTION,
	PTRS_SYMBOL_CONST,
	PTRS_SYMBOL_TYPED,
	PTRS_SYMBOL_LAZY,
	PTRS_SYMBOL_WILDCARD,
} ptrs_symboltype_t;

struct symbollist
{
	union
	{
		void *data;
		ptrs_jit_var_t *location;
		jit_function_t *function;
		struct
		{
			ptrs_jit_var_t *location;
			unsigned index;
		} wildcard;
		struct
		{
			ptrs_jit_var_t *location;
			ptrs_ast_t *value;
		} lazy;
		struct
		{
			ptrs_jit_var_t *location;
			ptrs_nativetype_info_t *type;
		} typed;
	} arg;
	ptrs_symboltype_t type;
	char *text;
	struct symbollist *next;
};
struct wildcardsymbol
{
	ptrs_ast_t *importStmt;
	char *start;
	int startLen;
	struct wildcardsymbol *next;
};

struct ptrs_symboltable
{
	bool isInline;
	struct symbollist *current;
	struct wildcardsymbol *wildcards;
	ptrs_symboltable_t *outer;
};

struct code
{
	const char *filename;
	char *src;
	char curr;
	int pos;
	ptrs_symboltable_t *symbols;
	bool insideIndex;
	ptrs_jit_var_t *thisVar;
	ptrs_jit_var_t *yield;
};

static ptrs_ast_t *parseStmtList(code_t *code, char end);
static ptrs_ast_t *parseStatement(code_t *code);
static ptrs_ast_t *parseExpression(code_t *code, bool required);
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls, bool ignoreAlgo);
static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls, bool ignoreAlgo);
static struct ptrs_astlist *parseExpressionList(code_t *code, char end);
static ptrs_ast_t *parseNew(code_t *code, bool onStack);
static void parseMap(code_t *code, ptrs_ast_t *expr);
static void parseStruct(code_t *code, ptrs_struct_t *struc);
static void parseImport(code_t *code, ptrs_ast_t *stmt);
static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt);

static ptrs_vartype_t readTypeName(code_t *code);
static ptrs_nativetype_info_t *readNativeType(code_t *code);
static ptrs_asthandler_t readPrefixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readSuffixOperator(code_t *code, const char **label);
//static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label);
static char *readIdentifier(code_t *code);
static char *readString(code_t *code, int *length, struct ptrs_stringformat **insertions, int *insertionsCount);
static char readEscapeSequence(code_t *code);
static int64_t readInt(code_t *code, int base);
static double readDouble(code_t *code);

static void setSymbol(code_t *code, char *text, unsigned offset);
static void addSymbol(code_t *code, char *text, ptrs_jit_var_t *location);
static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, ptrs_symboltype_t type);
static ptrs_ast_t *getSymbol(code_t *code, char *text);
static void symbolScope_increase(code_t *code, int buildInCount, bool isInline);
static void symbolScope_decrease(code_t *code);

static bool lookahead(code_t *code, const char *str);
static void consume(code_t *code, const char *str);
static void consumec(code_t *code, char c);
static void consumecm(code_t *code, char c, const char *msg);
static void next(code_t *code);
static void rawnext(code_t *code);

static bool skipSpaces(code_t *code);
static bool skipComments(code_t *code);

static void unexpectedm(code_t *code, const char *expected, const char *msg);

#define unexpected(code, expected) \
	unexpectedm(code, expected, NULL)

ptrs_ast_t *ptrs_parse(char *src, const char *filename, ptrs_symboltable_t **symbols)
{
	code_t code;
	code.filename = filename;
	code.src = src;
	code.curr = src[0];
	code.pos = 0;
	code.insideIndex = false;
	code.thisVar = NULL;
	code.yield = NULL;

	if(symbols == NULL || *symbols == NULL)
	{
		code.symbols = NULL;
		symbolScope_increase(&code, 1, false);
		setSymbol(&code, strdup("arguments"), 0);
	}
	else
	{
		code.symbols = *symbols;
	}

	while(skipSpaces(&code) || skipComments(&code));

	if(src[0] == '#' && src[1] == '!')
	{
		while(code.curr != '\n')
		{
			code.pos++;
			code.curr = code.src[code.pos];
		}
		next(&code);
	}


	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = ptrs_handle_file;
	ast->arg.astval = parseStmtList(&code, 0);

	ast->file = filename;
	ast->code = src;
	ast->codepos = 0;

	if(symbols == NULL)
		symbolScope_decrease(&code);
	else
		*symbols = code.symbols;
	return ast;
}

int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_ast_t **node)
{
	if(node != NULL)
		*node = NULL;

	unsigned level = 0;
	while(symbols != NULL)
	{
		struct symbollist *curr = symbols->current;
		while(curr != NULL)
		{
			if(strcmp(curr->text, text) == 0)
			{
				ptrs_ast_t *ast;
				switch(curr->type)
				{
					case PTRS_SYMBOL_DEFAULT:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_identifier;
						ast->setHandler = ptrs_handle_assign_identifier;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;

						ast->arg.varval = curr->arg.location;
						break;

					case PTRS_SYMBOL_FUNCTION:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_functionidentifier;
						ast->setHandler = NULL;
						ast->addressHandler = NULL;
						ast->callHandler = ptrs_handle_call_functionidentifier;

						ast->arg.funcval = curr->arg.function;
						break;

					case PTRS_SYMBOL_CONST:
						*node = ast = talloc(ptrs_ast_t);
						memcpy(ast, curr->arg.data, sizeof(ptrs_ast_t));
						break;

					case PTRS_SYMBOL_TYPED:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_typed;
						ast->setHandler = ptrs_handle_assign_typed;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;

						ast->arg.typed.location = curr->arg.typed.location;
						ast->arg.typed.type = curr->arg.typed.type;
						break;

					case PTRS_SYMBOL_LAZY:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_lazy;
						ast->setHandler = NULL;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;
						ast->arg.lazy.location = curr->arg.lazy.location;
						ast->arg.lazy.value = curr->arg.lazy.value;
						break;

					case PTRS_SYMBOL_WILDCARD:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_wildcardsymbol;
						ast->setHandler = NULL;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;

						ast->arg.wildcard.location = curr->arg.wildcard.location;
						ast->arg.wildcard.index = curr->arg.wildcard.index;
						break;
				}
				return 0;
			}
			curr = curr->next;
		}

		if(!symbols->isInline)
			level++;
		symbols = symbols->outer;
	}
	return 1;
}

static ptrs_ast_t *parseStmtList(code_t *code, char end)
{
	ptrs_ast_t *elem = talloc(ptrs_ast_t);
	elem->handler = ptrs_handle_body;
	if(code->curr == end || code->curr == 0)
	{
		elem->arg.astlist = NULL;
		return elem;
	}

	struct ptrs_astlist *curr = talloc(struct ptrs_astlist);
	elem->arg.astlist = curr;

	for(;;)
	{
		curr->entry = parseStatement(code);
		if(curr->entry == NULL)
			continue;

		if(code->curr == end || code->curr == 0)
			break;

		struct ptrs_astlist *next = talloc(struct ptrs_astlist);
		curr->next = next;
		curr = next;
	}

	if(curr == elem->arg.astlist)
	{
		free(elem);
		elem = curr->entry;
		free(curr);
		return elem;
	}
	else
	{
		curr->next = NULL;
		return elem;
	}
}

int parseIdentifierList(code_t *code, char *end, ptrs_jit_var_t **symbols, char ***fields)
{
	int pos = code->pos;
	int count = 1;
	for(;;)
	{
		if(lookahead(code, end))
		{
			break;
		}
		else if(code->curr == ',' || code->curr == '_' || isspace(code->curr) || isalnum(code->curr))
		{
			if(code->curr == ',')
				count++;
			rawnext(code);
		}
		else
		{
			break;
		}
	}

	code->pos = pos;
	code->curr = code->src[pos];
	if(fields != NULL)
		*fields = malloc(sizeof(char *) * count);
	*symbols = malloc(sizeof(ptrs_jit_var_t) * count);

	for(int i = 0; i < count; i++)
	{
		if(fields != NULL && lookahead(code, "_"))
		{
			(*symbols)[i].val = NULL;
			(*symbols)[i].meta = NULL;
			(*symbols)[i].constType = -1;
		}
		else
		{
			char *name = readIdentifier(code);
			if(fields != NULL)
			{
				if(lookahead(code, "as"))
				{
					(*fields)[i] = name;
					name = readIdentifier(code);
				}
				else
				{
					(*fields)[i] = strdup(name);
				}
			}

			addSymbol(code, name, *symbols + i);
		}

		if(i < count - 1)
			consumec(code, ',');
	}

	return count;
}

struct argdeflist
{
	char *name;
	bool isLazy;
	ptrs_ast_t *value;
	struct argdeflist *next;
};
static int parseArgumentDefinitionList(code_t *code, ptrs_jit_var_t **args, ptrs_ast_t ***argv, ptrs_jit_var_t *vararg)
{
	bool hasArgv = false;
	int argc = 0;
	consumecm(code, '(', "Expected ( as the beginning of an argument definition");

	if(vararg != NULL)
	{
		vararg->val = NULL;
		vararg->meta = NULL;
	}

	if(code->curr == ')')
	{
		next(code);
		*args = NULL;
		if(argv != NULL)
			*argv = NULL;
		return argc;
	}
	else
	{
		struct argdeflist first;
		struct argdeflist *list = &first;
		for(;;)
		{
			char *name;
			bool isLazy;

			if(lookahead(code, "_"))
			{
				name = NULL;
			}
			else if(lookahead(code, "lazy"))
			{
				name = readIdentifier(code);
				isLazy = true;
			}
			else
			{
				name = readIdentifier(code);
				isLazy = false;

				if(vararg != NULL && lookahead(code, "..."))
				{
					consumecm(code, ')', "Vararg argument has to be the last argument");

					addSymbol(code, name, vararg);
					break;
				}
			}

			list->next = talloc(struct argdeflist);
			list = list->next;

			list->name = name;
			list->isLazy = isLazy;

			if(argv != NULL)
			{
				if(name != NULL && lookahead(code, "="))
				{
					list->value = parseExpression(code, true);
					hasArgv = true;
				}
				else
				{
					list->value = NULL;
				}
			}

			argc++;

			if(code->curr == ')')
			{
				next(code);
				break;
			}
			consumecm(code, ',', "Expected , between two arguments");
		}

		*args = malloc(sizeof(ptrs_jit_var_t) * argc);
		if(hasArgv)
			*argv = malloc(sizeof(ptrs_ast_t *) * argc);
		else if(argv != NULL)
			*argv = NULL;

		list = first.next;
		for(int i = 0; i < argc; i++)
		{
			if(list->name == NULL)
			{
				//ignore
			}
			if(list->isLazy)
			{
				struct symbollist *symbol = addSpecialSymbol(code, readIdentifier(code), PTRS_SYMBOL_LAZY);
				symbol->arg.lazy.location = *args + i;
				symbol->arg.lazy.value = NULL;
			}
			else
			{
				addSymbol(code, list->name, *args + i);
			}

			if(hasArgv)
				(*argv)[i] = list->value;

			struct argdeflist *old = list;
			list = list->next;
			free(old);
		}

		return argc;
	}
}

static ptrs_ast_t *parseScopelessBody(code_t *code, bool allowStmt)
{
	ptrs_ast_t *result;
	if(!allowStmt || code->curr == '{')
	{
		consumec(code, '{');
		result = parseStmtList(code, '}');
		consumec(code, '}');
	}
	else
	{
		result = parseStatement(code);
	}
	return result;
}

static ptrs_ast_t *parseBody(code_t *code, bool allowStmt, bool isFunction)
{
	if(!isFunction)
		symbolScope_increase(code, 0, true);

	ptrs_ast_t *result = parseScopelessBody(code, allowStmt);

	symbolScope_decrease(code);

	return result;
}

static ptrs_function_t *parseFunction(code_t *code, char *name)
{
	symbolScope_increase(code, 1, false);
	setSymbol(code, strdup("this"), 0);

	ptrs_function_t *func = talloc(ptrs_function_t);
	func->name = name;

	func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
	func->body = parseBody(code, false, true);

	return func;
}

static ptrs_ast_t *parseStatement(code_t *code)
{
	if(code->curr == '{')
	{
		return parseBody(code, false, false);
	}
	else if(lookahead(code, "const"))
	{
		char *name = readIdentifier(code);
		consumec(code, '=');
		ptrs_ast_t *ast = parseExpression(code, true);
		if(ast->handler != ptrs_handle_constant)
			unexpectedm(code, NULL, "Initializer for 'const' variable is not a constant");

		struct symbollist *entry = addSpecialSymbol(code, name, PTRS_SYMBOL_CONST);
		entry->arg.data = ast;
		consumec(code, ';');
		return NULL;
	}

	ptrs_ast_t *stmt = talloc(ptrs_ast_t);
	stmt->setHandler = NULL;
	stmt->addressHandler = NULL;
	stmt->callHandler = NULL;
	stmt->codepos = code->pos;
	stmt->code = code->src;
	stmt->file = code->filename;

	if(lookahead(code, "let"))
	{
		stmt->handler = ptrs_handle_typeddefine;
		addSymbol(code, readIdentifier(code), &stmt->arg.define.location);

		if(lookahead(code, ":"))
		{
			stmt->arg.define.type = readTypeName(code);

			if(stmt->arg.define.type > PTRS_TYPE_STRUCT)
				unexpected(code, "TypeName");
		}
		else
		{
			stmt->arg.define.type = PTRS_TYPE_STRUCT + 1;
		}

		if(lookahead(code, "="))
			stmt->arg.define.value = parseExpression(code, true);
		else if(stmt->arg.define.type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "A let statement must either have a type or an initializer");
		else
			stmt->arg.define.value = NULL;
	}
	else if(lookahead(code, "var"))
	{
		stmt->handler = ptrs_handle_define;
		addSymbol(code, readIdentifier(code), &stmt->arg.define.location);
		stmt->arg.define.onStack = true;

		if(lookahead(code, "["))
		{
			stmt->handler = ptrs_handle_vararray;
			stmt->arg.define.value = parseExpression(code, false);
			stmt->arg.define.isInitExpr = false;
			consumec(code, ']');

			if(lookahead(code, "="))
			{
				consumec(code, '[');
				stmt->arg.define.initVal = parseExpressionList(code, ']');
				consumec(code, ']');
			}
			else if(stmt->arg.define.value == NULL)
			{
				unexpectedm(code, "Array initializer for implicitly sized array", NULL);
			}
			else
			{
				stmt->arg.define.initVal = NULL;
			}
		}
		else if(lookahead(code, "{"))
		{
			stmt->handler = ptrs_handle_array;
			stmt->arg.define.value = parseExpression(code, false);
			consumec(code, '}');

			if(lookahead(code, "="))
			{
				if(lookahead(code, "{"))
				{
					stmt->arg.define.initVal = parseExpressionList(code, '}');
					stmt->arg.define.isInitExpr = false;
					consumec(code, '}');
				}
				else
				{
					stmt->arg.define.initExpr = parseExpression(code, true);
					stmt->arg.define.isInitExpr = true;
				}
			}
			else if(stmt->arg.define.value == NULL)
			{
				unexpectedm(code, "Array or String initializer for implicitly sized byte array", NULL);
			}
			else
			{
				stmt->arg.define.initExpr = NULL;
				stmt->arg.define.isInitExpr = true;
			}
		}
		else if(lookahead(code, ":"))
		{
			stmt->handler = ptrs_handle_structvar;
			stmt->arg.define.value = parseUnaryExpr(code, true, false);
			stmt->arg.define.isInitExpr = false;
			stmt->arg.define.onStack = true;

			consumec(code, '(');
			stmt->arg.define.initVal = parseExpressionList(code, ')');
			consumec(code, ')');
		}
		else if(lookahead(code, "="))
		{
			stmt->arg.define.value = parseExpression(code, true);
		}
		else
		{
			stmt->arg.define.value = NULL;
		}

		consumec(code, ';');
	}
	else if(lookahead(code, "import"))
	{
		parseImport(code, stmt);
	}
	else if(lookahead(code, "return"))
	{
		stmt->handler = ptrs_handle_return;
		stmt->arg.astval = parseExpression(code, false);
		consumec(code, ';');
	}
	else if(lookahead(code, "break"))
	{
		stmt->handler = ptrs_handle_break;
		consumec(code, ';');
	}
	else if(lookahead(code, "continue"))
	{
		stmt->handler = ptrs_handle_continue;
		consumec(code, ';');
	}
	else if(lookahead(code, "delete"))
	{
		stmt->handler = ptrs_handle_delete;
		stmt->arg.astval = parseExpression(code, true);
		consumec(code, ';');
	}
	else if(lookahead(code, "throw"))
	{
		stmt->handler = ptrs_handle_throw;
		stmt->arg.astval = parseExpression(code, true);
		consumec(code, ';');
	}
	else if(lookahead(code, "scoped"))
	{
		stmt->handler = ptrs_handle_scopestatement;
		symbolScope_increase(code, 0, false);
		stmt->arg.astval = parseBody(code, true, true);
	}
	else if(lookahead(code, "try"))
	{
		stmt->handler = ptrs_handle_trycatch;
		stmt->arg.trycatch.tryBody = parseBody(code, true, false);

		if(lookahead(code, "catch"))
		{
			symbolScope_increase(code, 0, false);
			stmt->arg.trycatch.argc = parseArgumentDefinitionList(code, &stmt->arg.trycatch.args, NULL, NULL);
			stmt->arg.trycatch.catchBody = parseScopelessBody(code, true);
			symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.catchBody = NULL;
		}

		if(lookahead(code, "finally"))
		{
			symbolScope_increase(code, 0, true);
			if(code->curr == '(')
			{
				consumec(code, '(');
				addSymbol(code, readIdentifier(code), &stmt->arg.trycatch.retVal);
				consumec(code, ')');
			}
			else
			{
				stmt->arg.trycatch.retVal.val = NULL;
				stmt->arg.trycatch.retVal.meta = NULL;
				stmt->arg.trycatch.retVal.constType = -1;
			}

			stmt->arg.trycatch.finallyBody = parseBody(code, true, false);
			symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.retVal.val = NULL;
			stmt->arg.trycatch.retVal.meta = NULL;
			stmt->arg.trycatch.retVal.constType = -1;
			stmt->arg.trycatch.finallyBody = NULL;
		}
	}
	else if(lookahead(code, "function"))
	{
		stmt->handler = ptrs_handle_function;
		stmt->arg.function.isAnonymous = false;

		ptrs_function_t *func = &stmt->arg.function.func;
		func->name = readIdentifier(code);

		stmt->arg.function.symbol = talloc(jit_function_t);
		*stmt->arg.function.symbol = NULL;

		struct symbollist *symbol = addSpecialSymbol(code, strdup(func->name), PTRS_SYMBOL_FUNCTION);
		symbol->arg.function = stmt->arg.function.symbol;

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

		func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
		func->body = parseBody(code, false, true);
	}
	else if(lookahead(code, "struct"))
	{
		stmt->handler = ptrs_handle_struct;
		parseStruct(code, &stmt->arg.structval);
	}
	else if(lookahead(code, "if"))
	{
		stmt->handler = ptrs_handle_if;

		consumec(code, '(');
		stmt->arg.ifelse.condition = parseExpression(code, true);
		consumec(code, ')');
		stmt->arg.ifelse.ifBody = parseBody(code, true, false);

		if(lookahead(code, "else"))
			stmt->arg.ifelse.elseBody = parseBody(code, true, false);
		else
			stmt->arg.ifelse.elseBody = NULL;
	}
	else if(lookahead(code, "switch"))
	{
		parseSwitchCase(code, stmt);
	}
	else if(lookahead(code, "while"))
	{
		stmt->handler = ptrs_handle_while;

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code, true);
		consumec(code, ')');
		stmt->arg.control.body = parseBody(code, true, false);
	}
	else if(lookahead(code, "do"))
	{
		symbolScope_increase(code, 0, true);
		stmt->handler = ptrs_handle_dowhile;
		stmt->arg.control.body = parseScopelessBody(code, true);
		consume(code, "while");

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code, true);
		consumec(code, ')');
		consumec(code, ';');
		symbolScope_decrease(code);
	}
	else if(lookahead(code, "foreach"))
	{
		consumec(code, '(');
		symbolScope_increase(code, 0, false);
		stmt->arg.forin.varcount = parseIdentifierList(code, "in", &stmt->arg.forin.varsymbols, NULL);

		consume(code, "in");
		stmt->arg.forin.value = parseExpression(code, true);
		stmt->handler = ptrs_handle_forin;
		consumec(code, ')');

		stmt->arg.forin.body = parseScopelessBody(code, true);
		symbolScope_decrease(code);
	}
	else if(lookahead(code, "for"))
	{
		stmt->handler = ptrs_handle_for;
		symbolScope_increase(code, 0, true);

		consumec(code, '(');
		stmt->arg.forstatement.init = parseStatement(code);
		stmt->arg.forstatement.condition = parseExpression(code, false);
		consumec(code, ';');
		stmt->arg.forstatement.step = parseExpression(code, false);
		consumec(code, ')');

		stmt->arg.forstatement.body = parseScopelessBody(code, true);
		symbolScope_decrease(code);
	}
	else
	{
		stmt->handler = ptrs_handle_exprstatement;
		stmt->arg.astval = parseExpression(code, false);
		consumec(code, ';');
	}
	return stmt;
}

struct opinfo
{
	const char *op;
	int precendence;
	bool rightToLeft;
	ptrs_asthandler_t handler;
};

struct opinfo binaryOps[] = {
	// >> and << need to be before > and < or we will always lookahead greater / less
	{">>", 12, false, ptrs_handle_op_shr}, //shift right
	{"<<", 12, false, ptrs_handle_op_shl}, //shift left

	{"===", 9, false, ptrs_handle_op_typeequal},
	{"!==", 9, false, ptrs_handle_op_typeinequal},

	{"==", 9, false, ptrs_handle_op_equal},
	{"!=", 9, false, ptrs_handle_op_inequal},
	{"<=", 10, false, ptrs_handle_op_lessequal},
	{">=", 10, false, ptrs_handle_op_greaterequal},
	{"<", 10, false, ptrs_handle_op_less},
	{">", 10, false, ptrs_handle_op_greater},

	{"=", 1, true, ptrs_handle_op_assign},
	{"+=", 1, true, ptrs_handle_op_addassign},
	{"-=", 1, true, ptrs_handle_op_subassign},
	{"*=", 1, true, ptrs_handle_op_mulassign},
	{"/=", 1, true, ptrs_handle_op_divassign},
	{"%=", 1, true, ptrs_handle_op_modassign},
	{">>=", 1, true, ptrs_handle_op_shrassign},
	{"<<=", 1, true, ptrs_handle_op_shlassign},
	{"&=", 1, true, ptrs_handle_op_andassign},
	{"^=", 1, true, ptrs_handle_op_xorassign},
	{"|=", 1, true, ptrs_handle_op_orassign},

	{"?", 2, true, ptrs_handle_op_ternary},
	{":", -1, true, ptrs_handle_op_ternary},

	{"instanceof", 11, false, ptrs_handle_op_instanceof},
	{"in", 11, false, ptrs_handle_op_in},

	{"||", 3, false, ptrs_handle_op_logicor},
	{"^^", 4, false, ptrs_handle_op_logicxor},
	{"&&", 5, false, ptrs_handle_op_logicand},

	{"|", 6, false, ptrs_handle_op_or},
	{"^", 7, false, ptrs_handle_op_xor},
	{"&", 8, false, ptrs_handle_op_and},

	{"+", 13, false, ptrs_handle_op_add},
	{"-", 13, false, ptrs_handle_op_sub},

	{"*", 14, false, ptrs_handle_op_mul},
	{"/", 14, false, ptrs_handle_op_div},
	{"%", 14, false, ptrs_handle_op_mod}
};
static int binaryOpCount = sizeof(binaryOps) / sizeof(struct opinfo);

struct opinfo prefixOps[] = {
	{"typeof", 0, true, ptrs_handle_prefix_typeof},
	{"++", 0, true, ptrs_handle_prefix_inc}, //prefixed ++
	{"--", 0, true, ptrs_handle_prefix_dec}, //prefixed --
	{"!", 0, true, ptrs_handle_prefix_logicnot}, //logical NOT
	{"~", 0, true, ptrs_handle_prefix_not}, //bitwise NOT
	{"&", 0, true, ptrs_handle_prefix_address}, //adress of
	{"*", 0, true, ptrs_handle_prefix_dereference}, //dereference
	{"+", 0, true, ptrs_handle_prefix_plus}, //unary +
	{"-", 0, true, ptrs_handle_prefix_minus} //unary -
};
static int prefixOpCount = sizeof(prefixOps) / sizeof(struct opinfo);

struct opinfo suffixedOps[] = {
	{"++", 0, false, ptrs_handle_suffix_inc}, //suffixed ++
	{"--", 0, false, ptrs_handle_suffix_dec} //suffixed --
};
static int suffixedOpCount = sizeof(suffixedOps) / sizeof(struct opinfo);

static ptrs_ast_t *parseExpression(code_t *code, bool required)
{
	ptrs_ast_t *ast = parseBinaryExpr(code, parseUnaryExpr(code, false, false), 0);

	if(required && ast == NULL)
		unexpected(code, "Expression");

	return ast;
}

static struct opinfo *peekBinaryOp(code_t *code)
{
	int pos = code->pos;
	for(int i = 0; i < binaryOpCount; i++)
	{
		if(lookahead(code, binaryOps[i].op))
		{
			code->pos = pos;
			code->curr = code->src[pos];
			return &binaryOps[i];
		}
	}

	return NULL;
}
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec)
{
	struct opinfo *ahead = peekBinaryOp(code);
	while(ahead != NULL && ahead->precendence >= minPrec)
	{
		struct opinfo *op = ahead;
		int pos = code->pos;
		consume(code, ahead->op);
		ptrs_ast_t *right = parseUnaryExpr(code, false, false);
		ahead = peekBinaryOp(code);

		while(ahead != NULL && ahead->precendence > op->precendence)
		{
			right = parseBinaryExpr(code, right, ahead->precendence);
			ahead = peekBinaryOp(code);
		}

		if(op->handler == ptrs_handle_op_ternary)
		{
			ptrs_ast_t *ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_op_ternary;
			ast->arg.ternary.condition = left;
			ast->arg.ternary.trueVal = right;
			consumec(code, ':');
			ast->arg.ternary.falseVal = parseExpression(code, true);
			left = ast;
			continue;
		}

		ptrs_ast_t *_left = left;
		left = talloc(ptrs_ast_t);
		left->handler = op->handler;
		left->setHandler = NULL;
		left->addressHandler = NULL;
		left->callHandler = NULL;
		left->arg.binary.left = _left;
		left->arg.binary.right = right;
		left->codepos = pos;
		left->code = code->src;
		left->file = code->filename;

		if(op->rightToLeft && _left->setHandler == NULL)
			PTRS_HANDLE_ASTERROR(left, "Invalid assign expression, left side is not a valid lvalue");
	}
	return left;
}

struct constinfo
{
	char *text;
	ptrs_vartype_t type;
	ptrs_val_t value;
	ptrs_meta_t meta;
};
struct constinfo constants[] = {
	{"true", PTRS_TYPE_INT, {.intval = true}},
	{"false", PTRS_TYPE_INT, {.intval = false}},
	{"NULL", PTRS_TYPE_NATIVE, {.nativeval = NULL}},
	{"null", PTRS_TYPE_POINTER, {.ptrval = NULL}},
	{"VARSIZE", PTRS_TYPE_INT, {.intval = sizeof(ptrs_var_t)}},
	{"PTRSIZE", PTRS_TYPE_INT, {.intval = sizeof(void *)}},
	{"undefined", PTRS_TYPE_UNDEFINED, {}},
	{"NaN", PTRS_TYPE_FLOAT, {.floatval = NAN}},
	{"Infinity", PTRS_TYPE_FLOAT, {.floatval = INFINITY}},
	{"PI", PTRS_TYPE_FLOAT, {.floatval = M_PI}},
	{"E", PTRS_TYPE_FLOAT, {.floatval = M_E}},
};
int constantCount = sizeof(constants) / sizeof(struct constinfo);

static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls, bool ignoreAlgo)
{
	char curr = code->curr;
	int pos = code->pos;
	bool noSetHandler = true;
	ptrs_ast_t *ast = NULL;


	ptrs_asthandler_t handler = readPrefixOperator(code, NULL);
	if(handler != NULL)
	{
		ast = talloc(ptrs_ast_t);
		ast->arg.astval = parseUnaryExpr(code, false, true);
		ast->handler = handler;
		ast->addressHandler = NULL;
		ast->callHandler = NULL;
		if(handler == ptrs_handle_prefix_dereference)
			ast->setHandler = ptrs_handle_assign_dereference;
		else
			ast->setHandler = NULL;

		ast->codepos = pos;
		ast->code = code->src;
		ast->file = code->filename;

		return ast;
	}

	for(int i = 0; i < constantCount; i++)
	{
		if(lookahead(code, constants[i].text))
		{
			ast = talloc(ptrs_ast_t);
			memset(&ast->arg.constval.meta, 0, sizeof(ptrs_meta_t));
			ast->arg.constval.meta.type = constants[i].type;
			ast->arg.constval.value = constants[i].value;
			ast->handler = ptrs_handle_constant;
			ast->setHandler = NULL;
			ast->addressHandler = NULL;
			ast->callHandler = NULL;
			return ast;
		}
	}

	if(lookahead(code, "new"))
	{
		ast = parseNew(code, false);
	}
	else if(lookahead(code, "new_stack")) //TODO find a better syntax for this
	{
		ast = parseNew(code, true);
	}
	else if(lookahead(code, "type"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "Syntax is type<TYPENAME>");

		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_constant;
		ast->arg.constval.meta.type = PTRS_TYPE_INT;
		ast->arg.constval.value.intval = type;
		consumec(code, '>');
	}
	else if(lookahead(code, "sizeof"))
	{
		int pos = code->pos;
		bool hasBrace = lookahead(code, "(");

		ptrs_nativetype_info_t *nativeType = readNativeType(code);

		ast = talloc(ptrs_ast_t);
		if(nativeType != NULL)
		{
			if(hasBrace)
				consumec(code, ')');

			ast->handler = ptrs_handle_constant;
			ast->arg.constval.meta.type = PTRS_TYPE_INT;
			ast->arg.constval.value.intval = nativeType->size;
		}
		else if(lookahead(code, "var"))
		{
			if(hasBrace)
				consumec(code, ')');

			ast->handler = ptrs_handle_constant;
			ast->arg.constval.meta.type = PTRS_TYPE_INT;
			ast->arg.constval.value.intval = sizeof(ptrs_var_t);
		}
		else
		{
			if(hasBrace)
			{
				code->pos = pos;
				code->curr = code->src[pos];
			}

			ast->handler = ptrs_handle_prefix_sizeof;
			ast->arg.astval = parseUnaryExpr(code, ignoreCalls, ignoreAlgo);
		}
	}
	else if(lookahead(code, "as"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "Syntax is as<TYPE>");

		consumec(code, '>');

		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_as;
		ast->arg.cast.builtinType = type;
		ast->arg.cast.value = parseUnaryExpr(code, false, true);
	}
	else if(lookahead(code, "cast_stack")) //TODO find a better syntax for this
	{
		consumec(code, '<');
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_cast;
		ast->arg.cast.type = parseUnaryExpr(code, false, false);
		ast->arg.cast.onStack = true;

		if(ast->arg.cast.type == NULL)
			unexpectedm(code, NULL, "Syntax is cast_stack<TYPE>");

		consumec(code, '>');
		ast->arg.cast.value = parseUnaryExpr(code, false, true);
	}
	else if(lookahead(code, "cast"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type <= PTRS_TYPE_STRUCT)
		{
			if(type != PTRS_TYPE_INT && type != PTRS_TYPE_FLOAT && type != PTRS_TYPE_NATIVE)
				unexpectedm(code, NULL, "Invalid cast, can only cast to int, float, native and string");

			ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_cast_builtin;
			ast->arg.cast.builtinType = type;
		}
		else if(lookahead(code, "string"))
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_tostring;
		}
		else
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_cast;
			ast->arg.cast.type = parseUnaryExpr(code, false, false);
			ast->arg.cast.onStack = false;

			if(ast->arg.cast.type == NULL)
				unexpectedm(code, NULL, "Syntax is cast<TYPE>");
		}

		consumec(code, '>');
		ast->arg.cast.value = parseUnaryExpr(code, false, true);
	}
	else if(lookahead(code, "yield"))
	{
		if(code->yield != NULL)
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_yield;
			ast->arg.yield.yieldVal = code->yield;
			ast->arg.yield.values = parseExpressionList(code, ';');
		}
		else
		{
			unexpectedm(code, NULL, "Yield expressions are only allowed in foreach and algorithm overloads");
		}
	}
	else if(lookahead(code, "function"))
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_function;
		ast->arg.function.isAnonymous = true;

		ptrs_function_t *func = &ast->arg.function.func;
		func->name = "(anonymous function)";

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

		func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
		func->body = parseBody(code, false, true);
	}
	else if(lookahead(code, "map_stack"))
	{
		ast = talloc(ptrs_ast_t);
		ast->arg.newexpr.onStack = true;
		parseMap(code, ast);
	}
	else if(lookahead(code, "map"))
	{
		ast = talloc(ptrs_ast_t);
		ast->arg.newexpr.onStack = false;
		parseMap(code, ast);
	}
	else if(isalpha(curr) || curr == '_')
	{
		char *name = readIdentifier(code);
		ast = getSymbol(code, name);
		noSetHandler = false;
		free(name);
	}
	else if(isdigit(curr) || curr == '.')
	{
		int startPos = code->pos;
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_constant;

		ast->arg.constval.meta.type = PTRS_TYPE_INT;
		ast->arg.constval.value.intval = readInt(code, 0);

		if((code->curr == '.' || code->curr == 'e') && isdigit(code->src[code->pos + 1]))
		{
			code->pos = startPos;
			code->curr = code->src[code->pos];
			ast->arg.constval.meta.type = PTRS_TYPE_FLOAT;
			ast->arg.constval.value.floatval = readDouble(code);
			lookahead(code, "f");
		}
		else if(lookahead(code, "f"))
		{
			ast->arg.constval.meta.type = PTRS_TYPE_FLOAT;
			ast->arg.constval.value.floatval = ast->arg.constval.value.intval;
		}
	}
	else if(curr == '$')
	{
		if(!code->insideIndex)
			unexpectedm(code, NULL, "$ can only be used inside the [] of index expressions");

		next(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_indexlength;
	}
	else if(curr == '\'')
	{
		rawnext(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_constant;
		ast->arg.constval.meta.type = PTRS_TYPE_INT;

		if(code->curr == '\\')
		{
			rawnext(code);
			ast->arg.constval.value.intval = readEscapeSequence(code);
		}
		else
		{
			ast->arg.constval.value.intval = code->curr;
		}
		rawnext(code);
		consumecm(code, '\'', "A char literal cannot contain multiple characters");
	}
	else if(curr == '"')
	{
		rawnext(code);
		int len;
		int insertionCount;
		struct ptrs_stringformat *insertions = NULL;
		char *str = readString(code, &len, &insertions, &insertionCount);

		ast = talloc(ptrs_ast_t);
		if(insertions != NULL)
		{
			ast->handler = ptrs_handle_stringformat;
			ast->arg.strformat.str = str;
			ast->arg.strformat.insertions = insertions;
			ast->arg.strformat.insertionCount = insertionCount;
		}
		else
		{
			ast->handler = ptrs_handle_constant;
			ast->arg.constval.meta.type = PTRS_TYPE_NATIVE;
			ast->arg.constval.value.strval = str;
			ast->arg.constval.meta.array.readOnly = true;
			ast->arg.constval.meta.array.size = len;
		}
	}
	else if(curr == '`')
	{
		rawnext(code);
		int len = 0;
		char *src = &code->src[code->pos];

		while(*src++ != '`')
			len++;

		char *str = malloc(len + 1);
		strncpy(str, &code->src[code->pos], len);
		str[len] = 0;
		code->pos += len + 1;

		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_constant;
		ast->arg.constval.meta.type = PTRS_TYPE_NATIVE;
		ast->arg.constval.value.strval = str;
		ast->arg.constval.meta.array.readOnly = true;
		ast->arg.constval.meta.array.size = len;
	}
	else if(curr == '(')
	{
		int start = code->pos;
		consumec(code, '(');

		ast = NULL;
		if(isalnum(code->curr) || (code->curr == ')'))
		{
			if(code->curr != ')')
				free(readIdentifier(code));

			if(code->curr == ',' || (lookahead(code, ")") && lookahead(code, "->")))
			{
				code->pos = start;
				code->curr = code->src[start];

				symbolScope_increase(code, 1, false);
				setSymbol(code, strdup("this"), 0);

				ast = talloc(ptrs_ast_t);
				ast->handler = ptrs_handle_function;
				ast->arg.function.isAnonymous = true;

				ptrs_function_t *func = &ast->arg.function.func;
				func->name = "(lambda expression)";
				func->argv = NULL;
				func->argc = parseArgumentDefinitionList(code, &func->args, NULL, &func->vararg);

				consume(code, "->");
				if(lookahead(code, "{"))
				{
					func->body = parseStmtList(code, '}');
					consumec(code, '}');
				}
				else
				{
					ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
					retStmt->handler = ptrs_handle_return;
					retStmt->arg.astval = parseExpression(code, true);
					func->body = retStmt;
				}

				symbolScope_decrease(code);
			}
			else
			{
				code->pos = start + 1;
				code->curr = code->src[start + 1];
			}
		}

		if(ast == NULL)
		{
			ast = parseExpression(code, true);
			consumec(code, ')');
		}
	}
	else
	{
		return NULL;
	}

	if(noSetHandler)
	{
		ast->setHandler = NULL;
		ast->addressHandler = NULL;
		ast->callHandler = NULL;
	}
	ast->codepos = pos;
	ast->code = code->src;
	ast->file = code->filename;

	ptrs_ast_t *old;
	do
	{
		old = ast;
		ast = parseUnaryExtension(code, ast, ignoreCalls, ignoreAlgo);
	} while(ast != old);

	return ast;
}

static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls, bool ignoreAlgo)
{
	char curr = code->curr;
	if(curr == '.' && code->src[code->pos + 1] != '.')
	{
		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->handler = ptrs_handle_member;
		member->setHandler = ptrs_handle_assign_member;
		member->addressHandler = ptrs_handle_addressof_member;
		member->callHandler = ptrs_handle_call_member;
		member->codepos = code->pos;
		member->code = code->src;
		member->file = code->filename;

		consumec(code, '.');
		char *name = readIdentifier(code);

		member->arg.member.base = ast;
		member->arg.member.name = name;
		member->arg.member.namelen = strlen(name);

		ast = member;
	}
	else if(!ignoreCalls && (curr == '(' || curr == '!'))
	{
		ptrs_ast_t *call;
		if(curr == '!')
		{
			int pos = code->pos;

			next(code);
			if(isalpha(code->curr))
			{
				call = talloc(ptrs_ast_t);
				call->arg.call.retType = readNativeType(code);

				if(call->arg.call.retType == NULL)
					unexpected(code, "return type");
			}
			else
			{
				code->pos = pos;
				code->curr = code->src[pos];
				return parseUnaryExtension(code, ast, true, ignoreAlgo);
			}
		}
		else
		{
			call = talloc(ptrs_ast_t);
			call->arg.call.retType = NULL;
		}

		consumec(code, '(');

		call->handler = ptrs_handle_call;
		call->setHandler = NULL;
		call->addressHandler = NULL;
		call->codepos = code->pos;
		call->code = code->src;
		call->file = code->filename;
		call->arg.call.value = ast;
		call->arg.call.arguments = parseExpressionList(code, ')');

		ast = call;
		consumec(code, ')');
	}
	else if(curr == '[')
	{
		bool insideIndex = code->insideIndex;
		code->insideIndex = true;

		ptrs_ast_t *indexExpr = talloc(ptrs_ast_t);
		indexExpr->codepos = code->pos;
		indexExpr->code = code->src;
		indexExpr->file = code->filename;

		consumec(code, '[');

		ptrs_ast_t *expr = parseExpression(code, true);

		if(lookahead(code, ".."))
		{
			indexExpr->handler = ptrs_handle_slice;
			indexExpr->arg.slice.base = ast;
			indexExpr->arg.slice.start = expr;
			indexExpr->arg.slice.end = parseExpression(code, true);
		}
		else
		{
			indexExpr->handler = ptrs_handle_index;
			indexExpr->setHandler = ptrs_handle_assign_index;
			indexExpr->addressHandler = ptrs_handle_addressof_index;
			indexExpr->callHandler = ptrs_handle_call_index;
			indexExpr->arg.binary.left = ast;
			indexExpr->arg.binary.right = expr;
		}

		ast = indexExpr;
		code->insideIndex = insideIndex;
		consumec(code, ']');
	}
	else if(lookahead(code, "->"))
	{
		ptrs_ast_t *dereference = talloc(ptrs_ast_t);
		dereference->handler = ptrs_handle_prefix_dereference;
		dereference->setHandler = ptrs_handle_assign_dereference;
		dereference->callHandler = NULL;
		dereference->codepos = code->pos - 2;
		dereference->code = code->src;
		dereference->file = code->filename;
		dereference->arg.astval = ast;

		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->handler = ptrs_handle_member;
		member->setHandler = ptrs_handle_assign_member;
		member->addressHandler = ptrs_handle_addressof_member;
		member->callHandler = ptrs_handle_call_member;
		member->codepos = code->pos;
		member->code = code->src;
		member->file = code->filename;

		member->arg.member.base = dereference;
		member->arg.member.name = readIdentifier(code);

		ast = member;
	}
	else
	{
		int pos = code->pos;
		ptrs_asthandler_t handler = readSuffixOperator(code, NULL);
		if(handler != NULL)
		{
			ptrs_ast_t *opAst = talloc(ptrs_ast_t);
			opAst->codepos = pos;
			opAst->code = code->src;
			opAst->file = code->filename;
			opAst->arg.astval = ast;
			opAst->handler = handler;
			opAst->setHandler = NULL;
			opAst->callHandler = NULL;
			return opAst;
		}
	}

	return ast;
}

static struct ptrs_astlist *parseExpressionList(code_t *code, char end)
{
	if(code->curr == end || code->curr == 0)
		return NULL;

	struct ptrs_astlist *curr = talloc(struct ptrs_astlist);
	struct ptrs_astlist *first = curr;

	for(;;)
	{
		curr->expand = false;
		curr->lazy = false;

		if(lookahead(code, "_"))
		{
			curr->entry = NULL;
		}
		else if(lookahead(code, "..."))
		{
			curr->expand = true;
			curr->entry = parseExpression(code, true);
			if(curr->entry->handler != ptrs_handle_identifier
				&& curr->entry->handler != ptrs_handle_constant)
				unexpectedm(code, NULL, "Array spreading can only be used on identifiers and constants");

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}
		else if(lookahead(code, "lazy"))
		{
			curr->lazy = true;
			curr->entry = parseExpression(code, true);

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}
		else
		{
			curr->entry = parseExpression(code, true);

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}

		if(!lookahead(code, ","))
			break;

		struct ptrs_astlist *next = talloc(struct ptrs_astlist);
		curr->next = next;
		curr = next;
	}

	curr->next = NULL;
	return first;
}

static void parseImport(code_t *code, ptrs_ast_t *stmt)
{
	stmt->handler = ptrs_handle_import;
	stmt->arg.import.imports = NULL;
	stmt->arg.import.wildcardCount = 0;

	for(;;)
	{
		char *name = readIdentifier(code);
		if(code->curr == '*')
		{
			next(code);

			struct wildcardsymbol *curr = talloc(struct wildcardsymbol);
			curr->next = code->symbols->wildcards;
			code->symbols->wildcards = curr;

			curr->importStmt = stmt;
			curr->start = name;
			curr->startLen = strlen(name);
		}
		else if(code->curr == ':')
		{
			next(code);

			struct ptrs_importlist *curr = talloc(struct ptrs_importlist);
			curr->name = name;
			curr->next = stmt->arg.import.imports;
			stmt->arg.import.imports = curr;

			struct symbollist *symbol = addSpecialSymbol(code, strdup(name), PTRS_SYMBOL_TYPED);
			symbol->arg.typed.location = &curr->location;
			symbol->arg.typed.type = readNativeType(code);

			if(symbol->arg.typed.type == NULL)
				unexpected(code, "Native type name");
		}
		else
		{
			struct ptrs_importlist *curr = talloc(struct ptrs_importlist);
			curr->next = stmt->arg.import.imports;
			stmt->arg.import.imports = curr;

			curr->name = name;
			if(lookahead(code, "as"))
				addSymbol(code, readIdentifier(code), &curr->location);
			else
				addSymbol(code, strdup(curr->name), &curr->location);
		}

		if(code->curr == ';')
		{
			stmt->arg.import.from = NULL;
			break;
		}
		else if(lookahead(code, "from"))
		{
			stmt->arg.import.from = parseExpression(code, true);
			consumec(code, ';');
			break;
		}
		else
		{
			consumec(code, ',');
		}
	}
}

static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt)
{
	consumec(code, '(');
	stmt->arg.switchcase.condition = parseExpression(code, true);
	consumec(code, ')');
	consumec(code, '{');

	bool isEnd = false;
	char isDefault = 0; //0 = nothing, 1 = default's head, 2 = default's body, 3 = done

	stmt->handler = ptrs_handle_switch;
	stmt->arg.switchcase.defaultCase = NULL;

	struct ptrs_ast_case first;
	first.next = NULL;

	struct ptrs_astlist *body = NULL;
	struct ptrs_astlist *bodyStart = NULL;
	struct ptrs_ast_case *cases = &first;
	for(;;)
	{
		if(!isDefault && lookahead(code, "default"))
			isDefault = 1;
		else if(code->curr == '}')
			isEnd = true;

		if(isDefault == 1 || isEnd || lookahead(code, "case"))
		{
			ptrs_ast_t *expr;
			if(bodyStart != NULL)
			{
				expr = talloc(ptrs_ast_t);
				expr->handler = ptrs_handle_body;
				expr->arg.astlist = bodyStart;
			}
			else
			{
				expr = NULL;
			}

			if(isDefault == 2)
			{
				stmt->arg.switchcase.defaultCase = expr;
				isDefault = 3;
			}

			if(body != NULL)
				body->next = NULL;

			while(cases->next != NULL)
			{
				cases = cases->next;
				cases->body = expr;
			}
			body = NULL;

			if(isEnd)
			{
				consumec(code, '}');
				break;
			}

			if(isDefault == 1)
			{
				isDefault = 2;
			}
			else
			{
				struct ptrs_ast_case *currCase = cases;
				for(;;)
				{
					ptrs_ast_t *expr = parseExpression(code, true);
					if(expr->handler != ptrs_handle_constant || expr->arg.constval.meta.type != PTRS_TYPE_INT)
						unexpected(code, "integer constant");

					currCase->next = talloc(struct ptrs_ast_case);
					currCase = currCase->next;

					currCase->value = expr->arg.constval.value.intval;
					free(expr);
					currCase->next = NULL;

					if(code->curr == ':')
						break;
					consumec(code, ',');

					currCase->next = NULL;
				}
			}
			consumec(code, ':');
		}
		else
		{
			if(body == NULL)
			{
				bodyStart = body = talloc(struct ptrs_astlist);
			}
			else
			{
				body->next = talloc(struct ptrs_astlist);
				body = body->next;
			}
			body->entry = parseStatement(code);
		}
	}

	stmt->arg.switchcase.cases = first.next;
}



static ptrs_ast_t *parseNew(code_t *code, bool onStack)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);

	if(lookahead(code, "array"))
	{
		ast->arg.define.location.val = NULL;
		ast->arg.define.location.meta = NULL;
		ast->arg.define.initExpr = NULL;
		ast->arg.define.isInitExpr = true;
		ast->arg.define.onStack = onStack;

		if(lookahead(code, "["))
		{
			ast->handler = ptrs_handle_vararray;
			ast->arg.define.location.constType = PTRS_TYPE_POINTER;
			ast->arg.define.value = parseExpression(code, false);
			consumec(code, ']');

			if(lookahead(code, "["))
			{
				ast->arg.define.isInitExpr = false;
				ast->arg.define.initVal = parseExpressionList(code, ']');
				consumec(code, ']');
			}
			else if(ast->arg.define.value == NULL)
			{
				unexpected(code, "Array initializer for implicitly sized var-array");
			}
		}
		else if(lookahead(code, "{"))
		{
			ast->handler = ptrs_handle_array;
			ast->arg.define.location.constType = PTRS_TYPE_NATIVE;
			ast->arg.define.value = parseExpression(code, false);
			consumec(code, '}');

			if(lookahead(code, "{"))
			{
				ast->arg.define.isInitExpr = false;
				ast->arg.define.initVal = parseExpressionList(code, '}');
				consumec(code, '}');
			}
			else if(ast->arg.define.value == NULL)
			{
				unexpected(code, "Array initializer for implicitly sized array");
			}
		}
		else
		{
			unexpected(code, "[ or {");
		}
	}
	else
	{
		ast->handler = ptrs_handle_new;
		ast->arg.define.location.constType = PTRS_TYPE_STRUCT;
		ast->arg.newexpr.onStack = onStack;
		ast->arg.newexpr.value = parseUnaryExpr(code, true, false);

		consumec(code, '(');
		ast->arg.newexpr.arguments = parseExpressionList(code, ')');
		consumec(code, ')');
	}

	return ast;
}

static uint8_t knownPrimes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
struct ptrs_structParseList
{
	struct ptrs_structmember member;
	struct ptrs_structParseList *next;
};
static void createStructHashmap(code_t *code, ptrs_struct_t *struc, struct ptrs_structParseList *curr, int count)
{
	if(count == 0)
	{
		struc->memberCount = 0;
		struc->member = NULL;
		return;
	}
	else if(count >= 97) //TODO search for primes instead?
	{
		unexpectedm(code, NULL, "Structs cannot have more than 96 fields");
	}

	for(int i = 0; i < sizeof(knownPrimes); i++)
	{
		if(knownPrimes[i] > count)
		{
			count = knownPrimes[i];
			break;
		}
	}

	struct ptrs_structmember *member = malloc(count * sizeof(struct ptrs_structmember));
	for(int i = 0; i < count; i++)
		member[i].name = NULL;

	while(curr != NULL)
	{
		int i = ptrs_struct_hashName(curr->member.name) % count;
		while(member[i].name != NULL)
		{
			//printf("collision %s >< %s\n", curr->member.name, member[i].name);
			i = (i + 1) % count;
		}

		memcpy(&member[i], &curr->member, sizeof(struct ptrs_structmember));
		curr = curr->next;
	}

	struc->member = member;
	struc->memberCount = count;
}

static void parseMap(code_t *code, ptrs_ast_t *ast)
{
	ptrs_ast_t *structExpr = talloc(ptrs_ast_t);
	ptrs_struct_t *struc = &structExpr->arg.structval;

	ast->handler = ptrs_handle_new;
	ast->arg.newexpr.value = structExpr;
	ast->arg.newexpr.arguments = NULL;

	structExpr->handler = ptrs_handle_struct;

	struc->location = NULL;
	struc->size = 0;
	struc->name = "(map)";
	struc->overloads = NULL;
	struc->data = NULL;
	struc->staticData = NULL;

	consumec(code, '{');
	symbolScope_increase(code, 0, false);

	int count = 0;
	struct ptrs_structParseList *start = NULL;
	struct ptrs_structParseList *curr = NULL;
	for(;;)
	{
		if(start == NULL)
		{
			curr = alloca(sizeof(struct ptrs_structParseList));
			start = curr;
		}
		else
		{
			curr->next = alloca(sizeof(struct ptrs_structParseList));
			curr = curr->next;
		}
		count++;

		if(lookahead(code, "\""))
			curr->member.name = readString(code, NULL, NULL, NULL);
		else
			curr->member.name = readIdentifier(code);

		curr->member.namelen = strlen(curr->member.name);
		curr->member.protection = 0;
		curr->member.isStatic = 0;

		if(code->curr == '(')
		{
			curr->member.type = PTRS_STRUCTMEMBER_FUNCTION;
			symbolScope_increase(code, 1, false);
			setSymbol(code, strdup("this"), 0);

			ptrs_function_t *func = curr->member.value.function = talloc(ptrs_function_t);
			func->name = "(map function member)";
			func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);

			consume(code, "->");
			if(lookahead(code, "{"))
			{
				func->body = parseStmtList(code, '}');
				consumec(code, '}');
			}
			else
			{
				ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
				retStmt->handler = ptrs_handle_return;
				retStmt->arg.astval = parseExpression(code, true);
				func->body = retStmt;
			}
		}
		else
		{
			consumec(code, ':');

			curr->member.type = PTRS_STRUCTMEMBER_VAR;
			curr->member.offset = struc->size;
			struc->size += sizeof(ptrs_var_t);
			curr->member.value.startval = parseExpression(code, true);
		}


		if(code->curr == '}')
			break;
		consumec(code, ',');
	}

	curr->next = NULL;
	createStructHashmap(code, struc, start, count);

	symbolScope_decrease(code);

	consumec(code, '}');
}

static void parseStruct(code_t *code, ptrs_struct_t *struc)
{
	char *name;
	char *structName = readIdentifier(code);
	int structNameLen = strlen(structName);
	int staticMemSize = 0;

	ptrs_ast_t *oldAst;
	if(ptrs_ast_getSymbol(code->symbols, structName, &oldAst) == 0)
	{
		if(oldAst->handler != ptrs_handle_identifier)
			PTRS_HANDLE_ASTERROR(NULL, "Cannot redefine special symbol %s as a struct", structName);

		struc->location = oldAst->arg.varval;
		free(oldAst);
	}
	else
	{
		struc->location = talloc(ptrs_jit_var_t);
		addSymbol(code, strdup(structName), struc->location);
	}

	struc->name = structName;
	struc->overloads = NULL;
	struc->size = 0;
	struc->data = NULL;
	consumec(code, '{');

	symbolScope_increase(code, 0, true);
	int memberCount = 0;
	struct ptrs_structParseList *start = NULL;
	struct ptrs_structParseList *currList = NULL;
	struct ptrs_structmember *curr = NULL;
	struct ptrs_structmember *old;
	while(code->curr != '}')
	{
		bool isStatic;
		if(lookahead(code, "static"))
			isStatic = true;
		else
			isStatic = false;

		if(lookahead(code, "operator"))
		{
			symbolScope_increase(code, 1, false);
			setSymbol(code, strdup("this"), 0);

			ptrs_jit_var_t *oldYield = code->yield;
			const char *nameFormat = NULL;
			const char *opLabel = NULL;
			char *otherName = NULL;
			ptrs_function_t *func = talloc(ptrs_function_t);
			func->argc = 0;
			func->argv = NULL;
			func->vararg.val = NULL;
			func->vararg.meta = NULL;

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->isStatic = isStatic;

			bool isAddressOf = false;
			if(lookahead(code, "&"))
			{
				consume(code, "this");
				isAddressOf = true;
			}

			if(isAddressOf || lookahead(code, "this"))
			{
				char curr = code->curr;
				if(curr == '.' || curr == '[')
				{
					next(code);
					otherName = readIdentifier(code);
					func->argc = 1;

					if(curr == '.')
					{
						nameFormat = "%1$s.op this.%3$s%2$s";
					}
					else
					{
						nameFormat = "%1$s.op this[%3$s]%2$s";
						consumec(code, ']');
					}

					if(isAddressOf)
					{
						func->args = talloc(ptrs_jit_var_t);
						addSymbol(code, otherName, func->args);

						nameFormat = curr == '.' ? "%1$s.op &this.%3$s" : "%1$s.op &this[%3$s]";
						overload->op = curr == '.' ? ptrs_handle_addressof_member : ptrs_handle_addressof_index;
					}
					else if(lookahead(code, "="))
					{
						func->argc = 2;
						func->args = malloc(sizeof(ptrs_jit_var_t) * 2);
						addSymbol(code, otherName, &func->args[0]);
						addSymbol(code, readIdentifier(code), &func->args[1]);

						opLabel = " = value";
						overload->op = curr == '.' ? ptrs_handle_assign_member : ptrs_handle_assign_index;
					}
					else if(code->curr == '(')
					{
						func->argc = parseArgumentDefinitionList(code,
							&func->args, &func->argv, &func->vararg);
						func->argc++;

						opLabel = "()";
						overload->op = curr == '.' ? ptrs_handle_call_member : ptrs_handle_call_index;


						ptrs_jit_var_t *newArgs = malloc(sizeof(ptrs_jit_var_t) * func->argc);
						addSymbol(code, otherName, &newArgs[0]);

						memcpy(newArgs + 1, func->args, sizeof(ptrs_jit_var_t) * (func->argc - 1));
						free(func->args);
						func->args = newArgs;


						if(func->argv != NULL)
						{
							ptrs_ast_t **newArgv = malloc(sizeof(ptrs_jit_var_t) * func->argc);
							newArgv[0] = NULL;

							memcpy(newArgv + 1, func->argv, sizeof(ptrs_jit_var_t) * (func->argc - 1));
							free(func->argv);
							func->argv = newArgv;
						}
					}
					else
					{
						func->args = talloc(ptrs_jit_var_t);
						addSymbol(code, otherName, func->args);

						opLabel = "";
						overload->op = curr == '.' ? ptrs_handle_member : ptrs_handle_index;
					}
				}
				else if(curr == '(')
				{
					func->argc = parseArgumentDefinitionList(code,
						&func->args, &func->argv, &func->vararg);
					overload->op = ptrs_handle_call;

					opLabel = "()";
					nameFormat = "%1$s.op this()";
				}
				else
				{
					unexpected(code, "missing member overload");
				}
			}
			else if(lookahead(code, "sizeof"))
			{
				consume(code, "this");

				nameFormat = "%1$s.op sizeof this";
				overload->op = ptrs_handle_prefix_sizeof;

				func->argc = 0;
				func->args = NULL;
			}
			else if(lookahead(code, "foreach"))
			{
				consume(code, "in");
				consume(code, "this");

				nameFormat = "%1$s.op foreach in this";
				overload->op = ptrs_handle_forin;

				code->yield = talloc(ptrs_jit_var_t);

				func->argc = 1;
				func->args = code->yield;
			}
			else if(lookahead(code, "cast"))
			{
				consumec(code, '<');

				if(lookahead(code, "string"))
				{
					otherName = NULL;
					nameFormat = "%1$s.op cast<string>this";
					overload->op = ptrs_handle_tostring;

					func->argc = 0;
					func->args = NULL;
				}
				else
				{
					otherName = readIdentifier(code);
					nameFormat = "%1$s.op cast<%3$s>this";
					overload->op = ptrs_handle_cast_builtin;

					func->argc = 1;
					func->args = talloc(ptrs_jit_var_t);
					addSymbol(code, otherName, func->args);
				}

				consumec(code, '>');
				consume(code, "this");
			}

			if(overload->op == NULL)
				unexpected(code, "Operator");

			func->name = malloc(snprintf(NULL, 0, nameFormat, structName, opLabel, otherName) + 1);
			sprintf(func->name, nameFormat, structName, opLabel, otherName);

			func->body = parseBody(code, false, true);
			code->yield = oldYield;

			overload->handler = func;
			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}
		else if(lookahead(code, "constructor"))
		{
			name = malloc(structNameLen + strlen("constructor") + 2);
			sprintf(name, "%s.constructor", structName);

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->op = ptrs_handle_new;
			overload->handler = parseFunction(code, name);
			overload->isStatic = isStatic;

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}
		else if(lookahead(code, "destructor"))
		{
			name = malloc(structNameLen + strlen("destructor") + 2);
			sprintf(name, "%s.destructor", structName);

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->op = ptrs_handle_delete;
			overload->handler = parseFunction(code, name);
			overload->isStatic = isStatic;

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}

		old = curr;
		if(start == NULL)
		{
			currList = alloca(sizeof(struct ptrs_structParseList));
			start = currList;
		}
		else
		{
			currList->next = alloca(sizeof(struct ptrs_structParseList));
			currList = currList->next;
		}
		curr = &currList->member;
		memberCount++;

		if(lookahead(code, "private"))
			curr->protection = 2;
		else if(lookahead(code, "internal"))
			curr->protection = 1;
		else if(lookahead(code, "public"))
			curr->protection = 0;
		else
			curr->protection = 0;

		int currSize;
		if(isStatic || lookahead(code, "static"))
		{
			currSize = staticMemSize;
			curr->isStatic = true;
		}
		else
		{
			currSize = struc->size;
			curr->isStatic = false;
		}

		uint8_t isProperty;
		if(lookahead(code, "get"))
			isProperty = 1;
		else if(lookahead(code, "set"))
			isProperty = 2;
		else
			isProperty = 0;

		name = curr->name = readIdentifier(code);
		curr->namelen = strlen(name);

		if(isProperty > 0)
		{
			symbolScope_increase(code, 1, false);
			setSymbol(code, strdup("this"), 0);

			ptrs_function_t *func = talloc(ptrs_function_t);
			func->name = malloc(structNameLen + strlen(name) + 6);
			func->vararg.val = NULL;
			func->vararg.meta = NULL;

			if(isProperty == 1)
			{
				curr->type = PTRS_STRUCTMEMBER_GETTER;

				func->argc = 0;
				func->args = NULL;
				func->argv = NULL;
				sprintf(func->name, "%s.get %s", structName, name);
			}
			else
			{
				curr->type = PTRS_STRUCTMEMBER_SETTER;

				func->argc = 1;
				func->args = talloc(ptrs_jit_var_t);
				addSymbol(code, strdup("value"), func->args);
				sprintf(func->name, "%s.set %s", structName, name);
			}

			func->body = parseBody(code, false, true);
			curr->value.function = func;
		}
		else if(code->curr == '(')
		{
			char *funcName = malloc(structNameLen + strlen(name) + 2);
			sprintf(funcName, "%s.%s", structName, name);

			curr->type = PTRS_STRUCTMEMBER_FUNCTION;
			curr->value.function = parseFunction(code, funcName);
		}
		else if(code->curr == '[')
		{
			curr->type = PTRS_STRUCTMEMBER_VARARRAY;
			consumec(code, '[');
			ptrs_ast_t *ast = parseExpression(code, true);
			consumec(code, ']');

			if(lookahead(code, "="))
			{
				consumec(code, '[');
				curr->value.arrayInit = parseExpressionList(code, ']');
				consumec(code, ']');
			}
			else
			{
				curr->value.arrayInit = NULL;
			}

			consumec(code, ';');

			if(ast->handler != ptrs_handle_constant)
				PTRS_HANDLE_ASTERROR(ast, "Struct array member size must be a constant");

			curr->value.size = ast->arg.constval.value.intval * sizeof(ptrs_var_t);
			curr->offset = currSize;
			currSize += curr->value.size;
			free(ast);
		}
		else if(code->curr == '{')
		{
			curr->type = PTRS_STRUCTMEMBER_ARRAY;
			consumec(code, '{');
			ptrs_ast_t *ast = parseExpression(code, true);
			consumec(code, '}');

			if(lookahead(code, "="))
			{
				consumec(code, '{');
				curr->value.arrayInit = parseExpressionList(code, '}');
				consumec(code, '}');
			}
			else
			{
				curr->value.arrayInit = NULL;
			}

			consumec(code, ';');

			if(ast->handler != ptrs_handle_constant)
				PTRS_HANDLE_ASTERROR(ast, "Struct array member size must be a constant");

			curr->value.size = ast->arg.constval.value.intval;

			if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_TYPED)
				curr->offset = old->offset + old->value.type->size;
			else if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_ARRAY)
				curr->offset = old->offset + old->value.size;
			else
				curr->offset = currSize;

			currSize = ((curr->offset + curr->value.size - 1) & ~7) + 8;
			free(ast);
		}
		else if(code->curr == ':')
		{
			consumec(code, ':');
			ptrs_nativetype_info_t *type = readNativeType(code);
			consumec(code, ';');

			curr->type = PTRS_STRUCTMEMBER_TYPED;
			curr->value.type = type;

			if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_TYPED)
			{
				if(old->value.type->size < type->size)
					curr->offset = (old->offset & ~(type->size - 1)) + type->size;
				else
					curr->offset = old->offset + old->value.type->size;
			}
			else if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_ARRAY)
			{
				curr->offset = (old->offset & ~(type->size - 1)) + old->value.size;
			}
			else
			{
				curr->offset = currSize;
			}
			currSize = (curr->offset & ~7) + 8;
		}
		else
		{
			curr->type = PTRS_STRUCTMEMBER_VAR;
			curr->offset = currSize;
			currSize += sizeof(ptrs_var_t);

			if(lookahead(code, "="))
				curr->value.startval = parseExpression(code, true);
			else
				curr->value.startval = NULL;

			consumec(code, ';');
		}

		if(curr->isStatic)
			staticMemSize = currSize;
		else
			struc->size = currSize;
	}

	if(staticMemSize != 0)
		struc->staticData = malloc(staticMemSize);
	else
		struc->staticData = NULL;

	if(memberCount != 0)
		currList->next = NULL;
	createStructHashmap(code, struc, start, memberCount);

	symbolScope_decrease(code);
	consumec(code, '}');
	consumec(code, ';');
}

const char * const typeNames[] = {
	[PTRS_TYPE_UNDEFINED] = "undefined",
	[PTRS_TYPE_INT] = "int",
	[PTRS_TYPE_FLOAT] = "float",
	[PTRS_TYPE_NATIVE] = "native",
	[PTRS_TYPE_POINTER] = "pointer",
	[PTRS_TYPE_STRUCT] = "struct",
};
static int typeNameCount = sizeof(typeNames) / sizeof(const char *);

static ptrs_vartype_t readTypeName(code_t *code)
{
	for(int i = 0; i < typeNameCount; i++)
	{
		if(lookahead(code, typeNames[i]))
		{
			return i;
		}
	}

	return PTRS_TYPE_STRUCT + 1;
}

ptrs_nativetype_info_t ptrs_nativeTypes[] = {
	{"char", sizeof(signed char), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"short", sizeof(short), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"int", sizeof(int), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"long", sizeof(long), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"longlong", sizeof(long long), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},

	{"uchar", sizeof(unsigned char), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"ushort", sizeof(unsigned short), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"uint", sizeof(unsigned int), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"ulong", sizeof(unsigned long), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"ulonglong", sizeof(unsigned long long), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},

	{"i8", sizeof(int8_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"i16", sizeof(int16_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"i32", sizeof(int32_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"i64", sizeof(int64_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},

	{"u8", sizeof(uint8_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"u16", sizeof(uint16_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"u32", sizeof(uint32_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"u64", sizeof(uint64_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},

	{"single", sizeof(float), NULL, PTRS_TYPE_FLOAT, ptrs_handle_native_getFloat, ptrs_handle_native_setFloat},
	{"double", sizeof(double), NULL, PTRS_TYPE_FLOAT, ptrs_handle_native_getFloat, ptrs_handle_native_setFloat},

	{"native", sizeof(char *), NULL, PTRS_TYPE_NATIVE, ptrs_handle_native_getNative, ptrs_handle_native_setPointer},
	{"pointer", sizeof(ptrs_var_t *), NULL, PTRS_TYPE_POINTER, ptrs_handle_native_getPointer, ptrs_handle_native_setPointer},

	{"bool", sizeof(bool), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"ssize", sizeof(ssize_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"size", sizeof(size_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"intptr", sizeof(uintptr_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
	{"uintptr", sizeof(intptr_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getUInt, ptrs_handle_native_setUInt},
	{"ptrdiff", sizeof(ptrdiff_t), NULL, PTRS_TYPE_INT, ptrs_handle_native_getInt, ptrs_handle_native_setInt},
};
int ptrs_nativeTypeCount = sizeof(ptrs_nativeTypes) / sizeof(ptrs_nativetype_info_t);

static ptrs_nativetype_info_t *readNativeType(code_t *code)
{
	if(ptrs_nativeTypes[0].jitType == NULL)
	{
		jit_type_t types[] = {
			jit_type_sys_char,
			jit_type_sys_short,
			jit_type_sys_int,
			jit_type_sys_long,
			jit_type_sys_longlong,

			jit_type_sys_uchar,
			jit_type_sys_ushort,
			jit_type_sys_uint,
			jit_type_sys_ulong,
			jit_type_sys_ulonglong,

			jit_type_sbyte,
			jit_type_short,
			jit_type_int,
			jit_type_long,

			jit_type_ubyte,
			jit_type_ushort,
			jit_type_uint,
			jit_type_ulong,

			jit_type_float32,
			jit_type_float64,

			jit_type_void_ptr,
			jit_type_void_ptr,

			jit_type_sys_bool,
			jit_type_nint, //ssize
			jit_type_nuint, //size
			jit_type_nint,
			jit_type_nuint,
			jit_type_nint, //ptrdiff
		};

		for(int i = 0; i < ptrs_nativeTypeCount; i++)
			ptrs_nativeTypes[i].jitType = types[i];
	}

	for(int i = 0; i < ptrs_nativeTypeCount; i++)
	{
		if(lookahead(code, ptrs_nativeTypes[i].name))
			return &ptrs_nativeTypes[i];
	}
	return NULL;
}

static ptrs_asthandler_t readOperatorFrom(code_t *code, const char **label, struct opinfo *ops, int opCount)
{
	for(int i = 0; i < opCount; i++)
	{
		if(lookahead(code, ops[i].op))
		{
			if(label != NULL)
				*label = ops[i].op;
			return ops[i].handler;
		}
	}
	return NULL;
}
static ptrs_asthandler_t readPrefixOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, prefixOps, prefixOpCount);
}
static ptrs_asthandler_t readSuffixOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, suffixedOps, suffixedOpCount);
}
/*static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, binaryOps, binaryOpCount);
}*/

static char *readIdentifier(code_t *code)
{
	char val[128];
	int i = 0;

	while(isalpha(code->curr) || isdigit(code->curr) ||  code->curr == '_')
	{
		val[i] = code->curr;
		i++;
		rawnext(code);
	}

	if(i == 0)
		unexpected(code, "Identifier");

	skipSpaces(code);

	val[i] = 0;
	char *_val = malloc(i + 1);
	strcpy(_val, val);

	return _val;
}

static char *readString(code_t *code, int *length, struct ptrs_stringformat **insertions, int *insertionCount)
{
	int buffSize = 1024;
	int i = 0;
	uint8_t insertionMode = 0; //0 = no insertions or % yet, 1 = no insertions but had %, 2 = had insertion

	if(insertionCount != NULL)
		*insertionCount = 0;

	char *buff = malloc(buffSize);
	struct ptrs_stringformat *curr = NULL;

	for(;;)
	{
		while(code->curr != '"')
		{
			if(code->curr == '\\')
			{
				rawnext(code);
			 	buff[i++] = readEscapeSequence(code);
				rawnext(code);
			}
			else if(code->curr == '%')
			{
				rawnext(code);
				if(insertionMode < 2)
				{
					insertionMode = 1;
					buff[i++] = '%';
				}
				else
				{
					buff[i++] = '%';
					buff[i++] = '%';
				}
			}
			else if(insertions != NULL && code->curr == '$')
			{
				rawnext(code);

				if(insertionCount != NULL)
					(*insertionCount)++;

				if(curr == NULL)
				{
					if(insertionMode == 1)
						unexpectedm(code, NULL, "String insertions cannot come after % in a string");
					insertionMode = 2;

					curr = talloc(struct ptrs_stringformat);
					*insertions = curr;
				}
				else
				{
					curr->next = talloc(struct ptrs_stringformat);
					curr = curr->next;
				}

				if(code->curr == '%')
				{
					while(code->curr != '{')
					{
						buff[i++] = code->curr;
						rawnext(code);
					}
					curr->convert = false;
				}
				else
				{
					buff[i++] = '%';
					buff[i++] = 's';
					curr->convert = true;
				}

				if(code->curr == '{')
				{
					rawnext(code);
					curr->entry = parseExpression(code, true);

					if(code->curr != '}')
						unexpected(code, "}");
					rawnext(code);
				}
				else
				{
					int j;
					char name[128];
					for(j = 0; j < 127 && (isalnum(code->curr) || code->curr == '_'); j++)
					{
						name[j] = code->curr;
						rawnext(code);
					}
					name[j] = 0;

					curr->entry = getSymbol(code, name);
				}
			}
			else
			{
				buff[i++] = code->curr;
				rawnext(code);
			}

			if(i > buffSize - 3)
			{
				buffSize *= 2;
				buff = realloc(buff, buffSize);
			}
		}

		next(code);
		if(code->curr == '"')
			rawnext(code);
		else
			break;
	}

	if(curr != NULL)
		curr->next = NULL;

	buff[i++] = 0;
	if(length != NULL)
		*length = i;

	return realloc(buff, i);
}

static char readEscapeSequence(code_t *code)
{
	switch(code->curr)
	{
		case 'a':
			return '\a';
		case 'b':
			return '\b';
		case 'f':
			return '\f';
		case 'n':
			return '\n';
		case 'r':
			return '\r';
		case 't':
			return '\t';
		case 'v':
			return '\v';
		case '\\':
			return '\\';
		case '\'':
			return '\'';
		case '"':
			return '"';
		case '?':
			return '\?';
		case '$':
			return '$';
		case 'x':
			rawnext(code);
			char val = readInt(code, 16);
			code->pos--;
			return val;
		default:
			if(isdigit(code->curr))
			{
				char val = readInt(code, 8);
				code->pos--;
				return val;
			}
	}
	char msg[] = "Unknown escape sequence \\X";
	msg[25] = code->curr;
	unexpectedm(code, NULL, msg);
	return 0;
}

static int64_t readInt(code_t *code, int base)
{
	char *start = &code->src[code->pos];
	char *end;

	int64_t val = strtol(start, &end, base);
	code->pos += end - start - 1;
	next(code);

	return val;
}
static double readDouble(code_t *code)
{
	char *start = &code->src[code->pos];
	char *end;

	double val = strtod(start, &end);
	code->pos += end - start - 1;
	next(code);

	return val;
}

static void setSymbol(code_t *code, char *text, unsigned offset)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	//TODO remove
}

static void addSymbol(code_t *code, char *symbol, ptrs_jit_var_t *location)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->type = PTRS_SYMBOL_DEFAULT;
	entry->text = symbol;
	entry->arg.location = location;
	entry->next = curr->current;
	curr->current = entry;
}

static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, ptrs_symboltype_t type) //0x656590
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->text = symbol;
	entry->type = type;
	entry->next = curr->current;
	curr->current = entry;

	return entry;
}

static ptrs_ast_t *getSymbolFromWildcard(code_t *code, char *text)
{
	unsigned level = 0;
	ptrs_symboltable_t *symbols = code->symbols;
	while(symbols != NULL)
	{
		struct wildcardsymbol *curr = symbols->wildcards;
		while(curr != NULL)
		{
			if(strncmp(curr->start, text, curr->startLen) == 0)
			{
				struct ptrs_ast_import *stmt = &curr->importStmt->arg.import;
				struct ptrs_importlist *import = talloc(struct ptrs_importlist);
				import->next = stmt->imports;
				stmt->imports = import;

				import->wildcardIndex = stmt->wildcardCount++;
				import->name = strdup(text);

				ptrs_ast_t *ast = talloc(ptrs_ast_t);
				ast->handler = ptrs_handle_wildcardsymbol;
				ast->setHandler = NULL;
				ast->callHandler = NULL;
				ast->addressHandler = NULL;
				ast->arg.wildcard.location = &stmt->wildcards;
				ast->arg.wildcard.index = import->wildcardIndex;

				struct symbollist *entry = talloc(struct symbollist);
				entry->text = strdup(text);
				entry->type = PTRS_SYMBOL_WILDCARD;
				entry->arg.wildcard.location = &stmt->wildcards;
				entry->arg.wildcard.index = import->wildcardIndex;
				entry->next = symbols->current;
				symbols->current = entry;

				return ast;
			}

			curr = curr->next;
		}

		if(!symbols->isInline)
			level++;
		symbols = symbols->outer;
	}

	return NULL;
}

static ptrs_ast_t *getSymbol(code_t *code, char *text)
{
	ptrs_ast_t *ast = NULL;
	if(ptrs_ast_getSymbol(code->symbols, text, &ast) == 0)
		return ast;

	ast = getSymbolFromWildcard(code, text);
	if(ast != NULL)
		return ast;

	char buff[128];
	sprintf(buff, "Unknown identifier %s", text);
	unexpectedm(code, NULL, buff);
	return ast; //doh
}

static void symbolScope_increase(code_t *code, int buildInCount, bool isInline)
{
	ptrs_symboltable_t *new = talloc(ptrs_symboltable_t);
	new->isInline = isInline;
	new->outer = code->symbols;
	new->current = NULL;
	new->wildcards = NULL;

	code->symbols = new;
}

static void symbolScope_decrease(code_t *code)
{
	ptrs_symboltable_t *scope = code->symbols;
	code->symbols = scope->outer;

	struct symbollist *curr = scope->current;
	while(curr != NULL)
	{
		struct symbollist *old = curr;
		curr = curr->next;
		free(old->text);
		free(old);
	}

	struct wildcardsymbol *currw = scope->wildcards;
	while(curr != NULL)
	{
		struct wildcardsymbol *old = currw;
		currw = currw->next;
		free(old->start);
		free(old);
	}

	free(scope);
}

static bool lookahead(code_t *code, const char *str)
{
	int start = code->pos;
	while(*str)
	{
		if(code->curr != *str)
		{
			code->pos = start;
			code->curr = code->src[start];
			return false;
		}
		str++;
		rawnext(code);
	}

	str--;
	if((isalpha(*str) || *str == '_') && (isalpha(code->curr) || code->curr == '_'))
	{
		code->pos = start;
		code->curr = code->src[start];
		return false;
	}

	code->pos--;
	code->curr = code->src[start];
	next(code);
	return true;
}

static void consume(code_t *code, const char *str)
{
	while(*str)
	{
		if(code->curr != *str)
			unexpected(code, str);
		str++;
		next(code);
	}
}
static void consumec(code_t *code, char c)
{
	if(code->curr != c)
	{
		char expected[] = {c, 0};
		unexpected(code, expected);
	}
	next(code);
}
static void consumecm(code_t *code, char c, const char *msg)
{
	if(code->curr != c)
		unexpectedm(code, NULL, msg);
	next(code);
}

static bool skipSpaces(code_t *code)
{
	int pos = code->pos;
	char curr = code->src[pos];
	if(isspace(curr))
	{
		while(isspace(curr))
		{
			pos++;
			curr = code->src[pos];
		}

		code->pos = pos;
		code->curr = curr;
		return true;
	}

	return false;
}
static bool skipComments(code_t *code)
{
	int pos = code->pos;
	char curr = code->src[pos];

	if(curr == '/' && code->src[pos + 1] == '/')
	{
		while(curr != '\n')
		{
			pos++;
			curr = code->src[pos];
		}

		code->pos = pos + 1;
		return true;
	}
	else if(curr == '/' && code->src[pos + 1] == '*')
	{
		while(curr != '*' || code->src[pos + 1] != '/')
		{
			pos++;
			curr = code->src[pos];
		}

		code->pos = pos + 2;
		return true;
	}

	return false;
}
static void next(code_t *code)
{
	if(code->src[code->pos] == 0)
		unexpectedm(code, NULL, "Unexpected end of input");

	code->pos++;
	while(skipSpaces(code) || skipComments(code));
	code->curr = code->src[code->pos];
}
static void rawnext(code_t *code)
{
	if(code->src[code->pos] == 0)
		unexpectedm(code, NULL, "Unexpected end of input");

	code->pos++;
	code->curr = code->src[code->pos];
}

static void unexpectedm(code_t *code, const char *expected, const char *msg)
{
	ptrs_ast_t node;
	node.codepos = code->pos;
	node.code = code->src;
	node.file = code->filename;

	char actual[16] = {0};
	char *curr = &code->src[code->pos];
	bool startIsAlnum = isalnum(curr[0]);
	bool startIsPunct = ispunct(curr[0]);

	for(int i = 0; i < 16; i++)
	{
		if(startIsAlnum && isalnum(curr[i]))
			actual[i] = curr[i];
		else if(startIsPunct && ispunct(curr[i]))
			actual[i] = curr[i];
		else
			break;
	}

	if(msg == NULL)
		PTRS_HANDLE_ASTERROR(&node, "Expected %s got '%s'", expected, actual);
	else if(expected == NULL)
		PTRS_HANDLE_ASTERROR(&node, "%s", msg);
	else
		PTRS_HANDLE_ASTERROR(&node, "%s - Expected %s got '%s'", msg, expected, actual);

}
