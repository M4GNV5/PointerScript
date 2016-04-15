#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "../interpreter/interpreter.h"
#include "ast.h"

#define talloc(type) malloc(sizeof(type))

typedef struct code
{
	char *src;
	char curr;
	int pos;
} code_t;

typedef struct codepos
{
	int line;
	int column;
} codepos_t;

ptrs_ast_t *parseStmtList(code_t *code, char end);
ptrs_ast_t *parseStatement(code_t *code);
ptrs_ast_t *parseExpression(code_t *code);
ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
ptrs_ast_t *parseUnaryExpr(code_t *code);
ptrs_ast_t *parseUnary(code_t *code);
struct ptrs_astlist *parseExpressionList(code_t *code, char end);

ptrs_vartype_t readTypeName(code_t *code);
char *readIdentifier(code_t *code);
char *readString(code_t *code);
char readEscapeSequence(code_t *code);
int64_t readInt(code_t *code, int base);
double readDouble(code_t *code);

bool lookahead(code_t *code, const char *str);
void consume(code_t *code, const char *str);
void consumec(code_t *code, char c);
void next(code_t *code);
void rawnext(code_t *code);

bool skipSpaces(code_t *code);
bool skipComments(code_t *code);

void getPos(code_t *code, codepos_t *pos);
void unexpected(code_t *code, const char *expected);

ptrs_ast_t *parse(char *src)
{
	code_t code;
	code.src = src;
	code.pos = -1;
	next(&code);

	return parseStmtList(&code, 0);
}

