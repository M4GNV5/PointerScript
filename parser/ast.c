#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include INTERPRETER_INCLUDE
#include "ast.h"

#define talloc(type) malloc(sizeof(type))

struct symbollist
{
	unsigned offset;
	char *text;
	struct symbollist *next;
};
struct ptrs_symboltable
{
	unsigned offset;
	struct symbollist *current;
	ptrs_symboltable_t *outer;
};
typedef struct code
{
	const char *filename;
	char *src;
	char curr;
	int pos;
	ptrs_symboltable_t *symbols;
} code_t;

static ptrs_ast_t *parseStmtList(code_t *code, char end);
static ptrs_ast_t *parseStatement(code_t *code);
static ptrs_ast_t *parseExpression(code_t *code);
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls);
static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls);
static struct ptrs_astlist *parseExpressionList(code_t *code, char end);
static void parseStruct(code_t *code, ptrs_struct_t *struc);

static ptrs_vartype_t readTypeName(code_t *code);
static ptrs_asthandler_t readPrefixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readSuffixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label);
static char *readIdentifier(code_t *code);
static char *readString(code_t *code, int *length);
static char readEscapeSequence(code_t *code);
static int64_t readInt(code_t *code, int base);
static double readDouble(code_t *code);

static void setSymbol(code_t *code, char *text, unsigned offset);
static ptrs_symbol_t addSymbol(code_t *code, char *symbol);
static ptrs_symbol_t getSymbol(code_t *code, char *symbol);
static void symbolScope_increase(code_t *code, int buildInCount);
static unsigned symbolScope_decrease(code_t *code);

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
	code.pos = -1;
	code.symbols = NULL;
	next(&code);

	symbolScope_increase(&code, 1);
	setSymbol(&code, strdup("arguments"), 0);

	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = PTRS_HANDLE_FILE;
	ast->arg.scopestatement.body = parseStmtList(&code, 0);
	ast->arg.scopestatement.stackOffset = code.symbols->offset;
	ast->file = filename;
	ast->code = src;
	ast->codepos = 0;

	if(symbols == NULL)
		symbolScope_decrease(&code);
	else
		*symbols = code.symbols;
	return ast;
}

int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_symbol_t *out)
{
	out->scope = 0;
	while(symbols != NULL)
	{
		struct symbollist *curr = symbols->current;
		while(curr != NULL)
		{
			if(strcmp(curr->text, text) == 0)
			{
				out->offset = curr->offset;
				return 0;
			}
			curr = curr->next;
		}

		symbols = symbols->outer;
		out->scope++;
	}
	return 1;
}

static ptrs_ast_t *parseStmtList(code_t *code, char end)
{
	ptrs_ast_t *elem = talloc(ptrs_ast_t);
	elem->handler = PTRS_HANDLE_BODY;
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

		if(code->curr == end || code->curr == 0)
			break;

		struct ptrs_astlist *next = talloc(struct ptrs_astlist);
		curr->next = next;
		curr = next;
	}

	curr->next = NULL;
	return elem;
}

