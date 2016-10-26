#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#ifndef _PTRS_NOASM
#include <jitas.h>
#include <sys/mman.h>
#endif

#include "common.h"
#include INTERPRETER_INCLUDE
#include "ast.h"

#define talloc(type) malloc(sizeof(type))

typedef struct code code_t;
struct symbollist;
typedef ptrs_ast_t *(*symbolcreator_t)(unsigned scopeLevel, struct symbollist *entry);

struct symbollist
{
	union
	{
		unsigned offset;
		void *data;
	} arg;
	symbolcreator_t creator;
	char *text;
	struct symbollist *next;
};
struct ptrs_symboltable
{
	unsigned offset;
	unsigned maxOffset;
	bool isInline;
	struct symbollist *current;
	ptrs_symboltable_t *outer;
};
struct code
{
	const char *filename;
	char *src;
	char curr;
	int pos;
	ptrs_symboltable_t *symbols;
};

static ptrs_ast_t *parseStmtList(code_t *code, char end);
static ptrs_ast_t *parseStatement(code_t *code);
static ptrs_ast_t *parseExpression(code_t *code);
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls, bool ignoreAlgo);
static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls, bool ignoreAlgo);
static struct ptrs_astlist *parseExpressionList(code_t *code, char end);
static void parseAsm(code_t *code, ptrs_ast_t *stmt);
static void parseStruct(code_t *code, ptrs_struct_t *struc);
static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt);

static ptrs_vartype_t readTypeName(code_t *code);
static ptrs_nativetype_info_t *readNativeType(code_t *code);
static ptrs_asthandler_t readPrefixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readSuffixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label);
static char *readIdentifier(code_t *code);
static char *readString(code_t *code, int *length);
static char readEscapeSequence(code_t *code);
static int64_t readInt(code_t *code, int base);
static double readDouble(code_t *code);

static ptrs_ast_t *defaultSymbolCreator(unsigned scopeLevel, struct symbollist *entry);
static ptrs_ast_t *constSymbolCreator(unsigned scopeLevel, struct symbollist *entry);
static ptrs_ast_t *structMemberSymbolCreator(unsigned scopeLevel, struct symbollist *entry);
static void setSymbol(code_t *code, char *text, unsigned offset);
static ptrs_symbol_t addSymbol(code_t *code, char *text);
static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, symbolcreator_t creator);
static ptrs_ast_t *getSymbol(code_t *code, char *text);
static void symbolScope_increase(code_t *code, int buildInCount, bool isInline);
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

	if(src[0] == '#' && src[1] == '!')
	{
		while(code.curr != '\n')
		{
			code.pos++;
			code.curr = code.src[code.pos];
		}
		next(&code);
	}

	symbolScope_increase(&code, 1, false);
	setSymbol(&code, strdup("arguments"), 0);

	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = PTRS_HANDLE_FILE;
	ast->arg.scopestatement.body = parseStmtList(&code, 0);
	if(code.symbols->offset > code.symbols->maxOffset)
		ast->arg.scopestatement.stackOffset = code.symbols->offset;
	else
		ast->arg.scopestatement.stackOffset = code.symbols->maxOffset;

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
				*node = curr->creator(level, curr);
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
		if(curr->entry == NULL)
			continue;

		if(code->curr == end || code->curr == 0)
			break;

		struct ptrs_astlist *next = talloc(struct ptrs_astlist);
		curr->next = next;
		curr = next;
	}

	curr->next = NULL;
	return elem;
}

int parseIdentifierList(code_t *code, char *end, ptrs_symbol_t **symbols, char ***fields)
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
	*symbols = malloc(sizeof(ptrs_symbol_t) * count);

	for(int i = 0; i < count; i++)
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

		(*symbols)[i] = addSymbol(code, name);
		if(i < count - 1)
			consumec(code, ',');
	}

	return count;
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
			ptrs_symbol_t curr;
			if(lookahead(code, "_"))
			{
				curr.scope = (unsigned)-1;
			}
			else
			{
				curr = addSymbol(code, readIdentifier(code));

				if(vararg != NULL && lookahead(code, "..."))
				{
					*vararg = curr;
					consumecm(code, ')', "Vararg argument has to be the last argument");
					break;
				}
			}

			list->next = talloc(struct argdeflist);
			list = list->next;
			list->symbol = curr;

			if(argv != NULL)
			{
				if(curr.scope != (unsigned)-1 && lookahead(code, "="))
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
	if(!isFunction)
		symbolScope_increase(code, 0, true);

	ptrs_ast_t *result = parseScopelessBody(code, allowStmt);

	*stackOffset = symbolScope_decrease(code);
	return result;
}

static ptrs_function_t *parseFunction(code_t *code, char *name)
{
	symbolScope_increase(code, 1, false);
	setSymbol(code, strdup("this"), 0);

	ptrs_function_t *func = talloc(ptrs_function_t);
	func->name = name;

	func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
	func->body = parseBody(code, &func->stackOffset, false, true);

	return func;
}

