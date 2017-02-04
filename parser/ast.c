#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include <ffi.h>

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

typedef enum
{
	PTRS_SYMBOL_DEFAULT,
	PTRS_SYMBOL_CONST,
	PTRS_SYMBOL_LAZY,
	PTRS_SYMBOL_THISMEMBER,
	PTRS_SYMBOL_WILDCARD,
} ptrs_symboltype_t;

struct symbollist
{
	union
	{
		unsigned offset;
		void *data;
		struct
		{
			unsigned index;
			unsigned offset;
		} wildcard;
		struct
		{
			ptrs_ast_t *value;
			unsigned offset;
		} lazy;
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
	unsigned offset;
	unsigned maxOffset;
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
	int withCount;
	bool insideIndex;
	bool yieldIsAlgo;
	ptrs_symbol_t yield;
};

static ptrs_ast_t *parseStmtList(code_t *code, char end);
static ptrs_ast_t *parseStatement(code_t *code);
static ptrs_ast_t *parseExpression(code_t *code);
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls, bool ignoreAlgo);
static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls, bool ignoreAlgo);
static struct ptrs_astlist *parseExpressionList(code_t *code, char end);
static ptrs_ast_t *parseNew(code_t *code, bool onStack);
static void parseAsm(code_t *code, ptrs_ast_t *stmt);
static void parseMap(code_t *code, ptrs_ast_t *expr);
static void parseStruct(code_t *code, ptrs_struct_t *struc);
static void parseImport(code_t *code, ptrs_ast_t *stmt);
static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt);
static void parseAlgorithmExpression(code_t *code, struct ptrs_algorithmlist *curr, bool canBeLast);

static ptrs_vartype_t readTypeName(code_t *code);
static ptrs_nativetype_info_t *readNativeType(code_t *code);
static ptrs_asthandler_t readPrefixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readSuffixOperator(code_t *code, const char **label);
static ptrs_asthandler_t readBinaryOperator(code_t *code, const char **label);
static char *readIdentifier(code_t *code);
static char *readString(code_t *code, int *length, struct ptrs_stringformat **insertions, int *insertionsCount);
static char readEscapeSequence(code_t *code);
static int64_t readInt(code_t *code, int base);
static double readDouble(code_t *code);