struct argdeflist
{
	ptrs_symbol_t symbol;
	char *name;
	ptrs_ast_t *value;
	struct argdeflist *next;
};
static int parseArgumentDefinitionList(code_t *code, ptrs_symbol_t **args, ptrs_ast_t ***argv, ptrs_symbol_t *vararg)
{
	bool hasArgv = false;
	int argc = 0;
	consumecm(code, '(', "Expected ( as the beginning of an argument definition");

	if(vararg != NULL)
		vararg->scope = (unsigned)-1;

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
			ptrs_symbol_t curr = addSymbol(code, readIdentifier(code));

			if(vararg != NULL && lookahead(code, "..."))
			{
				*vararg = curr;
				consumecm(code, ')', "Vararg argument has to be the last argument");
				break;
			}

			list->next = talloc(struct argdeflist);
			list = list->next;
			list->symbol = curr;

			if(argv != NULL)
			{
				if(lookahead(code, "="))
				{
					list->value = parseExpression(code);
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

		*args = malloc(sizeof(ptrs_symbol_t) * argc);
		if(hasArgv)
			*argv = malloc(sizeof(ptrs_ast_t *) * argc);
		else if(argv != NULL)
			*argv = NULL;

		list = first.next;
		for(int i = 0; i < argc; i++)
		{
			(*args)[i] = list->symbol;
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

static ptrs_ast_t *parseBody(code_t *code, unsigned *stackOffset, bool allowStmt, bool isFunction)
{
	if(isFunction)
	{
		setSymbol(code, strdup("this"), 0);
	}
	else
	{
		symbolScope_increase(code, 0);
	}

	ptrs_ast_t *result = parseScopelessBody(code, allowStmt);

	*stackOffset = symbolScope_decrease(code);
	return result;
}

static ptrs_function_t *parseFunction(code_t *code, char *name)
{
	symbolScope_increase(code, 1);
	ptrs_function_t *func = talloc(ptrs_function_t);
	func->name = name;

	func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
	func->body = parseBody(code, &func->stackOffset, false, true);

	return func;
}

static ptrs_ast_t *parseStatement(code_t *code)
{
	ptrs_ast_t *stmt = talloc(ptrs_ast_t);
	stmt->codepos = code->pos;
	stmt->code = code->src;
	stmt->file = code->filename;

	if(lookahead(code, "var"))
	{
		stmt->handler = PTRS_HANDLE_DEFINE;
		stmt->arg.define.symbol = addSymbol(code, readIdentifier(code));

		if(lookahead(code, "["))
		{
			stmt->handler = PTRS_HANDLE_VARARRAY;
			stmt->arg.define.value = parseExpression(code);
			consumec(code, ']');

			if(lookahead(code, "="))
			{
				consumecm(code, '[', "Array initializer for array");
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
			stmt->handler = PTRS_HANDLE_ARRAY;
			stmt->arg.define.value = parseExpression(code);
			consumec(code, '}');

			if(lookahead(code, "="))
			{
				if(lookahead(code, "{"))
				{
					stmt->arg.define.initExpr = NULL;
					consumec(code, '{');
					stmt->arg.define.initVal = parseExpressionList(code, '}');
					consumec(code, '}');
				}
				else
				{
					stmt->arg.define.initVal = NULL;
					stmt->arg.define.initExpr = parseExpression(code);
				}
			}
			else if(stmt->arg.define.value == NULL)
			{
				unexpectedm(code, "Array or String initializer for implicitly sized byte array", NULL);
			}
			else
			{
				stmt->arg.define.initVal = NULL;
			}
		}
		else if(lookahead(code, ":"))
		{
			stmt->handler = PTRS_HANDLE_STRUCTVAR;
			stmt->arg.define.value = parseUnaryExpr(code, true);

			consumec(code, '(');
			stmt->arg.define.initVal = parseExpressionList(code, ')');
			consumec(code, ')');
		}
		else if(lookahead(code, "="))
		{
			stmt->arg.define.value = parseExpression(code);
		}
		else
		{
			stmt->arg.define.value = NULL;
		}

		consumec(code, ';');
	}
	else if(lookahead(code, "import"))
	{
		int pos = code->pos;
		int count = 1;
		for(;;)
		{
			if(lookahead(code, "from") || code->curr == ';')
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
				unexpected(code, "Identifier, from or ;");
			}
		}

		code->pos = pos;
		code->curr = code->src[pos];
		char **fields = malloc(sizeof(char *) * count);
		ptrs_symbol_t *symbols = malloc(sizeof(ptrs_symbol_t) * count);

		stmt->handler = PTRS_HANDLE_IMPORT;
		stmt->arg.import.count = count;
		stmt->arg.import.fields = fields;
		stmt->arg.import.symbols = symbols;

		for(int i = 0; i < count; i++)
		{
			char *name = readIdentifier(code);
			fields[i] = name;
			symbols[i] = addSymbol(code, strdup(name));

			if(i < count - 1)
				consumec(code, ',');
		}

		if(lookahead(code, "from"))
			stmt->arg.import.from = parseExpression(code);
		else
			stmt->arg.import.from = NULL;

		consumec(code, ';');
	}
	else if(lookahead(code, "return"))
	{
		stmt->handler = PTRS_HANDLE_RETURN;
		stmt->arg.astval = parseExpression(code);
		consumec(code, ';');
	}
	else if(lookahead(code, "break"))
	{
		stmt->handler = PTRS_HANDLE_BREAK;
		consumec(code, ';');
	}
	else if(lookahead(code, "continue"))
	{
		stmt->handler = PTRS_HANDLE_CONTINUE;
		consumec(code, ';');
	}
	else if(lookahead(code, "delete"))
	{
		stmt->handler = PTRS_HANDLE_DELETE;
		stmt->arg.astval = parseExpression(code);
		consumec(code, ';');
	}
	else if(lookahead(code, "throw"))
	{
		stmt->handler = PTRS_HANDLE_THROW;
		stmt->arg.astval = parseExpression(code);
		consumec(code, ';');
	}
	else if(lookahead(code, "try"))
	{
		stmt->handler = PTRS_HANDLE_TRYCATCH;
		stmt->arg.trycatch.tryBody = parseBody(code, &stmt->arg.trycatch.tryStackOffset, true, false);

		if(lookahead(code, "catch"))
		{
			symbolScope_increase(code, 0);
			stmt->arg.trycatch.argc = parseArgumentDefinitionList(code, &stmt->arg.trycatch.args, NULL, NULL);
			stmt->arg.trycatch.catchBody = parseScopelessBody(code, true);
			stmt->arg.trycatch.catchStackOffset = symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.catchBody = NULL;
		}
	}
	else if(code->curr == '{')
	{
		stmt->handler = PTRS_HANDLE_SCOPESTATEMENT;
		stmt->arg.scopestatement.body = parseBody(code, &stmt->arg.scopestatement.stackOffset, false, false);
	}
	else if(lookahead(code, "function"))
	{
		stmt->handler = PTRS_HANDLE_FUNCTION;
		stmt->arg.function.isAnonymous = false;

		char *name = readIdentifier(code);
		if(ptrs_ast_getSymbol(code->symbols, name, &stmt->arg.function.symbol) != 0)
			stmt->arg.function.symbol = addSymbol(code, strdup(name));
		stmt->arg.function.name = name;

		symbolScope_increase(code, 1);
		stmt->arg.function.argc = parseArgumentDefinitionList(code,
			&stmt->arg.function.args, &stmt->arg.function.argv, &stmt->arg.function.vararg);

		stmt->arg.function.body = parseBody(code, &stmt->arg.function.stackOffset, false, true);
	}
	else if(lookahead(code, "struct"))
	{
		stmt->handler = PTRS_HANDLE_STRUCT;
		parseStruct(code, &stmt->arg.structval);
	}
	else if(lookahead(code, "if"))
	{
		stmt->handler = PTRS_HANDLE_IF;

		consumec(code, '(');
		stmt->arg.ifelse.condition = parseExpression(code);
		consumec(code, ')');
		stmt->arg.ifelse.ifBody = parseBody(code, &stmt->arg.ifelse.ifStackOffset, true, false);

		if(lookahead(code, "else"))
			stmt->arg.ifelse.elseBody = parseBody(code, &stmt->arg.ifelse.elseStackOffset,true, false);
		else
			stmt->arg.ifelse.elseBody = NULL;
	}
	else if(lookahead(code, "while"))
	{
		stmt->handler = PTRS_HANDLE_WHILE;

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');
		stmt->arg.control.body = parseBody(code, &stmt->arg.control.stackOffset, true, false);
	}
	else if(lookahead(code, "do"))
	{
		symbolScope_increase(code, 0);
		stmt->handler = PTRS_HANDLE_DOWHILE;
		stmt->arg.control.body = parseScopelessBody(code, true);
		consume(code, "while");

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');
		consumec(code, ';');
		stmt->arg.control.stackOffset = symbolScope_decrease(code);
	}
	else if(lookahead(code, "for"))
	{
		consumec(code, '(');
		int pos = code->pos;

		symbolScope_increase(code, 0);

		if(lookahead(code, "var"))
			stmt->arg.forin.newvar = true;
		else
			stmt->arg.forin.newvar = false;

		if(isalpha(code->curr) || code->curr == '_')
		{
			char *name = readIdentifier(code);
			if(lookahead(code, "in"))
			{
				if(stmt->arg.forin.newvar)
					stmt->arg.forin.var = addSymbol(code, name);
				else
					stmt->arg.forin.var = getSymbol(code, name);

				stmt->arg.forin.value = parseExpression(code);
				stmt->handler = PTRS_HANDLE_FORIN;
				consumec(code, ')');

				stmt->arg.forin.body = parseScopelessBody(code, true);
				stmt->arg.forin.stackOffset = symbolScope_decrease(code);
				return stmt;
			}
			free(name);
		}

		code->pos = pos;
		code->curr = code->src[pos];
		stmt->handler = PTRS_HANDLE_FOR;

		stmt->arg.forstatement.init = parseStatement(code);
		stmt->arg.forstatement.condition = parseExpression(code);
		consumec(code, ';');
		stmt->arg.forstatement.step = parseExpression(code);
		consumec(code, ')');

		stmt->arg.forstatement.body = parseScopelessBody(code, true);
		stmt->arg.forstatement.stackOffset = symbolScope_decrease(code);
	}
	else
	{
		stmt->handler = PTRS_HANDLE_EXPRSTATEMENT;
		stmt->arg.astval = parseExpression(code);
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
	{">>", 9, false, PTRS_HANDLE_OP_SHR}, //shift right
	{"<<", 9, false, PTRS_HANDLE_OP_SHL}, //shift left

	// == needs to be before = or we will always lookahead assign
	{"==", 8, false, PTRS_HANDLE_OP_EQUAL},
	{"!=", 8, false, PTRS_HANDLE_OP_INEQUAL},
	{"<=", 8, false, PTRS_HANDLE_OP_LESSEQUAL},
	{">=", 8, false, PTRS_HANDLE_OP_GREATEREQUAL},
	{"<", 8, false, PTRS_HANDLE_OP_LESS},
	{">", 8, false, PTRS_HANDLE_OP_GREATER},

	{"=", 1, true, PTRS_HANDLE_OP_ASSIGN},
	{"+=", 1, true, PTRS_HANDLE_OP_ADDASSIGN},
	{"-=", 1, true, PTRS_HANDLE_OP_SUBASSIGN},
	{"*=", 1, true, PTRS_HANDLE_OP_MULASSIGN},
	{"/=", 1, true, PTRS_HANDLE_OP_DIVASSIGN},
	{"%=", 1, true, PTRS_HANDLE_OP_MODASSIGN},
	{">>=", 1, true, PTRS_HANDLE_OP_SHRASSIGN},
	{"<<=", 1, true, PTRS_HANDLE_OP_SHLASSIGN},
	{"&=", 1, true, PTRS_HANDLE_OP_ANDASSIGN},
	{"^=", 1, true, PTRS_HANDLE_OP_XORASSIGN},
	{"|=", 1, true, PTRS_HANDLE_OP_ORASSIGN},

	{"?", 2, true, PTRS_HANDLE_OP_TERNARY},
	{":", -1, true, PTRS_HANDLE_OP_TERNARY},

	{"instanceof", 2, false, PTRS_HANDLE_OP_INSTANCEOF},

	{"||", 3, false, PTRS_HANDLE_OP_LOGICOR},
	{"^^", 3, false, PTRS_HANDLE_OP_LOGICXOR},
	{"&&", 4, false, PTRS_HANDLE_OP_LOGICAND},

	{"|", 5, false, PTRS_HANDLE_OP_OR},
	{"^", 6, false, PTRS_HANDLE_OP_XOR},
	{"&", 7, false, PTRS_HANDLE_OP_AND},

	{"+", 10, false, PTRS_HANDLE_OP_ADD},
	{"-", 10, false, PTRS_HANDLE_OP_SUB},

	{"*", 11, false, PTRS_HANDLE_OP_MUL},
	{"/", 11, false, PTRS_HANDLE_OP_DIV},
	{"%", 11, false, PTRS_HANDLE_OP_MOD}
};
static int binaryOpCount = sizeof(binaryOps) / sizeof(struct opinfo);

struct opinfo prefixOps[] = {
	{"typeof", 12, true, PTRS_HANDLE_OP_TYPEOF},
	{"sizeof", 12, true, PTRS_HANDLE_PREFIX_LENGTH}, //length aka sizeof
	{"++", 12, true, PTRS_HANDLE_PREFIX_INC}, //prefixed ++
	{"--", 12, true, PTRS_HANDLE_PREFIX_DEC}, //prefixed --
	{"!", 12, true, PTRS_HANDLE_PREFIX_LOGICNOT}, //logical NOT
	{"~", 12, true, PTRS_HANDLE_PREFIX_NOT}, //bitwise NOT
	{"&", 12, true, PTRS_HANDLE_PREFIX_ADDRESS}, //adress of
	{"*", 12, true, PTRS_HANDLE_PREFIX_DEREFERENCE}, //dereference
	{"+", 12, true, PTRS_HANDLE_PREFIX_PLUS}, //unary +
	{"-", 12, true, PTRS_HANDLE_PREFIX_MINUS} //unary -
};
static int prefixOpCount = sizeof(prefixOps) / sizeof(struct opinfo);

struct opinfo suffixedOps[] = {
	{"++", 13, false, PTRS_HANDLE_SUFFIX_INC}, //suffixed ++
	{"--", 13, false, PTRS_HANDLE_SUFFIX_DEC} //suffixed --
};
static int suffixedOpCount = sizeof(suffixedOps) / sizeof(struct opinfo);

static ptrs_ast_t *parseExpression(code_t *code)
{
	return parseBinaryExpr(code, parseUnaryExpr(code, false), 0);
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
		ptrs_ast_t *right = parseUnaryExpr(code, false);
		ahead = peekBinaryOp(code);

		while(ahead != NULL && ahead->precendence > op->precendence)
		{
			right = parseBinaryExpr(code, right, ahead->precendence);
			ahead = peekBinaryOp(code);
		}

#ifndef PTRS_DISABLE_CONSTRESOLVE
		if(left->handler == PTRS_HANDLE_CONSTANT && right->handler == PTRS_HANDLE_CONSTANT)
		{
			ptrs_lastast = left;
			ptrs_var_t result;
			ptrs_ast_t node;
			node.handler = op->handler;
			node.arg.binary.left = left;
			node.arg.binary.right = right;

			node.handler(&node, &result, NULL);
			free(right);
			memcpy(&left->arg.constval, &result, sizeof(ptrs_var_t));
			continue;
		}
#endif
		if(op->handler == PTRS_HANDLE_OP_TERNARY)
		{
			ptrs_ast_t *ast = talloc(ptrs_ast_t);
			ast->handler = PTRS_HANDLE_OP_TERNARY;
			ast->arg.ternary.condition = left;
			ast->arg.ternary.trueVal = right;
			consumec(code, ':');
			ast->arg.ternary.falseVal = parseExpression(code);
			left = ast;
			continue;
		}

		ptrs_ast_t *_left = left;
		left = talloc(ptrs_ast_t);
		left->handler = op->handler;
		left->arg.binary.left = _left;
		left->arg.binary.right = right;
		left->codepos = pos;
		left->code = code->src;
		left->file = code->filename;
	}
	return left;
}

struct constinfo
{
	char *text;
	ptrs_vartype_t type;
	ptrs_val_t value;
};
struct constinfo constants[] = {
	{"true", PTRS_TYPE_INT, {.intval = true}},
	{"false", PTRS_TYPE_INT, {.intval = false}},
	{"NULL", PTRS_TYPE_NATIVE, {.nativeval = NULL}},
	{"null", PTRS_TYPE_POINTER, {.ptrval = NULL}},
	{"VARSIZE", PTRS_TYPE_INT, {.intval = sizeof(ptrs_var_t)}},
	{"undefined", PTRS_TYPE_UNDEFINED, {}},
	{"NaN", PTRS_TYPE_FLOAT, {.floatval = NAN}},
};
int constantCount = sizeof(constants) / sizeof(struct constinfo);

static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls)
{
	char curr = code->curr;
	int pos = code->pos;
	ptrs_ast_t *ast;

	for(int i = 0; i < prefixOpCount; i++)
	{
		if(lookahead(code, prefixOps[i].op))
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.astval = parseUnaryExpr(code, false);
			ast->handler = prefixOps[i].handler;
			ast->codepos = pos;
			ast->code = code->src;
			ast->file = code->filename;
			return ast;
		}
	}

	for(int i = 0; i < constantCount; i++)
	{
		if(lookahead(code, constants[i].text))
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.constval.type = constants[i].type;
			ast->arg.constval.value = constants[i].value;
			ast->handler = PTRS_HANDLE_CONSTANT;
			return ast;
		}
	}

	if(lookahead(code, "new"))
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_NEW;
		ast->arg.call.value = parseUnaryExpr(code, true);

		consumec(code, '(');
		ast->arg.call.arguments = parseExpressionList(code, ')');
		consumec(code, ')');
	}
	else if(lookahead(code, "type"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "Syntax is type<TYPENAME>");

		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CONSTANT;
		ast->arg.constval.type = PTRS_TYPE_INT;
		ast->arg.constval.value.intval = type;
		consumec(code, '>');
	}
	else if(lookahead(code, "as"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "Syntax is as<TYPENAME>");

		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_AS;
		ast->arg.cast.type = type;
		consumec(code, '>');

		ast->arg.cast.value = parseUnaryExpr(code, false);
	}
	else if(lookahead(code, "cast"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type > PTRS_TYPE_STRUCT)
			unexpectedm(code, NULL, "Syntax is cast<TYPENAME>");

		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CAST;
		ast->arg.cast.type = type;
		consumec(code, '>');

		ast->arg.cast.value = parseUnaryExpr(code, false);
	}
	else if(lookahead(code, "function"))
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_FUNCTION;
		ast->arg.function.isAnonymous = true;
		ast->arg.function.name = "(anonymous function)";

		symbolScope_increase(code, 1);
		ast->arg.function.argc = parseArgumentDefinitionList(code,
			&ast->arg.function.args, &ast->arg.function.argv, &ast->arg.function.vararg);

		ast->arg.function.body = parseBody(code, &ast->arg.function.stackOffset, false, true);
	}
	else if(curr == '[')
	{
		next(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_VARARRAYEXPR;
		ast->arg.astlist = parseExpressionList(code, ']');
		consumec(code, ']');
	}
	else if(curr == '{')
	{
		next(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_ARRAYEXPR;
		ast->arg.astlist = parseExpressionList(code, '}');
		consumec(code, '}');
	}
	else if(isalpha(curr) || curr == '_')
	{
		ast = talloc(ptrs_ast_t);
		char *name = readIdentifier(code);
		ast->handler = PTRS_HANDLE_IDENTIFIER;
		ast->arg.varval = getSymbol(code, name);
		free(name);
	}
	else if(isdigit(curr) || curr == '.')
	{
		int startPos = code->pos;
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CONSTANT;

		ast->arg.constval.type = PTRS_TYPE_INT;
		ast->arg.constval.value.intval = readInt(code, 0);

		if(code->curr == '.' || code->curr == 'e')
		{
			code->pos = startPos;
			code->curr = code->src[code->pos];
			ast->arg.constval.type = PTRS_TYPE_FLOAT;
			ast->arg.constval.value.floatval = readDouble(code);
			lookahead(code, "f");
		}
		else if(lookahead(code, "f"))
		{
			ast->arg.constval.type = PTRS_TYPE_FLOAT;
			ast->arg.constval.value.floatval = ast->arg.constval.value.intval;
		}
	}
	else if(curr == '\'')
	{
		rawnext(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CONSTANT;
		ast->arg.constval.type = PTRS_TYPE_INT;

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
		char *str = readString(code, &len);

		ast = talloc(ptrs_ast_t);
		if(code->curr == '%')
		{
			next(code);
			ast->handler = PTRS_HANDLE_STRINGFORMAT;
			ast->arg.strformat.str = str;
			ast->arg.strformat.args = parseExpressionList(code, 0);
		}
		else
		{
			ast->handler = PTRS_HANDLE_CONSTANT;
			ast->arg.constval.type = PTRS_TYPE_NATIVE;
			ast->arg.constval.value.strval = str;
			ast->arg.constval.meta.array.readOnly = true;
			ast->arg.constval.meta.array.size = len;
		}
	}
	else if(curr == '(')
	{
		int start = code->pos;
		consumec(code, '(');

		ast = NULL;
		if(isalnum(code->curr))
		{
			free(readIdentifier(code));
			if(code->curr == ',' || (lookahead(code, ")") && lookahead(code, "->")))
			{
				code->pos = start;
				code->curr = code->src[start];

				symbolScope_increase(code, 1);
				ast = talloc(ptrs_ast_t);
				ast->handler = PTRS_HANDLE_FUNCTION;
				ast->arg.function.name = "(lambda expression)";
				ast->arg.function.argv = NULL;
				ast->arg.function.argc = parseArgumentDefinitionList(code, &ast->arg.function.args, NULL, &ast->arg.function.vararg);

				consume(code, "->");
				code->symbols->offset += 2 * sizeof(ptrs_var_t);
				if(lookahead(code, "{"))
				{
					ast->arg.function.body = parseStmtList(code, '}');
					consumec(code, '}');
				}
				else
				{
					ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
					retStmt->handler = PTRS_HANDLE_RETURN;
					retStmt->arg.astval = parseExpression(code);
					ast->arg.function.body = retStmt;
				}
				ast->arg.function.isAnonymous = true;
				ast->arg.function.stackOffset = symbolScope_decrease(code);
			}
			else
			{
				code->pos = start + 1;
				code->curr = code->src[start + 1];
			}
		}

		if(ast == NULL)
		{
			ast = parseExpression(code);
			consumec(code, ')');
		}
	}
	else
	{
		return NULL;
	}

	ast->codepos = pos;
	ast->code = code->src;
	ast->file = code->filename;

	ptrs_ast_t *old;
	do {
		old = ast;
		ast = parseUnaryExtension(code, ast, ignoreCalls);
	} while(ast != old);

	return ast;
}

static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls)
{
	char curr = code->curr;
	if(curr == '.')
	{
		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->handler = PTRS_HANDLE_MEMBER;
		member->codepos = code->pos;
		member->code = code->src;
		member->file = code->filename;

		consumec(code, '.');
		member->arg.member.base = ast;
		member->arg.member.name = readIdentifier(code);

		ast = member;
	}
	else if(!ignoreCalls && curr == '(')
	{
		ptrs_ast_t *call = talloc(ptrs_ast_t);
		call->handler = PTRS_HANDLE_CALL;
		call->codepos = code->pos;
		call->code = code->src;
		call->file = code->filename;
		consumec(code, '(');

		call->arg.call.value = ast;
		call->arg.call.arguments = parseExpressionList(code, ')');

		ast = call;
		consumec(code, ')');
	}
	else if(curr == '[')
	{
		ptrs_ast_t *indexExpr = talloc(ptrs_ast_t);
		indexExpr->handler = PTRS_HANDLE_INDEX;
		indexExpr->codepos = code->pos;
		indexExpr->code = code->src;
		indexExpr->file = code->filename;
		consumec(code, '[');

		indexExpr->arg.binary.left = ast;
		indexExpr->arg.binary.right = parseExpression(code);

		ast = indexExpr;
		consumec(code, ']');
	}
	else if(lookahead(code, "->"))
	{
		ptrs_ast_t *dereference = talloc(ptrs_ast_t);
		dereference->handler = PTRS_HANDLE_PREFIX_DEREFERENCE;
		dereference->codepos = code->pos - 2;
		dereference->code = code->src;
		dereference->file = code->filename;
		dereference->arg.astval = ast;

		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->handler = PTRS_HANDLE_MEMBER;
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
		for(int i = 0; i < suffixedOpCount; i++)
		{
			if(lookahead(code, suffixedOps[i].op))
			{
				ptrs_ast_t *opAst = talloc(ptrs_ast_t);
				opAst->codepos = pos;
				opAst->code = code->src;
				opAst->file = code->filename;
				opAst->arg.astval = ast;
				opAst->handler = suffixedOps[i].handler;
				return opAst;
			}
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
		if(lookahead(code, "..."))
		{
			curr->expand = true;
			curr->entry = parseExpression(code);
			if(curr->entry->handler != PTRS_HANDLE_IDENTIFIER)
				unexpectedm(code, NULL, "Array spreading can only be used on identifiers");
		}
		else
		{
			curr->expand = false;
			curr->entry = parseExpression(code);
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

static void parseStruct(code_t *code, ptrs_struct_t *struc)
{
	char *name;
	char *structName = readIdentifier(code);
	int structNameLen = strlen(structName);

	struc->name = strdup(structName);
	struc->symbol = addSymbol(code, structName);
	struc->overloads = NULL;
	struc->size = 0;
	struc->data = NULL;
	consumec(code, '{');

	struct ptrs_structlist *curr = NULL;
	while(code->curr != '}')
	{
		if(lookahead(code, "operator"))
		{
			symbolScope_increase(code, 1);

			const char *nameFormat;
			const char *opLabel;
			char *otherName = NULL;
			ptrs_function_t *func = talloc(ptrs_function_t);
			func->argc = 0;
			func->argv = NULL;
			func->vararg.scope = (unsigned)-1;

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->isLeftSide = true;

			overload->op = readPrefixOperator(code, &opLabel);

			if(overload->op != NULL)
			{
				nameFormat = "%1$s.op %2$sthis";
				consume(code, "this");
			}
			else
			{
				if(lookahead(code, "this"))
				{
					overload->op = readSuffixOperator(code, &opLabel);
					nameFormat = "%1$s.op this%2$s";

					if(overload->op == NULL)
					{
						func->argc = 1;
						overload->op = readBinaryOperator(code, &opLabel);

						if(overload->op != NULL)
						{
							nameFormat = "%1$s.op this %2$s %3$s";
							otherName = readIdentifier(code);
							func->args = talloc(ptrs_symbol_t);
							func->args[0] = addSymbol(code, otherName);
						}
						else
						{
							if(code->curr == '.')
							{
								next(code);
								opLabel = ".";
								nameFormat = "%1$s.op this.%3$s";
								otherName = readIdentifier(code);

								func->args = talloc(ptrs_symbol_t);
								func->args[0] = addSymbol(code, otherName);
								overload->op = PTRS_HANDLE_MEMBER;
							}
							else if(code->curr == '[')
							{
								next(code);
								opLabel = "[]";
								nameFormat = "%1$s.op this[%3$s]";
								otherName = readIdentifier(code);

								func->args = talloc(ptrs_symbol_t);
								func->args[0] = addSymbol(code, otherName);
								consumec(code, ']');
								overload->op = PTRS_HANDLE_INDEX;
							}
							else if(code->curr == '(')
							{
								func->argc = parseArgumentDefinitionList(code,
									&func->args, &func->argv, &func->vararg);
								overload->op = PTRS_HANDLE_CALL;

								opLabel = "()";
								nameFormat = "%1$s.op this()";
							}
						}

					}
				}
				else
				{
					nameFormat = "%1$s.op %3$s %2$s this";
					otherName = readIdentifier(code);

					func->argc = 1;
					func->args = talloc(ptrs_symbol_t);
					func->args[0] = addSymbol(code, otherName);

					overload->isLeftSide = false;
					overload->op = readBinaryOperator(code, &opLabel);
					consume(code, "this");
				}

				if(overload->op == NULL)
					unexpected(code, "Operator");
			}

			func->name = malloc(snprintf(NULL, 0, nameFormat, structName, opLabel, otherName) + 1);
			sprintf(func->name, nameFormat, structName, opLabel, otherName);

			func->body = parseBody(code, &func->stackOffset, false, true);

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
			overload->isLeftSide = true;
			overload->op = PTRS_HANDLE_NEW;
			overload->handler = parseFunction(code, name);

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}
		else if(lookahead(code, "destructor"))
		{
			name = malloc(structNameLen + strlen("destructor") + 2);
			sprintf(name, "%s.destructor", structName);

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->isLeftSide = true;
			overload->op = PTRS_HANDLE_DELETE;
			overload->handler = parseFunction(code, name);

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}

		name = readIdentifier(code);
		struct ptrs_structlist *next = talloc(struct ptrs_structlist);
		next->next = curr;
		curr = next;

		curr->name = name;

		if(code->curr == '(')
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
			ptrs_ast_t *ast = parseExpression(code);
			consumec(code, ']');
			consumec(code, ';');

			if(ast->handler != PTRS_HANDLE_CONSTANT)
				PTRS_HANDLE_ASTERROR(ast, "Struct array member size must be a constant");

			curr->value.size = ast->arg.constval.value.intval * sizeof(ptrs_var_t);
			curr->offset = struc->size;
			struc->size += curr->value.size;
			free(ast);
		}
		else if(code->curr == '{')
		{
			curr->type = PTRS_STRUCTMEMBER_ARRAY;
			consumec(code, '{');
			ptrs_ast_t *ast = parseExpression(code);
			consumec(code, '}');
			consumec(code, ';');

			if(ast->handler != PTRS_HANDLE_CONSTANT)
				PTRS_HANDLE_ASTERROR(ast, "Struct array member size must be a constant");

			curr->value.size = ast->arg.constval.value.intval;
			curr->offset = struc->size;
			struc->size += curr->value.size;
			free(ast);
		}
		else
		{
			curr->offset = struc->size;
			struc->size += sizeof(ptrs_var_t);

			if(lookahead(code, "="))
				curr->value.startval = parseExpression(code);
			else
				curr->value.startval = NULL;

			consumec(code, ';');
		}
	}
	struc->member = curr;
	consumec(code, '}');
	consumec(code, ';');
}

struct typeName
{
	const char *name;
	ptrs_vartype_t type;
};
struct typeName typeNames[] = {
	{"undefined", PTRS_TYPE_UNDEFINED},
	{"int", PTRS_TYPE_INT},
	{"float", PTRS_TYPE_FLOAT},
	{"native", PTRS_TYPE_NATIVE},
	{"pointer", PTRS_TYPE_POINTER},
	{"function", PTRS_TYPE_FUNCTION},
	{"struct", PTRS_TYPE_STRUCT}
};
static int typeNameCount = sizeof(typeNames) / sizeof(struct typeName);

static ptrs_vartype_t readTypeName(code_t *code)
{
	for(int i = 0; i < typeNameCount; i++)
	{
		if(lookahead(code, typeNames[i].name))
		{
			return typeNames[i].type;
		}
	}

	return PTRS_TYPE_STRUCT + 1;
}

static ptrs_asthandler_t readOperatorFrom(code_t *code, const char **label, struct opinfo *ops, int opCount)
{
	for(int i = 0; i < opCount; i++)
	{
		if(lookahead(code, ops[i].op))
		{
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
static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, binaryOps, binaryOpCount);
}

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

static char *readString(code_t *code, int *length)
{
	int start = code->pos;
	int len = 0;
	for(;;)
	{
		char *curr = &code->src[code->pos];
		while(*curr != '"')
		{
			if(*curr == '\\')
				curr++;
			curr++;
			len++;
		}
		code->pos += curr - &code->src[code->pos];

		next(code);
		if(code->curr == '"')
			rawnext(code);
		else
			break;
	}
	*length = len;

	code->pos = start;
	code->curr = code->src[code->pos];
	char *val = malloc(len + 1);
	char *currVal = val;
	for(;;)
	{
		while(code->curr != '"')
		{
			if(code->curr == '\\')
			{
				rawnext(code);
				*currVal++ = readEscapeSequence(code);
			}
			else
			{
				*currVal++ = code->curr;
			}
			rawnext(code);
		}

		next(code);
		if(code->curr == '"')
			rawnext(code);
		else
			break;
	}

	val[len] = 0;
	return val;
}

static char readEscapeSequence(code_t *code)
{
	switch(code->curr)
	{
		case 'a':
			return '\a';
			break;
		case 'b':
			return '\b';
			break;
		case 'f':
			return '\f';
			break;
		case 'n':
			return '\n';
			break;
		case 'r':
			return '\r';
			break;
		case 't':
			return '\t';
			break;
		case 'v':
			return '\v';
			break;
		case '\\':
			return '\\';
			break;
		case '\'':
			return '\'';
			break;
		case '"':
			return '"';
			break;
		case '?':
			return '\?';
			break;
		case 'x':
			rawnext(code);
			return (char)readInt(code, 16);
			break;
		default:
			if(isdigit(code->curr))
				return (char)readInt(code, 8);
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

	entry->text = text;
	entry->next = curr->current;
	entry->offset = offset;
	curr->current = entry;
}

static ptrs_symbol_t addSymbol(code_t *code, char *symbol)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->text = symbol;
	entry->next = curr->current;
	entry->offset = curr->offset;
	curr->offset += sizeof(ptrs_var_t);
	curr->current = entry;

	ptrs_symbol_t result = {0, entry->offset};
	return result;
}

static ptrs_symbol_t getSymbol(code_t *code, char *text)
{
	ptrs_symbol_t out;
	if(ptrs_ast_getSymbol(code->symbols, text, &out) == 0)
		return out;

	char buff[128];
	sprintf(buff, "Unknown identifier %s", text);
	unexpectedm(code, NULL, buff);
	return out; //doh
}

static void symbolScope_increase(code_t *code, int buildInCount)
{
	ptrs_symboltable_t *new = talloc(ptrs_symboltable_t);
	new->offset = buildInCount * sizeof(ptrs_var_t);
	new->outer = code->symbols;
	new->current = NULL;
	code->symbols = new;
}

static unsigned symbolScope_decrease(code_t *code)
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

	unsigned val = scope->offset;
	free(scope);
	return val;
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
	code->pos++;
	while(skipSpaces(code) || skipComments(code));
	code->curr = code->src[code->pos];
}
static void rawnext(code_t *code)
{
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