static ptrs_ast_t *parseStatement(code_t *code)
{
	if(lookahead(code, "const"))
	{
		char *name = readIdentifier(code);
		consumec(code, '=');
		ptrs_ast_t *ast = parseExpression(code);
		if(ast->handler != PTRS_HANDLE_CONSTANT)
			unexpectedm(code, NULL, "Initializer for 'const' variable is not a constant");

		struct symbollist *entry = addSpecialSymbol(code, name, constSymbolCreator);
		entry->arg.data = ast;
		consumec(code, ';');
		return NULL;
	}

	ptrs_ast_t *stmt = talloc(ptrs_ast_t);
	stmt->setHandler = NULL;
	stmt->callHandler = NULL;
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
				stmt->arg.define.initExpr = NULL;
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
				stmt->arg.define.initExpr = NULL;
				stmt->arg.define.initVal = NULL;
			}
		}
		else if(lookahead(code, ":"))
		{
			stmt->handler = PTRS_HANDLE_STRUCTVAR;
			stmt->arg.define.value = parseUnaryExpr(code, true, false);

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
		stmt->handler = PTRS_HANDLE_IMPORT;
		stmt->arg.import.count = parseIdentifierList(code, "from", &stmt->arg.import.symbols,
			&stmt->arg.import.fields);

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
	else if(lookahead(code, "scoped"))
	{
		stmt->handler = PTRS_HANDLE_SCOPESTATEMENT;
		symbolScope_increase(code, 0, false);
		stmt->arg.scopestatement.body = parseBody(code, &stmt->arg.scopestatement.stackOffset, true, true);
	}
	else if(lookahead(code, "try"))
	{
		stmt->handler = PTRS_HANDLE_TRYCATCH;
		stmt->arg.trycatch.tryBody = parseBody(code, &stmt->arg.trycatch.tryStackOffset, true, false);

		if(lookahead(code, "catch"))
		{
			symbolScope_increase(code, 0, true);
			stmt->arg.trycatch.argc = parseArgumentDefinitionList(code, &stmt->arg.trycatch.args, NULL, NULL);
			stmt->arg.trycatch.catchBody = parseScopelessBody(code, true);
			stmt->arg.trycatch.catchStackOffset = symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.catchBody = NULL;
		}
	}
	else if(lookahead(code, "asm"))
	{
#ifndef _PTRS_NOASM
		stmt->handler = PTRS_HANDLE_ASM;
		parseAsm(code, stmt);
#else
		PTRS_HANDLE_ASTERROR(stmt, "Inline assembly is not available");
#endif
	}
	else if(lookahead(code, "function"))
	{
		stmt->handler = PTRS_HANDLE_FUNCTION;
		stmt->arg.function.isAnonymous = false;

		char *name = readIdentifier(code);
		ptrs_ast_t *oldAst;
		if(ptrs_ast_getSymbol(code->symbols, name, &oldAst) == 0)
		{
			if(oldAst->handler != PTRS_HANDLE_IDENTIFIER)
				PTRS_HANDLE_ASTERROR(stmt, "Cannot redefine special symbol %s as a function", name);

			stmt->arg.function.symbol = oldAst->arg.varval;
			free(oldAst);
		}
		else
		{
			stmt->arg.function.symbol = addSymbol(code, strdup(name));
		}
		stmt->arg.function.name = name;

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

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
	else if(lookahead(code, "switch"))
	{
		parseSwitchCase(code, stmt);
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
		symbolScope_increase(code, 0, true);
		stmt->handler = PTRS_HANDLE_DOWHILE;
		stmt->arg.control.body = parseScopelessBody(code, true);
		consume(code, "while");

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');
		consumec(code, ';');
		stmt->arg.control.stackOffset = symbolScope_decrease(code);
	}
	else if(lookahead(code, "foreach"))
	{
		consumec(code, '(');
		symbolScope_increase(code, 0, false);
		stmt->arg.forin.varcount = parseIdentifierList(code, "in", &stmt->arg.forin.varsymbols, NULL);

		consume(code, "in");
		stmt->arg.forin.value = parseExpression(code);
		stmt->handler = PTRS_HANDLE_FORIN;
		consumec(code, ')');

		stmt->arg.forin.body = parseScopelessBody(code, true);
		stmt->arg.forin.stackOffset = symbolScope_decrease(code);
	}
	else if(lookahead(code, "for"))
	{
		stmt->handler = PTRS_HANDLE_FOR;
		symbolScope_increase(code, 0, true);

		consumec(code, '(');
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
	{">>", 12, false, PTRS_HANDLE_OP_SHR}, //shift right
	{"<<", 12, false, PTRS_HANDLE_OP_SHL}, //shift left

	{"===", 9, false, PTRS_HANDLE_OP_TYPEEQUAL},
	{"!==", 9, false, PTRS_HANDLE_OP_TYPEINEQUAL},

	{"==", 9, false, PTRS_HANDLE_OP_EQUAL},
	{"!=", 9, false, PTRS_HANDLE_OP_INEQUAL},
	{"<=", 10, false, PTRS_HANDLE_OP_LESSEQUAL},
	{">=", 10, false, PTRS_HANDLE_OP_GREATEREQUAL},
	{"<", 10, false, PTRS_HANDLE_OP_LESS},
	{">", 10, false, PTRS_HANDLE_OP_GREATER},

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

	{"instanceof", 11, false, PTRS_HANDLE_OP_INSTANCEOF},
	{"in", 11, false, PTRS_HANDLE_OP_IN},

	{"||", 3, false, PTRS_HANDLE_OP_LOGICOR},
	{"^^", 4, false, PTRS_HANDLE_OP_LOGICXOR},
	{"&&", 5, false, PTRS_HANDLE_OP_LOGICAND},

	{"|", 6, false, PTRS_HANDLE_OP_OR},
	{"^", 7, false, PTRS_HANDLE_OP_XOR},
	{"&", 8, false, PTRS_HANDLE_OP_AND},

	{"+", 13, false, PTRS_HANDLE_OP_ADD},
	{"-", 13, false, PTRS_HANDLE_OP_SUB},

	{"*", 14, false, PTRS_HANDLE_OP_MUL},
	{"/", 14, false, PTRS_HANDLE_OP_DIV},
	{"%", 14, false, PTRS_HANDLE_OP_MOD}
};
static int binaryOpCount = sizeof(binaryOps) / sizeof(struct opinfo);

struct opinfo prefixOps[] = {
	{"typeof", 0, true, PTRS_HANDLE_OP_TYPEOF},
	{"sizeof", 0, true, PTRS_HANDLE_PREFIX_LENGTH}, //length aka sizeof
	{"++", 0, true, PTRS_HANDLE_PREFIX_INC}, //prefixed ++
	{"--", 0, true, PTRS_HANDLE_PREFIX_DEC}, //prefixed --
	{"!", 0, true, PTRS_HANDLE_PREFIX_LOGICNOT}, //logical NOT
	{"~", 0, true, PTRS_HANDLE_PREFIX_NOT}, //bitwise NOT
	{"&", 0, true, PTRS_HANDLE_PREFIX_ADDRESS}, //adress of
	{"*", 0, true, PTRS_HANDLE_PREFIX_DEREFERENCE}, //dereference
	{"+", 0, true, PTRS_HANDLE_PREFIX_PLUS}, //unary +
	{"-", 0, true, PTRS_HANDLE_PREFIX_MINUS} //unary -
};
static int prefixOpCount = sizeof(prefixOps) / sizeof(struct opinfo);

struct opinfo suffixedOps[] = {
	{"++", 0, false, PTRS_HANDLE_SUFFIX_INC}, //suffixed ++
	{"--", 0, false, PTRS_HANDLE_SUFFIX_DEC} //suffixed --
};
static int suffixedOpCount = sizeof(suffixedOps) / sizeof(struct opinfo);

static ptrs_ast_t *parseExpression(code_t *code)
{
	return parseBinaryExpr(code, parseUnaryExpr(code, false, false), 0);
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
		left->setHandler = NULL;
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
};
int constantCount = sizeof(constants) / sizeof(struct constinfo);

static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls, bool ignoreAlgo)
{
	char curr = code->curr;
	int pos = code->pos;
	bool noSetHandler = true;
	ptrs_ast_t *ast;

	for(int i = 0; i < prefixOpCount; i++)
	{
		if(lookahead(code, prefixOps[i].op))
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.astval = parseUnaryExpr(code, false, true);
			ast->handler = prefixOps[i].handler;
			if(ast->handler == PTRS_HANDLE_PREFIX_DEREFERENCE)
				ast->setHandler = PTRS_HANDLE_ASSIGN_DEREFERENCE;
			else
				ast->setHandler = NULL;

			ast->codepos = pos;
			ast->code = code->src;
			ast->file = code->filename;

#ifndef PTRS_DISABLE_CONSTRESOLVE
			if(ast->arg.astval->handler == PTRS_HANDLE_CONSTANT)
			{
				ptrs_var_t result;
				ast->handler(ast, &result, NULL);
				ast->handler = PTRS_HANDLE_CONSTANT;

				free(ast->arg.astval);
				memcpy(&ast->arg.constval, &result, sizeof(ptrs_var_t));
			}
#endif
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
			ast->setHandler = NULL;
			ast->callHandler = NULL;
			return ast;
		}
	}

	if(lookahead(code, "new"))
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_NEW;
		ast->arg.newexpr.onStack = false;
		ast->arg.newexpr.value = parseUnaryExpr(code, true, false);

		consumec(code, '(');
		ast->arg.newexpr.arguments = parseExpressionList(code, ')');
		consumec(code, ')');
	}
	else if(lookahead(code, "new_stack")) //TODO find a better syntax for this
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_NEW;
		ast->arg.newexpr.onStack = true;
		ast->arg.newexpr.value = parseUnaryExpr(code, true, false);

		consumec(code, '(');
		ast->arg.newexpr.arguments = parseExpressionList(code, ')');
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
			unexpectedm(code, NULL, "Syntax is as<TYPE>");

		consumec(code, '>');
		ptrs_ast_t *val = parseUnaryExpr(code, false, true);

		if(val->handler == PTRS_HANDLE_CALL)
		{
			val->arg.call.retType = type;
			ast = val;
		}
		else
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = PTRS_HANDLE_AS;
			ast->arg.cast.builtinType = type;
			ast->arg.cast.value = val;
		}
	}
	else if(lookahead(code, "cast_stack")) //TODO find a better syntax for this
	{
		consumec(code, '<');
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CAST;
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
			ast = talloc(ptrs_ast_t);
			ast->handler = PTRS_HANDLE_CAST_BUILTIN;
			ast->arg.cast.builtinType = type;
		}
		else
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = PTRS_HANDLE_CAST;
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
		if(ptrs_ast_getSymbol(code->symbols, ".yield", &ast) == 0)
		{
			ast->handler = PTRS_HANDLE_YIELD;
			ast->arg.yield.yieldVal = ast->arg.varval;
			ast->arg.yield.values = parseExpressionList(code, ';');
		}
		else if(ptrs_ast_getSymbol(code->symbols, ".yield_algorithm", &ast) == 0)
		{
			ast->handler = PTRS_HANDLE_YIELD_ALGORITHM;
			ast->arg.yield.yieldVal = ast->arg.varval;
			ast->arg.yield.value = parseExpression(code);
		}
		else
		{
			unexpectedm(code, NULL, "Yield expressions are only allowed in foreach and algorithm overloads");
		}
	}
	else if(lookahead(code, "function"))
	{
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_FUNCTION;
		ast->arg.function.isAnonymous = true;
		ast->arg.function.name = "(anonymous function)";

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

		ast->arg.function.argc = parseArgumentDefinitionList(code,
			&ast->arg.function.args, &ast->arg.function.argv, &ast->arg.function.vararg);

		ast->arg.function.body = parseBody(code, &ast->arg.function.stackOffset, false, true);
	}
	else if(lookahead(code, "map"))
	{
		ptrs_struct_t *struc = talloc(ptrs_struct_t);
		ptrs_ast_t *constExpr = talloc(ptrs_ast_t);

		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_NEW;
		ast->arg.call.value = constExpr;
		ast->arg.call.arguments = NULL;

		constExpr->handler = PTRS_HANDLE_CONSTANT;
		constExpr->arg.constval.type = PTRS_TYPE_STRUCT;
		constExpr->arg.constval.value.structval = struc;

		struc->size = 0;
		struc->name = "(map)";
		struc->overloads = NULL;
		struc->data = NULL;
		struc->staticData = NULL;

		consumec(code, '{');

		struct ptrs_structlist *curr = NULL;
		for(;;)
		{
			if(curr == NULL)
			{
				curr = talloc(struct ptrs_structlist);
				struc->member = curr;
			}
			else
			{
				curr->next = talloc(struct ptrs_structlist);
				curr = curr->next;
			}
			curr->type = PTRS_STRUCTMEMBER_VAR;
			curr->isPrivate = false;
			curr->offset = struc->size;
			struc->size += sizeof(ptrs_var_t);

			if(lookahead(code, "\""))
				curr->name = readString(code, NULL);
			else
				curr->name = readIdentifier(code);

			consumec(code, ':');
			curr->value.startval = parseExpression(code);

			if(code->curr == '}')
				break;
			consumec(code, ',');
		}

		consumec(code, '}');
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
		char *name = readIdentifier(code);
		ast = getSymbol(code, name);
		noSetHandler = false;
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
		ast->handler = PTRS_HANDLE_CONSTANT;
		ast->arg.constval.type = PTRS_TYPE_NATIVE;
		ast->arg.constval.value.strval = str;
		ast->arg.constval.meta.array.readOnly = true;
		ast->arg.constval.meta.array.size = len;
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

				symbolScope_increase(code, 1, false);
				setSymbol(code, strdup("this"), 0);

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

	if(noSetHandler)
	{
		ast->setHandler = NULL;
		ast->callHandler = NULL;
	}
	ast->codepos = pos;
	ast->code = code->src;
	ast->file = code->filename;

	ptrs_ast_t *old;
	do {
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
		member->handler = PTRS_HANDLE_MEMBER;
		member->setHandler = PTRS_HANDLE_ASSIGN_MEMBER;
		member->callHandler = PTRS_HANDLE_CALL_MEMBER;
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
		call->setHandler = NULL;
		call->codepos = code->pos;
		call->code = code->src;
		call->file = code->filename;
		consumec(code, '(');

		call->arg.call.retType = PTRS_TYPE_INT;
		call->arg.call.value = ast;
		call->arg.call.arguments = parseExpressionList(code, ')');

		ast = call;
		consumec(code, ')');
	}
	else if(curr == '[')
	{
		ptrs_ast_t *indexExpr = talloc(ptrs_ast_t);
		indexExpr->codepos = code->pos;
		indexExpr->code = code->src;
		indexExpr->file = code->filename;

		consumec(code, '[');
		ptrs_ast_t *expr = parseExpression(code);

		if(lookahead(code, ".."))
		{
			indexExpr->handler = PTRS_HANDLE_SLICE;
			indexExpr->arg.slice.base = ast;
			indexExpr->arg.slice.start = expr;
			indexExpr->arg.slice.end = parseExpression(code);
		}
		else
		{
			indexExpr->handler = PTRS_HANDLE_INDEX;
			indexExpr->setHandler = PTRS_HANDLE_ASSIGN_INDEX;
			indexExpr->callHandler = PTRS_HANDLE_CALL_INDEX;
			indexExpr->arg.binary.left = ast;
			indexExpr->arg.binary.right = expr;
		}

		ast = indexExpr;
		consumec(code, ']');
	}
	else if(lookahead(code, "->"))
	{
		ptrs_ast_t *dereference = talloc(ptrs_ast_t);
		dereference->handler = PTRS_HANDLE_PREFIX_DEREFERENCE;
		dereference->setHandler = PTRS_HANDLE_ASSIGN_DEREFERENCE;
		dereference->callHandler = NULL;
		dereference->codepos = code->pos - 2;
		dereference->code = code->src;
		dereference->file = code->filename;
		dereference->arg.astval = ast;

		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->handler = PTRS_HANDLE_MEMBER;
		member->setHandler = PTRS_HANDLE_ASSIGN_MEMBER;
		member->callHandler = PTRS_HANDLE_CALL_MEMBER;
		member->codepos = code->pos;
		member->code = code->src;
		member->file = code->filename;

		member->arg.member.base = dereference;
		member->arg.member.name = readIdentifier(code);

		ast = member;
	}
	else if(!ignoreAlgo && lookahead(code, "=>"))
	{
		struct ptrs_astlist *curr = talloc(struct ptrs_astlist);
		curr->entry = ast;
		curr->expand = false;

		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_ALGORITHM;
		ast->arg.astlist = curr;
		ast->code = code->src;
		ast->codepos = curr->entry->codepos;
		ast->file = curr->entry->file;

		do
		{
			curr->next = talloc(struct ptrs_astlist);
			curr = curr->next;

			curr->entry = parseUnaryExpr(code, false, true);
			curr->expand = false;
		} while(lookahead(code, "=>"));

		curr->next = NULL;
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
				opAst->setHandler = NULL;
				opAst->callHandler = NULL;
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
		if(lookahead(code, "_"))
		{
			curr->expand = false;
			curr->entry = NULL;
		}
		else if(lookahead(code, "..."))
		{
			curr->expand = true;
			curr->entry = parseExpression(code);
			if(curr->entry->handler != PTRS_HANDLE_IDENTIFIER)
				unexpectedm(code, NULL, "Array spreading can only be used on identifiers");

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}
		else
		{
			curr->expand = false;
			curr->entry = parseExpression(code);

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

static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt)
{
	consumec(code, '(');
	stmt->arg.switchcase.condition = parseExpression(code);
	consumec(code, ')');
	consumec(code, '{');

	bool isEnd = false;
	char isDefault = 0; //0 = nothing, 1 = default's head, 2 = default's body, 3 = done

	stmt->handler = PTRS_HANDLE_SWITCH;
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
			ptrs_ast_t *expr = talloc(ptrs_ast_t);
			expr->handler = PTRS_HANDLE_BODY;
			expr->arg.astlist = bodyStart;

			if(isDefault == 2)
			{
				stmt->arg.switchcase.defaultCase = expr;
				isDefault = 3;
			}

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
					ptrs_ast_t *expr = parseExpression(code);
					if(expr->handler != PTRS_HANDLE_CONSTANT || expr->arg.constval.type != PTRS_TYPE_INT)
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

#ifndef _PTRS_NOASM
struct ptrs_asmStatement
{
	uint8_t *start;
	uint8_t *end;
	ptrs_ast_t *ast;
	jitas_context_t *context;
	struct ptrs_asmStatement *next;
};
struct ptrs_asmStatement *ptrs_asmStatements = NULL;
void *ptrs_asmBuffStart = NULL;
void *ptrs_asmBuff = NULL;
size_t ptrs_asmSize = 4096;
static void parseAsm(code_t *code, ptrs_ast_t *stmt)
{
	struct ptrs_ast_asm *arg = &stmt->arg.asmstmt;

	if(ptrs_asmBuff == NULL)
	{
		ptrs_asmBuff = mmap(NULL, ptrs_asmSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		ptrs_asmBuffStart = ptrs_asmBuff;
	}

	arg->asmFunc = ptrs_asmBuff;
	arg->context = malloc(sizeof(jitas_context_t));
	jitas_init(arg->context, ptrs_asmBuff, NULL);
	arg->context->identifierToken = "._*";

	char **fields;
	if(code->curr == '{')
		arg->exportCount = 0;
	else
		arg->exportCount = parseIdentifierList(code, "{",
			&arg->exportSymbols, &fields);

	consumec(code, '{');

	char buff[128];
	char *buffptr = buff;
	int startPos = code->pos;
	jitas_symboltable_t *curr = NULL;
	jitas_symboltable_t *last = NULL;

	while(code->curr != '}' && code->curr != 0)
	{
		if(code->curr == '\n' || skipComments(code))
		{
			*buffptr = 0;
			rawnext(code);
			buffptr = buff;

			if(arg->context->ptr - (uint8_t *)ptrs_asmBuff > ptrs_asmSize)
				PTRS_HANDLE_ASTERROR(stmt, "Inline assembly size exceed. Try running with --asm-size <value>");

			jitas_assemble(arg->context, buff);

			int line;
			char *error = jitas_error(arg->context, &line);
			if(error != NULL)
			{
				while(jitas_error(arg->context, NULL) != NULL);

				ptrs_ast_t errstmt;
				memcpy(&errstmt, stmt, sizeof(ptrs_ast_t));
				errstmt.codepos = startPos;
				PTRS_HANDLE_ASTERROR(&errstmt, "%s", error);
			}

			curr = arg->context->symbols;
			while(curr != last)
			{
				curr->line = startPos;
				curr = curr->next;
			}
			last = curr;

			startPos = code->pos;
		}
		else
		{
			*buffptr++ = code->curr;
			rawnext(code);
		}
	}
	consumec(code, '}');

	struct ptrs_asmStatement *asmStmt = malloc(sizeof(struct ptrs_asmStatement));
	asmStmt->start = ptrs_asmBuff;
	asmStmt->end = arg->context->ptr - 1;
	asmStmt->ast = stmt;
	asmStmt->context = arg->context;
	asmStmt->next = ptrs_asmStatements;
	ptrs_asmStatements = asmStmt;

	ptrs_asmBuff = arg->context->ptr;

	arg->importCount = 0;
	curr = arg->context->symbols;
	while(curr != NULL)
	{
		if(jitas_findLocalSymbol(arg->context, curr->symbol) == NULL)
			arg->importCount++;

		curr = curr->next;
	}

	if(arg->importCount != 0)
	{
		arg->imports = malloc(sizeof(const char *) * arg->importCount);
		arg->importAsts = malloc(sizeof(ptrs_ast_t **) * arg->importCount);

		curr = arg->context->symbols;
		int index = 0;
		while(curr != NULL)
		{
			if(jitas_findLocalSymbol(arg->context, curr->symbol) == NULL)
			{
				arg->imports[index] = curr->symbol;
				if(curr->symbol[0] == '*')
					arg->importAsts[index] = getSymbol(code, (char *)curr->symbol + 1);
				else
					arg->importAsts[index] = getSymbol(code, (char *)curr->symbol);
				index++;
			}

			curr = curr->next;
		}
	}

	if(arg->exportCount != 0)
	{
		arg->exports = malloc(sizeof(void *) * arg->exportCount);
		arg->exportSymbols = malloc(sizeof(ptrs_symbol_t) * arg->exportCount);
		for(int i = 0; i < arg->exportCount; i++)
		{
			void *ptr = jitas_findLocalSymbol(arg->context, fields[i]);
			if(ptr == NULL)
				PTRS_HANDLE_ASTERROR(stmt, "Assembly code has no symbol '%s'", fields[i]);

			arg->exports[i] = ptr;
			arg->exportSymbols[i] = addSymbol(code, fields[i]);
		}

		free(fields);
	}
}
#endif

static void parseStruct(code_t *code, ptrs_struct_t *struc)
{
	char *name;
	char *structName = readIdentifier(code);
	int structNameLen = strlen(structName);
	int staticMemSize = 0;
	
	ptrs_ast_t *oldAst;
	if(ptrs_ast_getSymbol(code->symbols, structName, &oldAst) == 0)
	{
		if(oldAst->handler != PTRS_HANDLE_IDENTIFIER)
			PTRS_HANDLE_ASTERROR(NULL, "Cannot redefine special symbol %s as a function", structName);

		struc->symbol = oldAst->arg.varval;
		free(oldAst);
	}
	else
	{
		struc->symbol = addSymbol(code, strdup(structName));
	}

	struc->name = structName;
	struc->overloads = NULL;
	struc->size = 0;
	struc->data = NULL;
	consumec(code, '{');

	symbolScope_increase(code, 0, true);
	struct ptrs_structlist *curr = NULL;
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

			const char *nameFormat;
			const char *opLabel;
			char *otherName = NULL;
			ptrs_function_t *func = talloc(ptrs_function_t);
			func->argc = 0;
			func->argv = NULL;
			func->vararg.scope = (unsigned)-1;

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->isLeftSide = true;
			overload->isStatic = isStatic;

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
						if(lookahead(code, "=>"))
						{
							otherName = "any";
							consume(code, "any");

							func->argc = 1;
							func->args = talloc(ptrs_symbol_t);
							func->args[0] = addSymbol(code, strdup(".yield_algorithm"));

							nameFormat = "%1$s.op this => any";
							opLabel = "=>";
							overload->op = PTRS_HANDLE_ALGORITHM;
						}
						else
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
								char curr = code->curr;
								if(curr == '.' || curr == '[')
								{
									next(code);
									otherName = readIdentifier(code);

									if(curr == '.')
									{
										nameFormat = "%1$s.op this.%3$s%2$s";
									}
									else
									{
										nameFormat = "%1$s.op this[%3$s]%2$s";
										consumec(code, ']');
									}

									if(code->curr == '=')
									{
										consumec(code, '=');

										func->argc = 2;
										func->args = malloc(sizeof(ptrs_symbol_t) * 2);
										func->args[0] = addSymbol(code, otherName);
										func->args[1] = addSymbol(code, readIdentifier(code));

										opLabel = " = value";
										overload->op = curr == '.' ? PTRS_HANDLE_ASSIGN_MEMBER : PTRS_HANDLE_ASSIGN_INDEX;
									}
									else if(code->curr == '(')
									{
										func->argc = parseArgumentDefinitionList(code,
											&func->args, &func->argv, &func->vararg);
										func->argc++;

										opLabel = "()";
										overload->op = curr == '.' ? PTRS_HANDLE_CALL_MEMBER : PTRS_HANDLE_CALL_INDEX;


										ptrs_symbol_t *newArgs = malloc(sizeof(ptrs_symbol_t) * func->argc);
										newArgs[0] = addSymbol(code, otherName);

										memcpy(newArgs + 1, func->args, sizeof(ptrs_symbol_t) * (func->argc - 1));
										free(func->args);
										func->args = newArgs;


										if(func->argv != NULL)
										{
											ptrs_ast_t **newArgv = malloc(sizeof(ptrs_symbol_t) * func->argc);
											newArgv[0] = NULL;

											memcpy(newArgv + 1, func->argv, sizeof(ptrs_symbol_t) * (func->argc - 1));
											free(func->argv);
											func->argv = newArgv;
										}
									}
									else
									{
										func->args = talloc(ptrs_symbol_t);
										func->args[0] = addSymbol(code, otherName);

										opLabel = "";
										overload->op = curr == '.' ? PTRS_HANDLE_MEMBER : PTRS_HANDLE_INDEX;
									}
								}
								else if(curr == '(')
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
				}
				else if(lookahead(code, "foreach"))
				{
					consume(code, "in");
					consume(code, "this");
					nameFormat = "%1$s.op foreach in this";
					overload->op = PTRS_HANDLE_FORIN;

					func->argc = 1;
					func->args = talloc(ptrs_symbol_t);
					func->args[0] = addSymbol(code, strdup(".yield"));
				}
				else if(lookahead(code, "cast"))
				{
					consumec(code, '<');
					otherName = readIdentifier(code);
					consumec(code, '>');
					consume(code, "this");

					nameFormat = "%1$s.op cast<%3$s>this";
					overload->op = PTRS_HANDLE_CAST_BUILTIN;

					func->argc = 1;
					func->args = talloc(ptrs_symbol_t);
					func->args[0] = addSymbol(code, otherName);
				}
				else
				{
					otherName = readIdentifier(code);

					if(lookahead(code, "=>"))
					{
						consume(code, "this");

						if(lookahead(code, "=>"))
						{
							nameFormat = "%1$s.op %3$s => this => any";
							consume(code, "any");

							func->argc = 2;
							func->args = malloc(sizeof(ptrs_symbol_t) * 2);
							func->args[0] = addSymbol(code, strdup(".yield_algorithm"));
							func->args[1] = addSymbol(code, otherName);

							overload->isLeftSide = false;
							overload->op = PTRS_HANDLE_YIELD_ALGORITHM;
						}
						else
						{
							nameFormat = "%1$s.op %3$s => this";

							func->argc = 1;
							func->args = talloc(ptrs_symbol_t);
							func->args[0] = addSymbol(code, otherName);

							overload->isLeftSide = false;
							overload->op = PTRS_HANDLE_ALGORITHM;
						}
					}
					else
					{
						func->argc = 1;
						func->args = talloc(ptrs_symbol_t);
						func->args[0] = addSymbol(code, otherName);

						overload->isLeftSide = false;
						overload->op = readBinaryOperator(code, &opLabel);

						nameFormat = "%1$s.op %3$s %2$s this";
						consume(code, "this");
					}
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

		uint8_t isProperty = 0;
		if(lookahead(code, "get"))
			isProperty = 1;
		else if(lookahead(code, "set"))
			isProperty = 2;

		struct ptrs_structlist *old = curr;
		curr = talloc(struct ptrs_structlist);

		if(old == NULL)
			struc->member = curr;
		else
			old->next = curr;

		if(lookahead(code, "private"))
			curr->isPrivate = true;
		else
			curr->isPrivate = false;

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

		name = curr->name = readIdentifier(code);

		struct symbollist *symbol = addSpecialSymbol(code, strdup(curr->name), structMemberSymbolCreator);
		symbol->arg.data = curr;

		if(isProperty > 0)
		{
			symbolScope_increase(code, 1, false);
			setSymbol(code, strdup("this"), 0);

			ptrs_function_t *func = talloc(ptrs_function_t);
			func->name = malloc(structNameLen + strlen(name) + 6);
			func->vararg.scope = (unsigned)-1;

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
				func->args = talloc(ptrs_symbol_t);
				func->args[0] = addSymbol(code, strdup("value"));
				sprintf(func->name, "%s.set %s", structName, name);
			}

			func->body = parseBody(code, &func->stackOffset, false, true);
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
			ptrs_ast_t *ast = parseExpression(code);
			consumec(code, ']');
			consumec(code, ';');

			if(ast->handler != PTRS_HANDLE_CONSTANT)
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
			ptrs_ast_t *ast = parseExpression(code);
			consumec(code, '}');
			consumec(code, ';');

			if(ast->handler != PTRS_HANDLE_CONSTANT)
				PTRS_HANDLE_ASTERROR(ast, "Struct array member size must be a constant");

			curr->value.size = ast->arg.constval.value.intval;
			curr->offset = currSize;
			currSize += curr->value.size;
			free(ast);
		}
		else if(code->curr == ':')
		{
			consumec(code, ':');
			ptrs_nativetype_info_t *type = readNativeType(code);
			consumec(code, ';');

			curr->type = PTRS_STRUCTMEMBER_TYPED;
			curr->value.type = type;

			if(old != NULL && old->type == PTRS_STRUCTMEMBER_TYPED && old->isStatic == curr->isStatic)
			{
				if(old->value.type->size < type->size)
					curr->offset = (old->offset & ~(type->size - 1)) + type->size;
				else
					curr->offset = old->offset + old->value.type->size;
				currSize = (curr->offset & ~7) + 8;
			}
			else
			{
				curr->offset = currSize;
				currSize += 8;
			}
		}
		else
		{
			curr->type = PTRS_STRUCTMEMBER_VAR;
			curr->offset = currSize;
			currSize += sizeof(ptrs_var_t);

			if(lookahead(code, "="))
				curr->value.startval = parseExpression(code);
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

	if(curr == NULL)
		struc->member = NULL;
	else
		curr->next = NULL;
	symbolScope_decrease(code);
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

static ptrs_nativetype_info_t nativeTypes[] = {
	{"char", PTRS_CTYPE_CHAR, sizeof(signed char), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"short", PTRS_CTYPE_SHORT, sizeof(short), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"int", PTRS_CTYPE_INT, sizeof(int), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"long", PTRS_CTYPE_LONG, sizeof(long), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"longlong", PTRS_CTYPE_LONGLONG, sizeof(long long), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},

	{"uchar", PTRS_CTYPE_UCHAR, sizeof(unsigned char), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"ushort", PTRS_CTYPE_USHORT, sizeof(unsigned short), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"uint", PTRS_CTYPE_UINT, sizeof(unsigned int), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"ulong", PTRS_CTYPE_ULONG, sizeof(unsigned long), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"ulonglong", PTRS_CTYPE_ULONGLONG, sizeof(unsigned long long), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},

	{"i8", PTRS_CTYPE_I8, sizeof(int8_t), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"i16", PTRS_CTYPE_I16, sizeof(int16_t), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"i32", PTRS_CTYPE_I32, sizeof(int32_t), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},
	{"i64", PTRS_CTYPE_I64, sizeof(int64_t), PTRS_HANDLE_NATIVE_GETINT, PTRS_HANDLE_NATIVE_SETINT},

	{"u8", PTRS_CTYPE_U8, sizeof(uint8_t), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"u16", PTRS_CTYPE_U16, sizeof(uint16_t), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"u32", PTRS_CTYPE_U32, sizeof(uint32_t), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},
	{"u64", PTRS_CTYPE_U64, sizeof(uint64_t), PTRS_HANDLE_NATIVE_GETUINT, PTRS_HANDLE_NATIVE_SETUINT},

	{"single", PTRS_CTYPE_SINGLE, sizeof(float), PTRS_HANDLE_NATIVE_GETFLOAT, PTRS_HANDLE_NATIVE_SETFLOAT},
	{"double", PTRS_CTYPE_DOUBLE, sizeof(double), PTRS_HANDLE_NATIVE_GETFLOAT, PTRS_HANDLE_NATIVE_SETFLOAT},

	{"native", PTRS_CTYPE_NATIVE, sizeof(char *), PTRS_HANDLE_NATIVE_GETNATIVE, PTRS_HANDLE_NATIVE_SETNATIVE},
	{"pointer", PTRS_CTYPE_POINTER, sizeof(ptrs_var_t *), PTRS_HANDLE_NATIVE_GETPOINTER, PTRS_HANDLE_NATIVE_SETPOINTER},
};
static int nativeTypeCount = sizeof(nativeTypes) / sizeof(ptrs_nativetype_info_t);

static ptrs_nativetype_info_t *readNativeType(code_t *code)
{
	for(int i = 0; i < nativeTypeCount; i++)
	{
		if(lookahead(code, nativeTypes[i].name))
			return &nativeTypes[i];
	}
	return NULL;
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

	if(length != NULL)
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

static ptrs_ast_t *defaultSymbolCreator(unsigned scopeLevel, struct symbollist *entry)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = PTRS_HANDLE_IDENTIFIER;
	ast->setHandler = PTRS_HANDLE_ASSIGN_IDENTIFIER;
	ast->callHandler = NULL;

	ast->arg.varval.scope = scopeLevel;
	ast->arg.varval.offset = entry->arg.offset;

	return ast;
}

static ptrs_ast_t *constSymbolCreator(unsigned scopeLevel, struct symbollist *entry)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	memcpy(ast, entry->arg.data, sizeof(ptrs_ast_t));
	return ast;
}

static ptrs_ast_t *structMemberSymbolCreator(unsigned scopeLevel, struct symbollist *entry)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = PTRS_HANDLE_THISMEMBER;
	ast->setHandler = PTRS_HANDLE_ASSIGN_THISMEMBER;
	ast->callHandler = PTRS_HANDLE_CALL_THISMEMBER;

	ast->arg.thismember.base.scope = scopeLevel - 1;
	ast->arg.thismember.base.offset = 0;
	ast->arg.thismember.member = entry->arg.data;

	return ast;
}

static void setSymbol(code_t *code, char *text, unsigned offset)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->creator = defaultSymbolCreator;
	entry->text = text;
	entry->arg.offset = offset;
	entry->next = curr->current;
	curr->current = entry;
}

static ptrs_symbol_t addSymbol(code_t *code, char *symbol)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->creator = defaultSymbolCreator;
	entry->text = symbol;
	entry->next = curr->current;
	entry->arg.offset = curr->offset;
	curr->offset += sizeof(ptrs_var_t);
	curr->current = entry;

	ptrs_symbol_t result = {0, entry->arg.offset};
	return result;
}

static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, symbolcreator_t creator)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->text = symbol;
	entry->creator = creator;
	entry->next = curr->current;
	curr->current = entry;

	return entry;
}

static ptrs_ast_t *getSymbol(code_t *code, char *text)
{
	ptrs_ast_t *ast = NULL;
	if(ptrs_ast_getSymbol(code->symbols, text, &ast) == 0)
		return ast;

	char buff[128];
	sprintf(buff, "Unknown identifier %s", text);
	unexpectedm(code, NULL, buff);
	return ast; //doh
}

static void symbolScope_increase(code_t *code, int buildInCount, bool isInline)
{
	ptrs_symboltable_t *new = talloc(ptrs_symboltable_t);
	new->offset = buildInCount * sizeof(ptrs_var_t);
	new->maxOffset = 0;
	new->isInline = isInline;
	new->outer = code->symbols;
	new->current = NULL;

	code->symbols = new;
	if(isInline)
		new->offset += new->outer->offset;
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

	unsigned val = scope->offset > scope->maxOffset ? scope->offset : scope->maxOffset;
	if(code->symbols != NULL && val > code->symbols->maxOffset)
		code->symbols->maxOffset = val;

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