static void setSymbol(code_t *code, char *text, unsigned offset);
static ptrs_symbol_t addSymbol(code_t *code, char *text);
static ptrs_symbol_t addHiddenSymbol(code_t *code, size_t size);
static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, ptrs_symboltype_t type);
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
	code.curr = src[0];
	code.pos = 0;
	code.symbols = NULL;
	code.withCount = 0;
	code.insideIndex = false;
	code.yieldIsAlgo = false;
	code.yield.scope = (unsigned)-1;

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

	symbolScope_increase(&code, 1, false);
	setSymbol(&code, strdup("arguments"), 0);

	ptrs_ast_t *ast = talloc(ptrs_ast_t);
	ast->handler = ptrs_handle_file;
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
				ptrs_ast_t *ast;
				switch(curr->type)
				{
					case PTRS_SYMBOL_DEFAULT:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_identifier;
						ast->setHandler = ptrs_handle_assign_identifier;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;

						ast->arg.varval.scope = level;
						ast->arg.varval.offset = curr->arg.offset;
						break;

					case PTRS_SYMBOL_CONST:
						*node = ast = talloc(ptrs_ast_t);
						memcpy(ast, curr->arg.data, sizeof(ptrs_ast_t));
						break;

					case PTRS_SYMBOL_LAZY:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_lazy;
						ast->setHandler = NULL;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;
						ast->arg.lazy.symbol.scope = level;
						ast->arg.lazy.symbol.offset = curr->arg.lazy.offset;
						ast->arg.lazy.value = curr->arg.lazy.value;
						break;

					case PTRS_SYMBOL_THISMEMBER:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_thismember;
						ast->setHandler = ptrs_handle_assign_thismember;
						ast->addressHandler = ptrs_handle_addressof_thismember;
						ast->callHandler = ptrs_handle_call_thismember;

						ast->arg.thismember.base.scope = level - 1;
						ast->arg.thismember.base.offset = 0;
						ast->arg.thismember.member = curr->arg.data;
						break;

					case PTRS_SYMBOL_WILDCARD:
						*node = ast = talloc(ptrs_ast_t);
						ast->handler = ptrs_handle_wildcardsymbol;
						ast->setHandler = NULL;
						ast->addressHandler = NULL;
						ast->callHandler = NULL;

						ast->arg.wildcard.index = curr->arg.wildcard.index;
						ast->arg.wildcard.symbol.scope = level;
						ast->arg.wildcard.symbol.offset = curr->arg.wildcard.offset;
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
			else if(lookahead(code, "lazy"))
			{
				struct symbollist *symbol = addSpecialSymbol(code, readIdentifier(code), PTRS_SYMBOL_LAZY);
				curr = addHiddenSymbol(code, sizeof(ptrs_var_t));
				symbol->arg.lazy.offset = curr.offset;
				symbol->arg.lazy.value = NULL;
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

	int offset = symbolScope_decrease(code);
	if(stackOffset != NULL)
		*stackOffset = offset;

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
	if(code->curr == '{')
	{
		return parseBody(code, NULL, false, false);
	}
	else if(lookahead(code, "const"))
	{
		char *name = readIdentifier(code);
		consumec(code, '=');
		ptrs_ast_t *ast = parseExpression(code);
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

	if(lookahead(code, "var"))
	{
		stmt->handler = ptrs_handle_define;
		stmt->arg.define.symbol = addSymbol(code, readIdentifier(code));
		stmt->arg.define.onStack = true;

		if(lookahead(code, "["))
		{
			stmt->handler = ptrs_handle_vararray;
			stmt->arg.define.value = parseExpression(code);
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
			stmt->arg.define.value = parseExpression(code);
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
					stmt->arg.define.initExpr = parseExpression(code);
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
			stmt->arg.define.value = parseExpression(code);
		}
		else
		{
			stmt->arg.define.value = NULL;
		}

		consumec(code, ';');
	}
	else if(lookahead(code, "lazy"))
	{
		stmt->handler = ptrs_handle_lazyinit;
		stmt->arg.varval = addHiddenSymbol(code, sizeof(ptrs_var_t));

		struct symbollist *entry = addSpecialSymbol(code, readIdentifier(code), PTRS_SYMBOL_LAZY);
		consumec(code, '=');
		entry->arg.lazy.offset = stmt->arg.varval.offset;
		entry->arg.lazy.value = parseExpression(code);
		consumec(code, ';');

		if(entry->arg.lazy.value == NULL)
			unexpected(code, "Expression");
	}
	else if(lookahead(code, "import"))
	{
		parseImport(code, stmt);
	}
	else if(lookahead(code, "return"))
	{
		stmt->handler = ptrs_handle_return;
		stmt->arg.astval = parseExpression(code);
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
		stmt->arg.astval = parseExpression(code);
		consumec(code, ';');
	}
	else if(lookahead(code, "throw"))
	{
		stmt->handler = ptrs_handle_throw;
		stmt->arg.astval = parseExpression(code);
		consumec(code, ';');
	}
	else if(lookahead(code, "scoped"))
	{
		stmt->handler = ptrs_handle_scopestatement;
		symbolScope_increase(code, 0, false);
		stmt->arg.scopestatement.body = parseBody(code, &stmt->arg.scopestatement.stackOffset, true, true);
	}
	else if(lookahead(code, "try"))
	{
		stmt->handler = ptrs_handle_trycatch;
		stmt->arg.trycatch.tryBody = parseBody(code, NULL, true, false);

		if(lookahead(code, "catch"))
		{
			symbolScope_increase(code, 0, false);
			stmt->arg.trycatch.argc = parseArgumentDefinitionList(code, &stmt->arg.trycatch.args, NULL, NULL);
			stmt->arg.trycatch.catchBody = parseScopelessBody(code, true);
			stmt->arg.trycatch.catchStackOffset = symbolScope_decrease(code);
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
				stmt->arg.trycatch.retVal = addSymbol(code, readIdentifier(code));
				consumec(code, ')');
			}
			else
			{
				stmt->arg.trycatch.retVal.scope = (unsigned)-1;
			}

			stmt->arg.trycatch.finallyBody = parseBody(code, NULL, true, false);
			symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.retVal.scope = (unsigned)-1;
			stmt->arg.trycatch.finallyBody = NULL;
		}
	}
	else if(lookahead(code, "asm"))
	{
#ifndef _PTRS_NOASM
		stmt->handler = ptrs_handle_asm;
		parseAsm(code, stmt);
#else
		PTRS_HANDLE_ASTERROR(stmt, "Inline assembly is not available");
#endif
	}
	else if(lookahead(code, "function"))
	{
		stmt->handler = ptrs_handle_function;
		stmt->arg.function.isAnonymous = false;

		ptrs_function_t *func = &stmt->arg.function.func;
		func->scope = NULL;
		func->nativeCb = NULL;

		char *name = readIdentifier(code);
		ptrs_ast_t *oldAst;
		if(ptrs_ast_getSymbol(code->symbols, name, &oldAst) == 0)
		{
			if(oldAst->handler != ptrs_handle_identifier)
				PTRS_HANDLE_ASTERROR(stmt, "Cannot redefine special symbol %s as a function", name);

			stmt->arg.function.symbol = oldAst->arg.varval;
			free(oldAst);
		}
		else
		{
			stmt->arg.function.symbol = addSymbol(code, strdup(name));
		}
		func->name = name;

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

		func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
		func->body = parseBody(code, &func->stackOffset, false, true);
	}
	else if(lookahead(code, "struct"))
	{
		stmt->handler = ptrs_handle_struct;
		parseStruct(code, &stmt->arg.structval);
	}
	else if(lookahead(code, "with"))
	{
		int oldCount = code->withCount;
		code->withCount = 0;

		stmt->handler = ptrs_handle_with;

		consumec(code, '(');
		stmt->arg.with.base = parseExpression(code);
		consumec(code, ')');

		stmt->arg.with.symbol = addSymbol(code, strdup(".with"));
		stmt->arg.with.body = parseBody(code, NULL, true, false);
		stmt->arg.with.count = code->withCount;
		stmt->arg.with.memberBuff = code->symbols->offset;

		code->symbols->offset += sizeof(void *) * code->withCount;
		code->withCount = oldCount;
	}
	else if(lookahead(code, "if"))
	{
		stmt->handler = ptrs_handle_if;

		consumec(code, '(');
		stmt->arg.ifelse.condition = parseExpression(code);
		consumec(code, ')');
		stmt->arg.ifelse.ifBody = parseBody(code, NULL, true, false);

		if(lookahead(code, "else"))
			stmt->arg.ifelse.elseBody = parseBody(code, NULL, true, false);
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
		stmt->arg.control.condition = parseExpression(code);
		consumec(code, ')');
		stmt->arg.control.body = parseBody(code, NULL, true, false);
	}
	else if(lookahead(code, "do"))
	{
		symbolScope_increase(code, 0, true);
		stmt->handler = ptrs_handle_dowhile;
		stmt->arg.control.body = parseScopelessBody(code, true);
		consume(code, "while");

		consumec(code, '(');
		stmt->arg.control.condition = parseExpression(code);
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
		stmt->arg.forin.value = parseExpression(code);
		stmt->handler = ptrs_handle_forin;
		consumec(code, ')');

		stmt->arg.forin.body = parseScopelessBody(code, true);
		stmt->arg.forin.stackOffset = symbolScope_decrease(code);
	}
	else if(lookahead(code, "for"))
	{
		stmt->handler = ptrs_handle_for;
		symbolScope_increase(code, 0, true);

		consumec(code, '(');
		stmt->arg.forstatement.init = parseStatement(code);
		stmt->arg.forstatement.condition = parseExpression(code);
		consumec(code, ';');
		stmt->arg.forstatement.step = parseExpression(code);
		consumec(code, ')');

		stmt->arg.forstatement.body = parseScopelessBody(code, true);
		symbolScope_decrease(code);
	}
	else
	{
		stmt->handler = ptrs_handle_exprstatement;
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
		if(left->handler == ptrs_handle_constant && right->handler == ptrs_handle_constant)
		{
			ptrs_var_t result;
			ptrs_ast_t node;

			node.handler = op->handler;
			node.arg.binary.left = left;
			node.arg.binary.right = right;
			node.file = code->filename;
			node.code = code->src;
			node.codepos = pos;

			ptrs_lastast = &node;
			ptrs_lastscope = NULL;

			node.handler(&node, &result, NULL);
			free(right);
			memcpy(&left->arg.constval, &result, sizeof(ptrs_var_t));
			continue;
		}
#endif
		if(op->handler == ptrs_handle_op_ternary)
		{
			ptrs_ast_t *ast = talloc(ptrs_ast_t);
			ast->handler = ptrs_handle_op_ternary;
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
	ptrs_ast_t *ast;

	for(int i = 0; i < prefixOpCount; i++)
	{
		if(lookahead(code, prefixOps[i].op))
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.astval = parseUnaryExpr(code, false, true);
			ast->handler = prefixOps[i].handler;
			ast->addressHandler = NULL;
			ast->callHandler = NULL;
			if(ast->handler == ptrs_handle_prefix_dereference)
				ast->setHandler = ptrs_handle_assign_dereference;
			else
				ast->setHandler = NULL;

			ast->codepos = pos;
			ast->code = code->src;
			ast->file = code->filename;

#ifndef PTRS_DISABLE_CONSTRESOLVE
			if(ast->arg.astval->handler == ptrs_handle_constant)
			{
				ptrs_lastast = ast;
				ptrs_lastscope = NULL;

				ptrs_var_t result;
				ast->handler(ast, &result, NULL);
				ast->handler = ptrs_handle_constant;

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
			memset(&ast->arg.constval.meta, 0, sizeof(ptrs_meta_t));
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
		ast->arg.constval.type = PTRS_TYPE_INT;
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
			ast->arg.constval.type = PTRS_TYPE_INT;
			ast->arg.constval.value.intval = nativeType->size;
		}
		else if(lookahead(code, "var"))
		{
			if(hasBrace)
				consumec(code, ')');

			ast->handler = ptrs_handle_constant;
			ast->arg.constval.type = PTRS_TYPE_INT;
			ast->arg.constval.value.intval = sizeof(ptrs_var_t);
		}
		else
		{
			if(hasBrace)
			{
				code->pos = pos;
				code->curr = code->src[pos];
			}

			ast->handler = ptrs_handle_prefix_length;
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
		if(code->yield.scope != (unsigned)-1)
		{
			ast = talloc(ptrs_ast_t);
			ast->arg.yield.yieldVal = code->yield;
			if(code->yieldIsAlgo)
			{
				ast->handler = ptrs_handle_yield_algorithm;
				ast->arg.yield.value = parseExpression(code);
			}
			else
			{
				ast->handler = ptrs_handle_yield;
				ast->arg.yield.values = parseExpressionList(code, ';');
			}
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
		func->scope = NULL;
		func->nativeCb = NULL;

		symbolScope_increase(code, 1, false);
		setSymbol(code, strdup("this"), 0);

		func->argc = parseArgumentDefinitionList(code, &func->args, &func->argv, &func->vararg);
		func->body = parseBody(code, &func->stackOffset, false, true);
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
	else if(curr == '.' && (code->src[pos + 1] == '_' || isalpha(code->src[pos + 1])))
	{
		if(ptrs_ast_getSymbol(code->symbols, ".with", &ast) == 0)
		{
			rawnext(code);
			noSetHandler = false;

			ast->handler = ptrs_handle_withmember;
			ast->setHandler = ptrs_handle_assign_withmember;
			ast->addressHandler = ptrs_handle_addressof_withmember;
			ast->callHandler = ptrs_handle_call_withmember;

			ast->arg.withmember.base = ast->arg.varval;
			ast->arg.withmember.name = readIdentifier(code);
			ast->arg.withmember.index = code->withCount++;
		}
		else
		{
			unexpectedm(code, NULL, ".Identifier expressions are only valid inside with statements");
		}
	}
	else if(isdigit(curr) || curr == '.')
	{
		int startPos = code->pos;
		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_constant;

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
		ast->handler = ptrs_handle_constant;
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
				ast->handler = ptrs_handle_function;
				ast->arg.function.isAnonymous = true;

				ptrs_function_t *func = &ast->arg.function.func;
				func->name = "(lambda expression)";
				func->scope = NULL;
				func->nativeCb = NULL;
				func->argv = NULL;
				func->argc = parseArgumentDefinitionList(code, &func->args, NULL, &func->vararg);

				consume(code, "->");
				code->symbols->offset += 2 * sizeof(ptrs_var_t);
				if(lookahead(code, "{"))
				{
					func->body = parseStmtList(code, '}');
					consumec(code, '}');
				}
				else
				{
					ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
					retStmt->handler = ptrs_handle_return;
					retStmt->arg.astval = parseExpression(code);
					func->body = retStmt;
				}

				func->stackOffset = symbolScope_decrease(code);
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
		ast->addressHandler = NULL;
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

		ptrs_ast_t *expr = parseExpression(code);
		if(expr == NULL)
			unexpected(code, "Index");

		if(lookahead(code, ".."))
		{
			indexExpr->handler = ptrs_handle_slice;
			indexExpr->arg.slice.base = ast;
			indexExpr->arg.slice.start = expr;
			indexExpr->arg.slice.end = parseExpression(code);

			if(indexExpr->arg.slice.end == NULL)
				unexpected(code, "Index");
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
	else if(!ignoreAlgo && lookahead(code, "=>"))
	{
		struct ptrs_algorithmlist *curr = talloc(struct ptrs_algorithmlist);
		curr->entry = ast;
		curr->flags = 0;
		curr->orCombine = false;

		parseAlgorithmExpression(code, curr, true);

		ast = talloc(ptrs_ast_t);
		ast->handler = ptrs_handle_algorithm;
		ast->arg.algolist = curr;
		ast->code = code->src;
		ast->codepos = curr->entry->codepos;
		ast->file = curr->entry->file;
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
		curr->expand = false;
		curr->lazy = false;

		if(lookahead(code, "_"))
		{
			curr->entry = NULL;
		}
		else if(lookahead(code, "..."))
		{
			curr->expand = true;
			curr->entry = parseExpression(code);
			if(curr->entry->handler != ptrs_handle_identifier)
				unexpectedm(code, NULL, "Array spreading can only be used on identifiers");

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}
		else if(lookahead(code, "lazy"))
		{
			curr->lazy = true;
			curr->entry = parseExpression(code);

			if(curr->entry == NULL)
				unexpected(code, "Expression");
		}
		else
		{
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

static void parseAlgorithmExpression(code_t *code, struct ptrs_algorithmlist *curr, bool canBeLast)
{
	curr->next = talloc(struct ptrs_algorithmlist);
	curr = curr->next;

	switch(code->curr)
	{
		case '[':
			curr->flags = 3;
			break;
		case '?':
			curr->flags = 2;
			break;
		case '!':
			curr->flags = 1;
			break;
		default:
			curr->flags = 0;
	}

	if(curr->flags > 0)
		next(code);

	curr->entry = parseUnaryExpr(code, false, true);

	if(curr->flags == 3)
	{
		curr->orCombine = false;
		consumec(code, ']');
		consume(code, "=>");
		parseAlgorithmExpression(code, curr, true);
	}
	else if(lookahead(code, "=>"))
	{
		curr->orCombine = false;
		parseAlgorithmExpression(code, curr, true);
	}
	else if(curr->flags < 2 && lookahead(code, "||"))
	{
		curr->orCombine = true;
		parseAlgorithmExpression(code, curr, false);
	}
	else
	{
		if(curr->flags == 0 && canBeLast)
			curr->next = NULL;
		else
			unexpected(code, "=>");
	}
}

static void parseImport(code_t *code, ptrs_ast_t *stmt)
{
	stmt->handler = ptrs_handle_import;
	stmt->arg.import.imports = NULL;
	stmt->arg.import.wildcards = addHiddenSymbol(code, sizeof(ptrs_var_t));
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
		else
		{
			struct ptrs_importlist *curr = talloc(struct ptrs_importlist);
			curr->next = stmt->arg.import.imports;
			stmt->arg.import.imports = curr;

			curr->name = name;
			if(lookahead(code, "as"))
				curr->symbol = addSymbol(code, readIdentifier(code));
			else
				curr->symbol = addSymbol(code, strdup(curr->name));
		}

		if(code->curr == ';')
		{
			stmt->arg.import.from = NULL;
			break;
		}
		else if(lookahead(code, "from"))
		{
			stmt->arg.import.from = parseExpression(code);
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
	stmt->arg.switchcase.condition = parseExpression(code);
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
					ptrs_ast_t *expr = parseExpression(code);
					if(expr->handler != ptrs_handle_constant || expr->arg.constval.type != PTRS_TYPE_INT)
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


void *ptrs_asmBuffStart = NULL;
void *ptrs_asmBuff = NULL;
size_t ptrs_asmSize = 4096;

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

static ptrs_ast_t *parseNew(code_t *code, bool onStack)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);

	if(lookahead(code, "array"))
	{
		ast->arg.define.symbol.scope = -1;
		ast->arg.define.initExpr = NULL;
		ast->arg.define.isInitExpr = true;
		ast->arg.define.onStack = onStack;

		if(lookahead(code, "["))
		{
			ast->handler = ptrs_handle_vararray;
			ast->arg.define.value = parseExpression(code);
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
			ast->arg.define.value = parseExpression(code);
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
		ast->arg.newexpr.onStack = onStack;
		ast->arg.newexpr.value = parseUnaryExpr(code, true, false);

		consumec(code, '(');
		ast->arg.newexpr.arguments = parseExpressionList(code, ')');
		consumec(code, ')');
	}

	return ast;
}

static void parseMap(code_t *code, ptrs_ast_t *ast)
{
	ptrs_ast_t *structExpr = talloc(ptrs_ast_t);
	ptrs_struct_t *struc = &structExpr->arg.structval;

	ast->handler = ptrs_handle_new;
	ast->arg.newexpr.value = structExpr;
	ast->arg.newexpr.arguments = NULL;

	structExpr->handler = ptrs_handle_struct;

	struc->symbol.scope = (unsigned)-1;
	struc->size = 0;
	struc->name = "(map)";
	struc->overloads = NULL;
	struc->data = NULL;
	struc->staticData = NULL;

	consumec(code, '{');
	symbolScope_increase(code, 0, false);

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
		curr->protection = 0;
		curr->isStatic = 0;
		curr->offset = struc->size;
		struc->size += sizeof(ptrs_var_t);

		if(lookahead(code, "\""))
			curr->name = readString(code, NULL, NULL, NULL);
		else
			curr->name = readIdentifier(code);

		curr->namelen = strlen(curr->name);

		consumec(code, ':');
		curr->value.startval = parseExpression(code);

		if(code->curr == '}')
			break;
		consumec(code, ',');
	}
	curr->next = NULL;

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

			uint8_t oldYieldType = 2;
			ptrs_symbol_t oldYield = {(unsigned)-1, (unsigned)-1};
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
				consume(code, "this");

				if(overload->op == ptrs_handle_prefix_address && (code->curr == '.' || code->curr == '['))
				{
					char curr = code->curr;
					next(code);
					otherName = readIdentifier(code);

					if(curr == '.')
					{
						nameFormat = "%1$s.op &this.%3$s";
						overload->op = ptrs_handle_addressof_member;
					}
					else
					{
						consumec(code, ']');
						nameFormat = "%1$s.op &this[%3$s]";
						overload->op = ptrs_handle_addressof_index;
					}

					func->argc = 1;
					func->args = talloc(ptrs_symbol_t);
					func->args[0] = addSymbol(code, otherName);
				}
				else
				{
					nameFormat = "%1$s.op %2$sthis";
				}
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

							oldYieldType = code->yieldIsAlgo ? 1 : 0;
							oldYield = code->yield;
							code->yieldIsAlgo = true;
							code->yield = addHiddenSymbol(code, 16);

							func->argc = 1;
							func->args = talloc(ptrs_symbol_t);
							func->args[0] = code->yield;

							nameFormat = "%1$s.op this => any";
							opLabel = "=>";
							overload->op = ptrs_handle_algorithm;
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
										overload->op = curr == '.' ? ptrs_handle_assign_member : ptrs_handle_assign_index;
									}
									else if(code->curr == '(')
									{
										func->argc = parseArgumentDefinitionList(code,
											&func->args, &func->argv, &func->vararg);
										func->argc++;

										opLabel = "()";
										overload->op = curr == '.' ? ptrs_handle_call_member : ptrs_handle_call_index;


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
							}
						}
					}
				}
				else if(lookahead(code, "sizeof"))
				{
					consume(code, "this");

					nameFormat = "%1$s.op sizeof this";
					overload->op = ptrs_handle_prefix_length;

					func->argc = 0;
					func->args = NULL;
				}
				else if(lookahead(code, "foreach"))
				{
					consume(code, "in");
					consume(code, "this");

					nameFormat = "%1$s.op foreach in this";
					overload->op = ptrs_handle_forin;

					oldYieldType = code->yieldIsAlgo ? 1 : 0;
					oldYield = code->yield;
					code->yieldIsAlgo = false;
					code->yield = addHiddenSymbol(code, 16);

					func->argc = 1;
					func->args = talloc(ptrs_symbol_t);
					func->args[0] = code->yield;
				}
				else if(lookahead(code, "cast"))
				{
					consumec(code, '<');
					otherName = readIdentifier(code);
					consumec(code, '>');
					consume(code, "this");

					nameFormat = "%1$s.op cast<%3$s>this";
					overload->op = ptrs_handle_cast_builtin;

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

							oldYieldType = code->yieldIsAlgo ? 1 : 0;
							oldYield = code->yield;
							code->yieldIsAlgo = true;
							code->yield = addHiddenSymbol(code, 16);

							func->argc = 2;
							func->args = malloc(sizeof(ptrs_symbol_t) * 2);
							func->args[0] = code->yield;
							func->args[1] = addSymbol(code, otherName);

							overload->isLeftSide = false;
							overload->op = ptrs_handle_yield_algorithm;
						}
						else
						{
							nameFormat = "%1$s.op %3$s => this";

							func->argc = 1;
							func->args = talloc(ptrs_symbol_t);
							func->args[0] = addSymbol(code, otherName);

							overload->isLeftSide = false;
							overload->op = ptrs_handle_algorithm;
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

			if(oldYieldType != 2)
			{
				code->yield = oldYield;
				code->yieldIsAlgo = oldYieldType == 1 ? true : false;
			}

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
			overload->isLeftSide = true;
			overload->op = ptrs_handle_delete;
			overload->handler = parseFunction(code, name);
			overload->isStatic = isStatic;

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}

		struct ptrs_structlist *old = curr;
		curr = talloc(struct ptrs_structlist);

		if(old == NULL)
			struc->member = curr;
		else
			old->next = curr;

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

		struct symbollist *symbol = addSpecialSymbol(code, strdup(curr->name), PTRS_SYMBOL_THISMEMBER);
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
			ptrs_ast_t *ast = parseExpression(code);
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

#if SIZE_MAX == UINT16_MAX
#define ffi_type_size ffi_type_uint16
#define ffi_type_ssize ffi_type_sint16
#elif SIZE_MAX == UINT32_MAX
#define ffi_type_size ffi_type_uint32
#define ffi_type_ssize ffi_type_sint32
#elif SIZE_MAX == UINT64_MAX
#define ffi_type_size ffi_type_uint64
#define ffi_type_ssize ffi_type_sint64
#else
#error "size_t size is not supported"
#endif

#if UINTPTR_MAX == UINT16_MAX
#define ffi_type_uintptr ffi_type_uint16
#define ffi_type_sintptr ffi_type_sint16
#elif UINTPTR_MAX == UINT32_MAX
#define ffi_type_uintptr ffi_type_uint32
#define ffi_type_sintptr ffi_type_sint32
#elif UINTPTR_MAX == UINT64_MAX
#define ffi_type_uintptr ffi_type_uint64
#define ffi_type_sintptr ffi_type_sint64
#else
#error "intptr_t size is not supported"
#endif

#if PTRDIFF_MAX == INT16_MAX
#define ffi_type_ptrdiff ffi_type_sint16
#elif PTRDIFF_MAX == INT32_MAX
#define ffi_type_ptrdiff ffi_type_sint32
#elif PTRDIFF_MAX == INT64_MAX
#define ffi_type_ptrdiff ffi_type_sint64
#else
#error "ptrdiff_t size is not supported"
#endif

ptrs_nativetype_info_t ptrs_nativeTypes[] = {
	{"char", sizeof(signed char), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_schar},
	{"short", sizeof(short), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sshort},
	{"int", sizeof(int), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint},
	{"long", sizeof(long), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_slong},
	{"longlong", sizeof(long long), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint64},

	{"uchar", sizeof(unsigned char), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uchar},
	{"ushort", sizeof(unsigned short), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_ushort},
	{"uint", sizeof(unsigned int), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint},
	{"ulong", sizeof(unsigned long), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_ulong},
	{"ulonglong", sizeof(unsigned long long), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint64},

	{"i8", sizeof(int8_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint8},
	{"i16", sizeof(int16_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint16},
	{"i32", sizeof(int32_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint32},
	{"i64", sizeof(int64_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sint64},

	{"u8", sizeof(uint8_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint8},
	{"u16", sizeof(uint16_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint16},
	{"u32", sizeof(uint32_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint32},
	{"u64", sizeof(uint64_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uint64},

	{"single", sizeof(float), ptrs_handle_native_getFloat, ptrs_handle_native_setFloat, &ffi_type_float},
	{"double", sizeof(double), ptrs_handle_native_getFloat, ptrs_handle_native_setFloat, &ffi_type_double},

	{"native", sizeof(char *), ptrs_handle_native_getNative, ptrs_handle_native_setPointer, &ffi_type_pointer},
	{"pointer", sizeof(ptrs_var_t *), ptrs_handle_native_getPointer, ptrs_handle_native_setPointer, &ffi_type_pointer},

	{"bool", sizeof(bool), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uchar},
	{"ssize", sizeof(ssize_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_ssize},
	{"size", sizeof(size_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_size},
	{"intptr", sizeof(uintptr_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_sintptr},
	{"uintptr", sizeof(intptr_t), ptrs_handle_native_getUInt, ptrs_handle_native_setUInt, &ffi_type_uintptr},
	{"ptrdiff", sizeof(ptrdiff_t), ptrs_handle_native_getInt, ptrs_handle_native_setInt, &ffi_type_ptrdiff},
};
int ptrs_nativeTypeCount = sizeof(ptrs_nativeTypes) / sizeof(ptrs_nativetype_info_t);

static ptrs_nativetype_info_t *readNativeType(code_t *code)
{
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

static char *readString(code_t *code, int *length, struct ptrs_stringformat **insertions, int *insertionCount)
{
	int buffSize = 1024;
	int i = 0;

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
			else if(insertions != NULL && code->curr == '$')
			{
				rawnext(code);

				if(insertionCount != NULL)
					(*insertionCount)++;

				if(curr == NULL)
				{
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
					curr->entry = parseExpression(code);

					if(code->curr != '}')
						unexpected(code, "}");
					rawnext(code);
				}
				else
				{
					int j;
					char name[128];
					for(j = 0; j < 128 && (isalnum(code->curr) || code->curr == '_'); j++)
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

	entry->type = PTRS_SYMBOL_DEFAULT;
	entry->text = text;
	entry->arg.offset = offset;
	entry->next = curr->current;
	curr->current = entry;
}

static ptrs_symbol_t addSymbol(code_t *code, char *symbol)
{
	ptrs_symboltable_t *curr = code->symbols;
	struct symbollist *entry = talloc(struct symbollist);

	entry->type = PTRS_SYMBOL_DEFAULT;
	entry->text = symbol;
	entry->next = curr->current;
	entry->arg.offset = curr->offset;
	curr->offset += sizeof(ptrs_var_t);
	curr->current = entry;

	ptrs_symbol_t result = {0, entry->arg.offset};
	return result;
}

static ptrs_symbol_t addHiddenSymbol(code_t *code, size_t size)
{
	ptrs_symbol_t result = {0, code->symbols->offset};
	code->symbols->offset += size;

	return result;
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
				ast->arg.wildcard.symbol.scope = stmt->wildcards.scope + level;
				ast->arg.wildcard.symbol.offset = stmt->wildcards.offset;
				ast->arg.wildcard.index = import->wildcardIndex;

				struct symbollist *entry = talloc(struct symbollist);
				entry->text = strdup(text);
				entry->type = PTRS_SYMBOL_WILDCARD;
				entry->arg.wildcard.index = import->wildcardIndex;
				entry->arg.wildcard.offset = stmt->wildcards.offset;
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
	new->offset = buildInCount * sizeof(ptrs_var_t);
	new->maxOffset = 0;
	new->isInline = isInline;
	new->outer = code->symbols;
	new->current = NULL;
	new->wildcards = NULL;

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

	struct wildcardsymbol *currw = scope->wildcards;
	while(curr != NULL)
	{
		struct wildcardsymbol *old = currw;
		currw = currw->next;
		free(old->start);
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