ptrs_ast_t *parseStmtList(code_t *code, char end)
{
	ptrs_ast_t *elem = talloc(ptrs_ast_t);
	elem->handler = PTRS_HANDLE_BODY;
	if(code->curr == end || code->curr == 0)
	{
		next(code);
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

ptrs_ast_t *parseStatement(code_t *code)
{
	ptrs_ast_t *stmt = talloc(ptrs_ast_t);
	stmt->codepos = code->pos;
	stmt->code = code->src;

	if(lookahead(code, "var"))
	{
		stmt->arg.define.name = readIdentifier(code);
		stmt->handler = PTRS_HANDLE_DEFINE;

		if(lookahead(code, "="))
			stmt->arg.define.value = parseExpression(code);
		else
			stmt->arg.define.value = NULL;

		consumec(code, ';');
	}
	else if(lookahead(code, "import"))
	{
		stmt->handler = PTRS_HANDLE_IMPORT;
		stmt->arg.import.fields = parseExpressionList(code, 0);

		if(lookahead(code, "from"))
			stmt->arg.import.from = parseExpression(code);
		else
			stmt->arg.import.from = NULL;

		consumec(code, ';');
	}
	else if(lookahead(code, "function"))
	{
		stmt->handler = PTRS_HANDLE_FUNCTION;
		stmt->arg.function.name = readIdentifier(code);

		int pos = code->pos;

		int argc = 0;
		while(code->curr != ')')
		{
			next(code);
			argc++;
			while(code->curr != ')' && code->curr != ',')
				next(code);
		}

		char **args = malloc(sizeof(char *) * argc);
		code->pos = pos;
		code->curr = code->src[pos];

		consumec(code, '(');
		for(int i = 0; i < argc; i++)
		{
			args[i] = readIdentifier(code);

			if(i != argc - 1)
				consumec(code, ',');
		}
		consumec(code, ')');

		stmt->arg.function.argc = argc;
		stmt->arg.function.args = args;

		consumec(code, '{');
		stmt->arg.function.body = parseStmtList(code, '}');
		consumec(code, '}');
	}
	else if(lookahead(code, "if"))
	{
		stmt->handler = PTRS_HANDLE_IF;

		consumec(code, '(');
		stmt->arg.ifelse.condition = parseExpression(code);
		consumec(code, ')');

		if(lookahead(code, "{"))
		{
			stmt->arg.ifelse.ifBody = parseStmtList(code, '}');
			consumec(code, '}');
		}
		else
		{
			stmt->arg.ifelse.ifBody = parseStatement(code);
		}

		stmt->arg.ifelse.elseBody = NULL;
		if(lookahead(code, "else"))
		{
			if(lookahead(code, "{"))
			{
				stmt->arg.ifelse.elseBody = parseStmtList(code, '}');
				consumec(code, '}');
			}
			else
			{
				stmt->arg.ifelse.elseBody = parseStatement(code);
			}
		}
	}
	else if(lookahead(code, "while"))
	{
		stmt->handler = PTRS_HANDLE_WHILE;

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');

		if(lookahead(code, "{"))
		{
			stmt->arg.control.body = parseStmtList(code, '}');
			consumec(code, '}');
		}
		else
		{
			stmt->arg.control.body = parseStatement(code);
		}
	}
	else if(lookahead(code, "do"))
	{
		stmt->handler = PTRS_HANDLE_DOWHILE;

		if(lookahead(code, "{"))
		{
			stmt->arg.control.body = parseStmtList(code, '}');
			consumec(code, '}');
		}
		else
		{
			stmt->arg.control.body = parseStatement(code);
		}

		consume(code, "while");

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');
		consumec(code, ';');
	}
	else if(lookahead(code, "for"))
	{
		stmt->handler = PTRS_HANDLE_FOR;

		consumec(code, '(');
		stmt->arg.forstatement.init = parseStatement(code);
		stmt->arg.forstatement.condition = parseExpression(code);
		consumec(code, ';');
		stmt->arg.forstatement.step = parseExpression(code);
		consumec(code, ')');

		if(lookahead(code, "{"))
		{
			stmt->arg.forstatement.body = parseStmtList(code, '}');
			consumec(code, '}');
		}
		else
		{
			stmt->arg.forstatement.body = parseStatement(code);
		}
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

	// 2 = ternary TODO

	{"||", 3, false, PTRS_HANDLE_OP_LOGICOR},
	{"&&", 4, false, PTRS_HANDLE_OP_LOGICAND},

	{"|", 5, false, PTRS_HANDLE_OP_OR},
	{"^", 6, false, PTRS_HANDLE_OP_XOR},
	{"&", 7, false, PTRS_HANDLE_OP_AND},

	{">>", 9, false, PTRS_HANDLE_OP_SHR}, //shift right
	{"<<", 9, false, PTRS_HANDLE_OP_SHL}, //shift left

	{"+", 10, false, PTRS_HANDLE_OP_ADD},
	{"-", 10, false, PTRS_HANDLE_OP_SUB},

	{"*", 11, false, PTRS_HANDLE_OP_MUL},
	{"/", 11, false, PTRS_HANDLE_OP_DIV},
	{"%", 11, false, PTRS_HANDLE_OP_MOD}
};
int binaryOpCount = sizeof(binaryOps) / sizeof(struct opinfo);

struct opinfo prefixOps[] = {
	{"typeof", 12, true, PTRS_HANDLE_OP_TYPEOF},
	{"++", 12, true, PTRS_HANDLE_PREFIX_INC}, //prefixed ++
	{"--", 12, true, PTRS_HANDLE_PREFIX_DEC}, //prefixed --
	{"!", 12, true, PTRS_HANDLE_PREFIX_LOGICNOT}, //logical NOT
	{"~", 12, true, PTRS_HANDLE_PREFIX_NOT}, //bitwise NOT
	{"&", 12, true, PTRS_HANDLE_PREFIX_ADDRESS}, //adress of
	{"*", 12, true, PTRS_HANDLE_PREFIX_DEREFERENCE}, //dereference
	{"+", 12, true, PTRS_HANDLE_PREFIX_PLUS}, //unary +
	{"-", 12, true, PTRS_HANDLE_PREFIX_MINUS} //unary -
};
int prefixOpCount = sizeof(prefixOps) / sizeof(struct opinfo);

struct opinfo suffixedOps[] = {
	{"++", 13, false, PTRS_HANDLE_SUFFIX_INC}, //suffixed ++
	{"--", 13, false, PTRS_HANDLE_SUFFIX_DEC} //suffixed --
};
int suffixedOpCount = sizeof(suffixedOps) / sizeof(struct opinfo);

ptrs_ast_t *parseExpression(code_t *code)
{
	return parseBinaryExpr(code, parseUnaryExpr(code), 0);;
}

struct opinfo *peekBinaryOp(code_t *code)
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
ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec)
{
	struct opinfo *ahead = peekBinaryOp(code);
	while(ahead != NULL && ahead->precendence >= minPrec)
	{
		struct opinfo *op = ahead;
		int pos = code->pos;
		consume(code, ahead->op);
		ptrs_ast_t *right = parseUnaryExpr(code);
		ahead = peekBinaryOp(code);

		while(ahead != NULL && ahead->precendence > op->precendence)
		{
			right = parseBinaryExpr(code, right, ahead->precendence);
			ahead = peekBinaryOp(code);
		}

		ptrs_ast_t *_left = left;
		left = talloc(ptrs_ast_t);
		left->handler = op->handler;
		left->arg.binary.left = _left;
		left->arg.binary.right = right;
		left->codepos = pos;
		left->code = code->src;
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
	{"true", PTRS_TYPE_INT, {true}},
	{"false", PTRS_TYPE_INT, {false}},
	{"NULL", PTRS_TYPE_NATIVE, {(int64_t)NULL}},
	{"null", PTRS_TYPE_POINTER, {(int64_t)NULL}},
	{"VARSIZE", PTRS_TYPE_INT, {sizeof(ptrs_var_t)}},
	{"undefined", PTRS_TYPE_UNDEFINED, {42}}
};
int constantCount = sizeof(constants) / sizeof(struct constinfo);

ptrs_ast_t *parseUnaryExpr(code_t *code)
{
	char curr = code->curr;
	int pos = code->pos;
	ptrs_ast_t *ast;

	for(int i = 0; i < prefixOpCount; i++)
	{
		if(lookahead(code, prefixOps[i].op))
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.astval = parseUnaryExpr(code);
			ast->handler = prefixOps[i].handler;
			ast->codepos = pos;
			ast->code = code->src;
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

	if(isalpha(curr) || curr == '_')
	{
		ast = talloc(ptrs_ast_t);
		ast->arg.strval = readIdentifier(code);
		ast->handler = PTRS_HANDLE_IDENTIFIER;
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
		}
	}
	else if(curr == '\'')
	{
		rawnext(code);
		ast = talloc(ptrs_ast_t);
		ast->handler = PTRS_HANDLE_CONSTANT;
		ast->arg.constval.type = PTRS_TYPE_INT;

		if(curr == '\\')
		{
			rawnext(code);
			ast->arg.constval.value.intval = readEscapeSequence(code);
		}
		else
		{
			ast->arg.constval.value.intval = code->curr;
		}
		rawnext(code);
		consumec(code, '\'');
	}
	else if(curr == '"')
	{
		rawnext(code);
		ast = talloc(ptrs_ast_t);
		ast->arg.constval.type = PTRS_TYPE_STRING;
		ast->arg.constval.value.strval = readString(code);
		ast->handler = PTRS_HANDLE_CONSTANT;
		consumec(code, '"');
	}
	else if(curr == '(')
	{
		consumec(code, '(');
		ptrs_vartype_t type = readTypeName(code);

		if(lookahead(code, ")") && type != PTRS_TYPE_UNDEFINED)
		{
			ast = talloc(ptrs_ast_t);
			ast->handler = PTRS_HANDLE_CAST;
			ast->arg.cast.type = type;
			ast->arg.cast.value = parseUnaryExpr(code);
		}
		else
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
	curr = code->curr;
	if(curr == '(')
	{
		ptrs_ast_t *call = talloc(ptrs_ast_t);
		call->handler = PTRS_HANDLE_CALL;
		call->codepos = code->pos;
		call->code = code->src;
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
		consumec(code, '[');

		indexExpr->arg.binary.left = ast;
		indexExpr->arg.binary.right = parseExpression(code);

		ast = indexExpr;
		consumec(code, ']');
	}

	pos = code->pos;
	for(int i = 0; i < suffixedOpCount; i++)
	{
		if(lookahead(code, suffixedOps[i].op))
		{
			ptrs_ast_t *opAst = talloc(ptrs_ast_t);
			opAst->codepos = pos;
			opAst->code = code->src;
			opAst->arg.astval = ast;
			opAst->handler = suffixedOps[i].handler;
			return opAst;
		}
	}

	return ast;
}

struct ptrs_astlist *parseExpressionList(code_t *code, char end)
{
	if(code->curr == end || code->curr == 0)
		return NULL;

	struct ptrs_astlist *curr = talloc(struct ptrs_astlist);
	struct ptrs_astlist *first = curr;

	for(;;)
	{
		curr->entry = parseExpression(code);

		if(!lookahead(code, ","))
			break;

		struct ptrs_astlist *next = talloc(struct ptrs_astlist);
		curr->next = next;
		curr = next;
	}

	curr->next = NULL;
	return first;
}

struct typeName
{
	const char *name;
	ptrs_vartype_t type;
};
struct typeName typeNames[] = {
	{"int", PTRS_TYPE_INT},
	{"float", PTRS_TYPE_FLOAT},
	{"native", PTRS_TYPE_NATIVE},
	{"string", PTRS_TYPE_STRING},
	{"pointer", PTRS_TYPE_POINTER},
	{"object", PTRS_TYPE_OBJECT}
};
int typeNameCount = sizeof(typeNames) / sizeof(struct typeName);

ptrs_vartype_t readTypeName(code_t *code)
{
	ptrs_vartype_t type = PTRS_TYPE_UNDEFINED;
	for(int i = 0; i < typeNameCount; i++)
	{
		if(lookahead(code, typeNames[i].name))
		{
			type = typeNames[i].type;
			break;
		}
	}

	return type;
}

char *readIdentifier(code_t *code)
{
	char val[128];
	int i = 0;

	while(isalpha(code->curr) || code->curr == '_')
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

char *readString(code_t *code)
{
	char val[1024];
	int i = 0;

	while(code->curr != '"')
	{
		if(code->curr == '\\')
		{
			rawnext(code);
			val[i] = readEscapeSequence(code);
		}
		else
		{
			val[i] = code->curr;
		}
		i++;
		rawnext(code);
	}

	val[i] = 0;
	char *_val = malloc(i + 1);
	strcpy(_val, val);

	return _val;
}

char readEscapeSequence(code_t *code)
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
	unexpected(code, "escape sequence");
	return 0;
}

int64_t readInt(code_t *code, int base)
{
	char *start = &code->src[code->pos];
	char *end;

	int64_t val = strtol(start, &end, base);
	code->pos += end - start - 1;
	next(code);

	return val;
}
double readDouble(code_t *code)
{
	char *start = &code->src[code->pos];
	char *end;

	double val = strtod(start, &end);
	code->pos += end - start - 1;
	next(code);

	return val;
}

bool lookahead(code_t *code, const char *str)
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

	skipSpaces(code);
	return true;
}

void consume(code_t *code, const char *str)
{
	while(*str)
	{
		if(code->curr != *str)
			unexpected(code, str);
		str++;
		next(code);
	}
}
void consumec(code_t *code, char c)
{
	if(code->curr != c)
	{
		char expected[] = {c, 0};
		unexpected(code, expected);
	}
	next(code);
}

bool skipSpaces(code_t *code)
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
bool skipComments(code_t *code)
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

	return false;
}
void next(code_t *code)
{
	code->pos++;
	while(skipSpaces(code) || skipComments(code));
	code->curr = code->src[code->pos];
}
void rawnext(code_t *code)
{
	code->pos++;
	code->curr = code->src[code->pos];
}

void getPos(code_t *code, codepos_t *pos)
{
	pos->line = 1;
	pos->column = 1;
	for(int i = 0; i < code->pos; i++)
	{
		char curr = code->src[i];
		if(curr == '\n')
		{
			pos->line++;
			pos->column = 1;
		}
		else
		{
			pos->column++;
		}
	}
}

void unexpected(code_t *code, const char *expected)
{
	codepos_t pos;
	getPos(code, &pos);

	fprintf(stderr, "Expecting %s got '%c' at line %d column %d\n", expected, code->curr, pos.line, pos.column);
	exit(1);
}
