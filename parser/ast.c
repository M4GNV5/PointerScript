#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "../jit/jit.h"
#include "ast.h"

#define talloc(type) calloc(sizeof(type), 1)

typedef struct code code_t;
struct symbollist;

typedef enum
{
	PTRS_SYMBOL_DEFAULT,
	PTRS_SYMBOL_FUNCTION,
	PTRS_SYMBOL_CONST,
	PTRS_SYMBOL_IMPORTED,
	PTRS_SYMBOL_THISMEMBER,
} ptrs_symboltype_t;

struct symbollist
{
	union
	{
		void *data;
		ptrs_jit_var_t *location;
		struct ptrs_ast_function *function;
		struct
		{
			ptrs_nativetype_info_t *type; //optional
			ptrs_ast_t *import;
			unsigned index;
		} imported;
	} arg;
	ptrs_symboltype_t type;
	char *text;
	struct symbollist *next;
};
struct typelist
{
	char *name;
	ptrs_typing_t type;
	struct typelist *next;
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
	bool functionBoundary;
	struct symbollist *current;
	struct typelist *types;
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
	bool usesTryCatch;
	ptrs_jit_var_t *thisVar;
};

static ptrs_ast_t *parseStmtList(code_t *code, char end);
static ptrs_ast_t *parseStatement(code_t *code);
static ptrs_ast_t *parseExpression(code_t *code, bool required);
static ptrs_ast_t *parseBinaryExpr(code_t *code, ptrs_ast_t *left, int minPrec);
static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls);
static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls);
static struct ptrs_astlist *parseExpressionList(code_t *code, char end);
static ptrs_ast_t *parseNew(code_t *code, bool onStack);
static void parseTyping(code_t *code, ptrs_typing_t *typing);
static void parseOptionalTyping(code_t *code, ptrs_typing_t *typing);
static void parseArrayTyping(code_t *code, ptrs_nativetype_info_t *nativeType, ptrs_meta_t *result, struct ptrs_ast **sizePtr);
static void parseMap(code_t *code, ptrs_ast_t *expr);
static void parseStruct(code_t *code, ptrs_struct_t *struc);
static void parseImport(code_t *code, ptrs_ast_t *stmt);
static void parseSwitchCase(code_t *code, ptrs_ast_t *stmt);

static ptrs_vartype_t readTypeName(code_t *code);
static ptrs_nativetype_info_t *readNativeType(code_t *code);
static ptrs_ast_vtable_t *readPrefixOperator(code_t *code, const char **label);
static ptrs_ast_vtable_t *readSuffixOperator(code_t *code, const char **label);
static char *readIdentifier(code_t *code);
static char *readString(code_t *code, int *length, struct ptrs_stringformat **insertions, int *insertionsCount);
static char readEscapeSequence(code_t *code);
static int64_t readInt(code_t *code, int base);
static double readDouble(code_t *code);

static void addSymbol(code_t *code, char *text, ptrs_jit_var_t *location);
static struct symbollist *addSpecialSymbol(code_t *code, char *symbol, ptrs_symboltype_t type);
static ptrs_ast_t *getSymbol(code_t *code, char *text);
static void addType(code_t *code, char *name, ptrs_typing_t *type);
static ptrs_typing_t *getType(code_t *code, const char *name);
static void symbolScope_increase(code_t *code, bool functionBoundary);
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

ptrs_ast_t *ptrs_parse(char *src, const char *filename, ptrs_symboltable_t **symbols, bool addInitRoot)
{
	code_t code;
	code.filename = filename;
	code.src = src;
	code.curr = src[0];
	code.pos = 0;
	code.usesTryCatch = false;
	code.insideIndex = false;
	code.thisVar = NULL;

	if(symbols == NULL || *symbols == NULL)
	{
		code.symbols = NULL;
		symbolScope_increase(&code, true);
		//addSymbol(&code, strdup("arguments"), TODO argumentsLocation);
	}
	else
	{
		code.symbols = *symbols;
	}

	if(src[0] == '#' && src[1] == '!')
	{
		while(code.curr != '\n')
		{
			code.pos++;
			code.curr = code.src[code.pos];
		}
		next(&code);
	}

	while(skipSpaces(&code) || skipComments(&code));

	ptrs_ast_t *initRoot;
	if(addInitRoot)
	{
		initRoot = talloc(ptrs_ast_t);
		initRoot->vtable = &ptrs_ast_vtable_initroot;
		initRoot->code = code.src;
		initRoot->codepos = 0;
		initRoot->file = code.filename;
		addSymbol(&code, strdup("arguments"), &initRoot->arg.initroot.argumentsLocation);
	}

	ptrs_ast_t *ast = parseStmtList(&code, 0);

	if(addInitRoot)
	{
		initRoot->arg.initroot.hasTryCatch = code.usesTryCatch;

		struct ptrs_astlist *entry = talloc(struct ptrs_astlist);
		entry->entry = initRoot;
		entry->next = ast->arg.astlist;
		ast->arg.astlist = entry;
	}

	if(symbols == NULL)
		symbolScope_decrease(&code);
	else
		*symbols = code.symbols;
	return ast;
}

int ptrs_ast_getSymbol(ptrs_symboltable_t *symbols, char *text, ptrs_ast_t **node)
{
	bool functionBoundary = false;
	ptrs_symboltable_t *innermost = symbols;

	if(node != NULL)
		*node = NULL;

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
						ast->vtable = &ptrs_ast_vtable_identifier;

						ast->arg.identifier.location = curr->arg.location;
						ast->arg.identifier.typePredicted = false;
						ast->arg.identifier.valuePredicted = false;
						ast->arg.identifier.metaPredicted = false;

						if(functionBoundary)
							ast->arg.identifier.location->addressable = 1;
						break;

					case PTRS_SYMBOL_FUNCTION:
						*node = ast = talloc(ptrs_ast_t);
						ast->vtable = &ptrs_ast_vtable_functionidentifier;

						ast->arg.funcval = curr->arg.function;
						break;

					case PTRS_SYMBOL_CONST:
						*node = ast = talloc(ptrs_ast_t);
						memcpy(ast, curr->arg.data, sizeof(ptrs_ast_t));
						break;

					case PTRS_SYMBOL_IMPORTED:
						*node = ast = talloc(ptrs_ast_t);
						ast->vtable = &ptrs_ast_vtable_importedsymbol;

						ast->arg.importedsymbol.import = curr->arg.imported.import;
						ast->arg.importedsymbol.index = curr->arg.imported.index;
						ast->arg.importedsymbol.type = curr->arg.imported.type;
						break;

					case PTRS_SYMBOL_THISMEMBER:
						*node = ast = talloc(ptrs_ast_t);
						ast->vtable = &ptrs_ast_vtable_member;

						if(ptrs_ast_getSymbol(innermost, "this", &ast->arg.member.base) != 0)
							ptrs_error(NULL, "Internal error with thismember symbol %s", curr->text);
						ast->arg.member.name = strdup(curr->text);
						ast->arg.member.namelen = strlen(curr->text);
						break;
				}
				return 0;
			}
			curr = curr->next;
		}

		functionBoundary = functionBoundary || symbols->functionBoundary;
		symbols = symbols->outer;
	}
	return 1;
}

static ptrs_ast_t *parseStmtList(code_t *code, char end)
{
	ptrs_ast_t *elem = talloc(ptrs_ast_t);
	elem->vtable = &ptrs_ast_vtable_body;
	elem->codepos = code->pos;
	elem->code = code->src;
	elem->file = code->filename;

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

static ptrs_ast_t *astToAstlist(ptrs_ast_t *ast)
{
	struct ptrs_astlist *entry = talloc(struct ptrs_astlist);
	entry->entry = ast;

	ast = talloc(ptrs_ast_t);
	ast->vtable = &ptrs_ast_vtable_body;
	ast->codepos = ast->codepos;
	ast->code = ast->code;
	ast->file = ast->file;
	ast->arg.astlist = entry;

	return ast;
}
static ptrs_ast_t *prependAstToAst(ptrs_ast_t *ast, ptrs_ast_t *elem)
{
	if(ast->vtable != &ptrs_ast_vtable_body)
		ast = astToAstlist(ast);

	struct ptrs_astlist *entry = talloc(struct ptrs_astlist);
	entry->entry = elem;
	entry->next = ast->arg.astlist;
	ast->arg.astlist = entry;

	return ast;
}
static ptrs_ast_t *appendAstToAst(ptrs_ast_t *ast, ptrs_ast_t *elem)
{
	if(ast->vtable != &ptrs_ast_vtable_body)
		ast = astToAstlist(ast);

	struct ptrs_astlist *last = ast->arg.astlist;
	while(last->next != NULL)
		last = last->next;

	last->next = talloc(struct ptrs_astlist);
	last = last->next;
	last->entry = elem;
	last->next = NULL;

	return ast;
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
		*fields = calloc(count, sizeof(char *));
	*symbols = calloc(count, sizeof(ptrs_jit_var_t));

	for(int i = 0; i < count; i++)
	{
		if(fields != NULL && lookahead(code, "_"))
		{
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

static ptrs_funcparameter_t *parseArgumentDefinitionList(code_t *code,
	ptrs_jit_var_t **vararg, ptrs_typing_t *retType)
{
	consumec(code, '(');

	if(vararg != NULL)
		*vararg = NULL;

	if(code->curr == ')')
	{
		next(code);
		if(retType != NULL)
			parseOptionalTyping(code, retType);

		return NULL;
	}

	ptrs_funcparameter_t *first = NULL;
	ptrs_funcparameter_t **nextPtr = &first;

	for(;;)
	{
		char *name;
		if(lookahead(code, "_"))
		{
			name = NULL;
		}
		else
		{
			name = readIdentifier(code);
			if(vararg != NULL && lookahead(code, "..."))
			{
				consumecm(code, ')', "Vararg argument has to be the last argument");

				*vararg = talloc(ptrs_jit_var_t);
				addSymbol(code, name, *vararg);
				break;
			}
		}

		ptrs_funcparameter_t *curr = talloc(ptrs_funcparameter_t);
		*nextPtr = curr;
		nextPtr = &curr->next;

		curr->name = name;
		curr->arg.constType = -1;
		curr->typing.meta.type = -1;
		curr->typing.nativetype = NULL;

		if(name != NULL)
		{
			if(retType != NULL)
				parseOptionalTyping(code, &curr->typing);

			if(lookahead(code, "="))
			{
				curr->argv = parseExpression(code, true);
				if(curr->argv->vtable != &ptrs_ast_vtable_constant)
					PTRS_HANDLE_ASTERROR(curr->argv, "function parameter default values have to be constants");
			}

			addSymbol(code, strdup(curr->name), &curr->arg);
		}

		if(code->curr == ')')
		{
			next(code);
			break;
		}

		consumec(code, ',');
	}

	if(retType != NULL)
		parseOptionalTyping(code, retType);

	return first;
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

static ptrs_ast_t *parseBody(code_t *code, bool allowStmt)
{
	symbolScope_increase(code, false);
	ptrs_ast_t *result = parseScopelessBody(code, allowStmt);
	symbolScope_decrease(code);

	return result;
}

static void parseFunctionBody(code_t *code, ptrs_function_t *func)
{
	bool oldTryCatch = code->usesTryCatch;
	code->usesTryCatch = false;
	func->body = parseScopelessBody(code, false);
	func->usesTryCatch = code->usesTryCatch;
	code->usesTryCatch = oldTryCatch;
}

static void parseFunctionInto(code_t *code, ptrs_function_t *func)
{
	symbolScope_increase(code, true);
	addSymbol(code, strdup("this"), &func->thisVal);

	func->args = parseArgumentDefinitionList(code, &func->vararg, &func->retType);
	parseFunctionBody(code, func);

	symbolScope_decrease(code);
}

static ptrs_function_t *parseFunction(code_t *code, char *name)
{
	ptrs_function_t *func = talloc(ptrs_function_t);
	func->name = name;
	parseFunctionInto(code, func);

	return func;
}

static ptrs_ast_t *parseStatement(code_t *code)
{
	if(code->curr == '{')
	{
		return parseBody(code, false);
	}
	else if(lookahead(code, "const"))
	{
		char *name = readIdentifier(code);
		consumec(code, '=');
		ptrs_ast_t *ast = parseExpression(code, true);
		if(ast->vtable != &ptrs_ast_vtable_constant)
			unexpectedm(code, NULL, "Initializer for 'const' variable is not a constant");

		struct symbollist *entry = addSpecialSymbol(code, name, PTRS_SYMBOL_CONST);
		entry->arg.data = ast;
		consumec(code, ';');
		return NULL;
	}

	ptrs_ast_t *stmt = talloc(ptrs_ast_t);
	stmt->codepos = code->pos;
	stmt->code = code->src;
	stmt->file = code->filename;

	if(lookahead(code, "var"))
	{
		char *name = readIdentifier(code);

		if(lookahead(code, ":"))
		{
			ptrs_nativetype_info_t *nativeType = readNativeType(code);
			stmt->vtable = &ptrs_ast_vtable_array;
			stmt->arg.definearray.onStack = true;
			parseArrayTyping(code, nativeType, &stmt->arg.definearray.meta, &stmt->arg.definearray.length);

			if(lookahead(code, "="))
			{
				consumec(code, '[');
				stmt->arg.definearray.initVal = parseExpressionList(code, ']');
				consumec(code, ']');
			}

			addSymbol(code, name, &stmt->arg.definearray.location);
		}
		else if(lookahead(code, "="))
		{
			stmt->vtable = &ptrs_ast_vtable_define;
			stmt->arg.define.value = parseExpression(code, true);
			stmt->arg.define.type = (uint8_t)-1;
			addSymbol(code, name, &stmt->arg.define.location);
		}
		else
		{
			stmt->vtable = &ptrs_ast_vtable_define;
			stmt->arg.define.value = NULL;
			stmt->arg.define.type = (uint8_t)-1;
			addSymbol(code, name, &stmt->arg.define.location);
		}

		consumec(code, ';');
	}
	else if(lookahead(code, "import"))
	{
		parseImport(code, stmt);
	}
	else if(lookahead(code, "return"))
	{
		stmt->vtable = &ptrs_ast_vtable_return;
		stmt->arg.astval = parseExpression(code, false);
		consumec(code, ';');
	}
	else if(lookahead(code, "break"))
	{
		stmt->vtable = &ptrs_ast_vtable_break;
		consumec(code, ';');
	}
	else if(lookahead(code, "continue"))
	{
		stmt->vtable = &ptrs_ast_vtable_continue;
		consumec(code, ';');
	}
	else if(lookahead(code, "delete"))
	{
		stmt->vtable = &ptrs_ast_vtable_delete;
		stmt->arg.astval = parseExpression(code, true);
		consumec(code, ';');
	}
	else if(lookahead(code, "throw"))
	{
		stmt->vtable = &ptrs_ast_vtable_throw;
		stmt->arg.astval = parseExpression(code, true);
		consumec(code, ';');
	}
	else if(lookahead(code, "scoped"))
	{
		stmt->vtable = &ptrs_ast_vtable_scopestatement;
		symbolScope_increase(code, true);
		stmt->arg.astval = parseBody(code, true);
		symbolScope_decrease(code);
	}
	else if(lookahead(code, "try"))
	{
		code->usesTryCatch = true;

		stmt->vtable = &ptrs_ast_vtable_trycatch;
		stmt->arg.trycatch.tryBody = parseBody(code, true);

		if(lookahead(code, "catch"))
		{
			symbolScope_increase(code, true);
			stmt->arg.trycatch.args = parseArgumentDefinitionList(code, NULL, NULL);
			stmt->arg.trycatch.catchBody = parseScopelessBody(code, true);
			symbolScope_decrease(code);
		}
		else
		{
			stmt->arg.trycatch.catchBody = NULL;
		}

		if(lookahead(code, "finally"))
		{
			symbolScope_increase(code, true);
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

			stmt->arg.trycatch.finallyBody = parseBody(code, true);
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
		stmt->vtable = &ptrs_ast_vtable_function;

		ptrs_function_t *func = &stmt->arg.function.func;
		func->name = readIdentifier(code);

		stmt->arg.function.isExpression = false;
		stmt->arg.function.symbol = NULL;

		struct symbollist *symbol = addSpecialSymbol(code, strdup(func->name), PTRS_SYMBOL_FUNCTION);
		symbol->arg.function = &stmt->arg.function;

		parseFunctionInto(code, func);
	}
	else if(lookahead(code, "struct"))
	{
		stmt->vtable = &ptrs_ast_vtable_struct;
		ptrs_struct_t *struc = &stmt->arg.structval;
		parseStruct(code, struc);
		struc->ast = stmt;
	}
	else if(lookahead(code, "if"))
	{
		stmt->vtable = &ptrs_ast_vtable_if;

		consumec(code, '(');
		stmt->arg.ifelse.condition = parseExpression(code, true);
		consumec(code, ')');
		stmt->arg.ifelse.ifBody = parseBody(code, true);

		if(lookahead(code, "else"))
			stmt->arg.ifelse.elseBody = parseBody(code, true);
		else
			stmt->arg.ifelse.elseBody = NULL;
	}
	else if(lookahead(code, "switch"))
	{
		parseSwitchCase(code, stmt);
	}
	else if(lookahead(code, "loop"))
	{
		stmt->vtable = &ptrs_ast_vtable_loop;
		stmt->arg.astval = parseBody(code, true);
	}
	else if(lookahead(code, "while"))
	{
		stmt->vtable = &ptrs_ast_vtable_loop;

		ptrs_ast_t *breakIf = talloc(ptrs_ast_t);
		breakIf->vtable = &ptrs_ast_vtable_if;
		consumec(code, '(');
		breakIf->arg.ifelse.condition = parseExpression(code, true);
		consumec(code, ')');
		breakIf->arg.ifelse.ifBody = NULL;
		breakIf->arg.ifelse.elseBody = talloc(ptrs_ast_t);
		breakIf->arg.ifelse.elseBody->vtable = &ptrs_ast_vtable_break;

		stmt->arg.astval = parseBody(code, true);
		stmt->arg.astval = prependAstToAst(stmt->arg.astval, breakIf);
	}
	else if(lookahead(code, "do"))
	{
		stmt->vtable = &ptrs_ast_vtable_loop;

		symbolScope_increase(code, false);
		stmt->arg.astval = parseScopelessBody(code, true);
		consume(code, "while");

		ptrs_ast_t *breakIf = talloc(ptrs_ast_t);
		breakIf->vtable = &ptrs_ast_vtable_if;
		breakIf->arg.ifelse.ifBody = NULL;
		breakIf->arg.ifelse.elseBody = talloc(ptrs_ast_t);
		breakIf->arg.ifelse.elseBody->vtable = &ptrs_ast_vtable_break;

		consumec(code, '(');
		breakIf->arg.ifelse.condition = parseExpression(code, true);
		consumec(code, ')');
		consumec(code, ';');
		symbolScope_decrease(code);

		stmt->arg.astval = appendAstToAst(stmt->arg.astval, breakIf);
	}
	else if(lookahead(code, "foreach"))
	{
		stmt->vtable = &ptrs_ast_vtable_forin_setup;
		symbolScope_increase(code, false);

		consumec(code, '(');
		stmt->arg.forin.varcount = parseIdentifierList(code, "in", &stmt->arg.forin.varsymbols, NULL);
		consume(code, "in");
		stmt->arg.forin.valueAst = parseExpression(code, true);
		consumec(code, ')');

		ptrs_ast_t *loopStmt = talloc(ptrs_ast_t);
		loopStmt->vtable = &ptrs_ast_vtable_loop;
		loopStmt->arg.astval = parseScopelessBody(code, true);

		ptrs_ast_t *loopStep = talloc(ptrs_ast_t);
		loopStep->vtable = &ptrs_ast_vtable_forin_step;
		loopStep->arg.forinptr = &stmt->arg.forin;

		loopStmt->arg.astval = prependAstToAst(loopStmt->arg.astval, loopStep);
		stmt = appendAstToAst(stmt, loopStmt);

		symbolScope_decrease(code);
	}
	else if(lookahead(code, "for"))
	{
		stmt->vtable = &ptrs_ast_vtable_loop;

		consumec(code, '(');

		symbolScope_increase(code, false);
		ptrs_ast_t *init = parseStatement(code);

		ptrs_ast_t *condition = parseExpression(code, false);
		ptrs_ast_t *breakIf = NULL;
		if(condition != NULL)
		{
			breakIf = talloc(ptrs_ast_t);
			breakIf->vtable = &ptrs_ast_vtable_if;
			breakIf->arg.ifelse.ifBody = NULL;
			breakIf->arg.ifelse.elseBody = talloc(ptrs_ast_t);
			breakIf->arg.ifelse.elseBody->vtable = &ptrs_ast_vtable_break;
			breakIf->arg.ifelse.condition = condition;
		}

		consumec(code, ';');
		ptrs_ast_t *stepExpr = parseExpression(code, false);
		ptrs_ast_t *step = talloc(ptrs_ast_t);
		step->vtable = &ptrs_ast_vtable_exprstatement;
		step->arg.astval = stepExpr;
		consumec(code, ')');

		stmt->arg.astval = parseScopelessBody(code, true);
		symbolScope_decrease(code);

		if(step != NULL)
		{
			ptrs_ast_t *contLabel = talloc(ptrs_ast_t);
			contLabel->vtable = &ptrs_ast_vtable_continue_label;
			stmt->arg.astval = appendAstToAst(stmt->arg.astval, contLabel);
			stmt->arg.astval = appendAstToAst(stmt->arg.astval, step);
		}

		if(breakIf != NULL)
			stmt->arg.astval = prependAstToAst(stmt->arg.astval, breakIf);

		stmt = prependAstToAst(stmt, init);
	}
	else
	{
		stmt->vtable = &ptrs_ast_vtable_exprstatement;
		stmt->arg.astval = parseExpression(code, false);
		consumec(code, ';');
	}
	return stmt;
}

struct opinfo
{
	const char *op;
	int precendence;
	bool isAssignOp;
	ptrs_ast_vtable_t *vtable;
};

struct opinfo binaryOps[] = {
	// >> and << need to be before > and < or we will always lookahead greater / less
	{">>", 12, false, &ptrs_ast_vtable_op_sshr}, //signed shift right
	{">>", 12, false, &ptrs_ast_vtable_op_ushr}, //unsigned shift right
	{"<<", 12, false, &ptrs_ast_vtable_op_shl}, //shift left

	{"===", 9, false, &ptrs_ast_vtable_op_typeequal},
	{"!==", 9, false, &ptrs_ast_vtable_op_typeinequal},

	{"==", 9, false, &ptrs_ast_vtable_op_equal},
	{"!=", 9, false, &ptrs_ast_vtable_op_inequal},
	{"<=", 10, false, &ptrs_ast_vtable_op_lessequal},
	{">=", 10, false, &ptrs_ast_vtable_op_greaterequal},
	{"<", 10, false, &ptrs_ast_vtable_op_less},
	{">", 10, false, &ptrs_ast_vtable_op_greater},

	{"=", 1, true, &ptrs_ast_vtable_op_assign},
	{"+=", 1, true, &ptrs_ast_vtable_op_add},
	{"-=", 1, true, &ptrs_ast_vtable_op_sub},
	{"*=", 1, true, &ptrs_ast_vtable_op_mul},
	{"/=", 1, true, &ptrs_ast_vtable_op_div},
	{"%=", 1, true, &ptrs_ast_vtable_op_mod},
	{">>=", 1, true, &ptrs_ast_vtable_op_sshr},
	{"<<=", 1, true, &ptrs_ast_vtable_op_shl},
	{"&=", 1, true, &ptrs_ast_vtable_op_and},
	{"^=", 1, true, &ptrs_ast_vtable_op_xor},
	{"|=", 1, true, &ptrs_ast_vtable_op_or},

	{"?", 2, true, &ptrs_ast_vtable_op_ternary},
	//{":", -1, true, &ptrs_ast_vtable_op_ternary},

	{"instanceof", 11, false, &ptrs_ast_vtable_op_instanceof},
	{"in", 11, false, &ptrs_ast_vtable_op_in},

	{"||", 3, false, &ptrs_ast_vtable_op_logicor},
	{"^^", 4, false, &ptrs_ast_vtable_op_logicxor},
	{"&&", 5, false, &ptrs_ast_vtable_op_logicand},

	{"|", 6, false, &ptrs_ast_vtable_op_or},
	{"^", 7, false, &ptrs_ast_vtable_op_xor},
	{"&", 8, false, &ptrs_ast_vtable_op_and},

	{"+", 13, false, &ptrs_ast_vtable_op_add},
	{"-", 13, false, &ptrs_ast_vtable_op_sub},

	{"*", 14, false, &ptrs_ast_vtable_op_mul},
	{"/", 14, false, &ptrs_ast_vtable_op_div},
	{"%", 14, false, &ptrs_ast_vtable_op_mod}
};
static int binaryOpCount = sizeof(binaryOps) / sizeof(struct opinfo);

struct opinfo prefixOps[] = {
	{"typeof", 0, true, &ptrs_ast_vtable_prefix_typeof},
	{"++", 0, true, &ptrs_ast_vtable_prefix_inc}, //prefixed ++
	{"--", 0, true, &ptrs_ast_vtable_prefix_dec}, //prefixed --
	{"!", 0, true, &ptrs_ast_vtable_prefix_logicnot}, //logical NOT
	{"~", 0, true, &ptrs_ast_vtable_prefix_not}, //bitwise NOT
	{"&", 0, true, &ptrs_ast_vtable_prefix_address}, //adress of
	{"*", 0, true, &ptrs_ast_vtable_prefix_dereference}, //dereference
	{"+", 0, true, &ptrs_ast_vtable_prefix_plus}, //unary +
	{"-", 0, true, &ptrs_ast_vtable_prefix_minus} //unary -
};
static int prefixOpCount = sizeof(prefixOps) / sizeof(struct opinfo);

struct opinfo suffixedOps[] = {
	{"++", 0, false, &ptrs_ast_vtable_suffix_inc}, //suffixed ++
	{"--", 0, false, &ptrs_ast_vtable_suffix_dec} //suffixed --
};
static int suffixedOpCount = sizeof(suffixedOps) / sizeof(struct opinfo);

static ptrs_ast_t *parseExpression(code_t *code, bool required)
{
	ptrs_ast_t *ast = parseUnaryExpr(code, false);
	if(ast != NULL)
		ast = parseBinaryExpr(code, ast, 0);

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
		ptrs_ast_t *right = parseUnaryExpr(code, false);
		ahead = peekBinaryOp(code);

		while(ahead != NULL && ahead->precendence > op->precendence)
		{
			right = parseBinaryExpr(code, right, ahead->precendence);
			ahead = peekBinaryOp(code);
		}

		ptrs_ast_t *_left = left;

		if(op->vtable == &ptrs_ast_vtable_op_ternary)
		{
			left = talloc(ptrs_ast_t);
			left->vtable = &ptrs_ast_vtable_op_ternary;
			left->arg.ternary.condition = _left;
			left->arg.ternary.trueVal = right;
			consumec(code, ':');
			left->arg.ternary.falseVal = parseExpression(code, true);
			left->codepos = pos;
			left->code = code->src;
			left->file = code->filename;
			continue;
		}

		left = talloc(ptrs_ast_t);
		left->vtable = op->vtable;
		left->arg.binary.left = _left;
		left->arg.binary.right = right;
		left->codepos = pos;
		left->code = code->src;
		left->file = code->filename;

		if(op->isAssignOp && _left->vtable->set == NULL)
			PTRS_HANDLE_ASTERROR(left, "Invalid assign expression, left side is not a valid lvalue");

		if(op->isAssignOp && op->vtable != &ptrs_ast_vtable_op_assign)
		{
			right = left;
			left = talloc(ptrs_ast_t);
			left->vtable = &ptrs_ast_vtable_op_assign;
			left->arg.binary.left = _left;
			left->arg.binary.right = right;
			left->codepos = pos;
			left->code = code->src;
			left->file = code->filename;
		}
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
	{"null", PTRS_TYPE_POINTER, {.ptrval = NULL}},
	{"undefined", PTRS_TYPE_UNDEFINED, {}},
	{"NaN", PTRS_TYPE_FLOAT, {.floatval = NAN}},
	{"Infinity", PTRS_TYPE_FLOAT, {.floatval = INFINITY}},
	{"PI", PTRS_TYPE_FLOAT, {.floatval = M_PI}},
	{"E", PTRS_TYPE_FLOAT, {.floatval = M_E}},
};
int constantCount = sizeof(constants) / sizeof(struct constinfo);

static ptrs_ast_t *parseUnaryExpr(code_t *code, bool ignoreCalls)
{
	char curr = code->curr;
	int pos = code->pos;
	ptrs_ast_t *ast = NULL;


	ptrs_ast_vtable_t *vtable = readPrefixOperator(code, NULL);
	if(vtable == &ptrs_ast_vtable_prefix_minus && isdigit(code->curr))
	{
		code->pos = pos;
		code->curr = curr;
	}
	else if(vtable != NULL)
	{
		ast = talloc(ptrs_ast_t);
		ast->arg.astval = parseUnaryExpr(code, false);
		ast->vtable = vtable;

		if(vtable == &ptrs_ast_vtable_prefix_address)
		{
			ptrs_ast_t *child = ast->arg.astval;
			if(child == NULL || child->vtable == NULL)
				unexpectedm(code, NULL, "Cannot get the address of this expression");
			else if(child->vtable == &ptrs_ast_vtable_identifier)
				child->arg.identifier.location->addressable = 1;
		}

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
			ast->vtable = &ptrs_ast_vtable_constant;
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

		if(type >= PTRS_NUM_TYPES)
			unexpectedm(code, NULL, "Syntax is type<TYPENAME>");

		ast = talloc(ptrs_ast_t);
		ast->vtable = &ptrs_ast_vtable_constant;
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

			ast->vtable = &ptrs_ast_vtable_constant;
			ast->arg.constval.meta.type = PTRS_TYPE_INT;
			ast->arg.constval.value.intval = nativeType->size;
		}
		else if(lookahead(code, "var"))
		{
			if(hasBrace)
				consumec(code, ')');

			ast->vtable = &ptrs_ast_vtable_constant;
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

			ast->vtable = &ptrs_ast_vtable_prefix_sizeof;
			ast->arg.astval = parseUnaryExpr(code, ignoreCalls);
		}
	}
	else if(lookahead(code, "as"))
	{
		consumec(code, '<');
		ptrs_vartype_t type = readTypeName(code);

		if(type < PTRS_NUM_TYPES)
		{
			ast = talloc(ptrs_ast_t);
			ast->vtable = &ptrs_ast_vtable_as;
			ast->arg.cast.meta.type = type;
		}
		else
		{
			ptrs_nativetype_info_t *type = readNativeType(code);

			if(type != NULL)
			{
				ast = talloc(ptrs_ast_t);
				ast->vtable = &ptrs_ast_vtable_as;
				parseArrayTyping(code, type, &ast->arg.cast.meta, NULL);
			}
			else
			{
				ptrs_ast_t *structType = parseUnaryExpr(code, false);

				if(structType == NULL)
					unexpectedm(code, NULL, "Expected a variable, native or struct type to cast to");

				ast = talloc(ptrs_ast_t);
				ast->vtable = &ptrs_ast_vtable_as_struct;
				ast->arg.cast.type = structType;
			}
		}

		consumec(code, '>');
		ast->arg.cast.value = parseUnaryExpr(code, false);
	}
	else if(lookahead(code, "cast"))
	{
		consumec(code, '<');

		if(lookahead(code, "int"))
		{
			ast = talloc(ptrs_ast_t);
			ast->vtable = &ptrs_ast_vtable_cast_builtin;
			ast->arg.cast.meta.type = PTRS_TYPE_INT;
		}
		else if(lookahead(code, "float"))
		{
			ast = talloc(ptrs_ast_t);
			ast->vtable = &ptrs_ast_vtable_cast_builtin;
			ast->arg.cast.meta.type = PTRS_TYPE_FLOAT;
		}
		else if(lookahead(code, "string"))
		{
			ast = talloc(ptrs_ast_t);
			ast->vtable = &ptrs_ast_vtable_tostring;
		}
		else
		{
			unexpected(code, "int, float or string");
		}

		consumec(code, '>');
		ast->arg.cast.value = parseUnaryExpr(code, false);
	}
	else if(lookahead(code, "function"))
	{
		ast = talloc(ptrs_ast_t);
		ast->vtable = &ptrs_ast_vtable_function;
		ast->arg.function.isExpression = true;

		ptrs_function_t *func = &ast->arg.function.func;
		func->name = "(anonymous function)";

		parseFunctionInto(code, func);
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
		free(name);
	}
	else if(isdigit(curr) || curr == '.' || curr == '-')
	{
		int startPos = code->pos;
		ast = talloc(ptrs_ast_t);
		ast->vtable = &ptrs_ast_vtable_constant;

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
		ast->vtable = &ptrs_ast_vtable_indexlength;
	}
	else if(curr == '\'')
	{
		rawnext(code);
		ast = talloc(ptrs_ast_t);
		ast->vtable = &ptrs_ast_vtable_constant;
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
			ast->vtable = &ptrs_ast_vtable_stringformat;
			ast->arg.strformat.str = str;
			ast->arg.strformat.insertions = insertions;
			ast->arg.strformat.insertionCount = insertionCount;
		}
		else
		{
			ast->vtable = &ptrs_ast_vtable_constant;
			ast->arg.constval.meta.type = PTRS_TYPE_POINTER;
			ast->arg.constval.value.ptrval = str;
			ast->arg.constval.meta.array.size = len;
			ast->arg.constval.meta.array.typeIndex = PTRS_NATIVETYPE_INDEX_CHAR;
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
		ast->vtable = &ptrs_ast_vtable_constant;
		ast->arg.constval.meta.type = PTRS_TYPE_POINTER;
		ast->arg.constval.value.ptrval = str;
		ast->arg.constval.meta.array.size = len;
		ast->arg.constval.meta.array.typeIndex = PTRS_NATIVETYPE_INDEX_CHAR;
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

			if(code->curr == ',' || code->curr == ':'
				|| (lookahead(code, ")") && (lookahead(code, "->") || lookahead(code, ":"))))
			{
				code->pos = start;
				code->curr = code->src[start];

				symbolScope_increase(code, true);

				ast = talloc(ptrs_ast_t);
				ast->vtable = &ptrs_ast_vtable_function;
				ast->arg.function.isExpression = true;

				ptrs_function_t *func = &ast->arg.function.func;
				func->name = "(lambda expression)";
				addSymbol(code, strdup("this"), &func->thisVal);
				func->args = parseArgumentDefinitionList(code, &func->vararg, &func->retType);

				consume(code, "->");
				if(code->curr == '{')
				{
					parseFunctionBody(code, func);
					symbolScope_decrease(code);
				}
				else
				{
					ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
					retStmt->vtable = &ptrs_ast_vtable_return;
					retStmt->arg.astval = parseExpression(code, true);
					func->body = retStmt;
					func->usesTryCatch = false;

					symbolScope_decrease(code);
				}

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

	ast->codepos = pos;
	ast->code = code->src;
	ast->file = code->filename;

	ptrs_ast_t *old;
	do
	{
		old = ast;
		ast = parseUnaryExtension(code, ast, ignoreCalls);
	} while(ast != old);

	return ast;
}

static ptrs_ast_t *parseUnaryExtension(code_t *code, ptrs_ast_t *ast, bool ignoreCalls)
{
	char curr = code->curr;
	if(curr == '.' && code->src[code->pos + 1] != '.')
	{
		ptrs_ast_t *member = talloc(ptrs_ast_t);
		member->vtable = &ptrs_ast_vtable_member;
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
				parseTyping(code, &call->arg.call.typing);
			}
			else
			{
				code->pos = pos;
				code->curr = code->src[pos];
				return parseUnaryExtension(code, ast, true);
			}
		}
		else
		{
			call = talloc(ptrs_ast_t);
			call->arg.call.typing.meta.type = -1;
			call->arg.call.typing.nativetype = NULL;
		}

		consumec(code, '(');

		call->vtable = &ptrs_ast_vtable_call;
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
			indexExpr->vtable = &ptrs_ast_vtable_slice;
			indexExpr->arg.slice.base = ast;
			indexExpr->arg.slice.start = expr;
			indexExpr->arg.slice.end = parseExpression(code, true);
		}
		else
		{
			indexExpr->vtable = &ptrs_ast_vtable_index;
			indexExpr->arg.binary.left = ast;
			indexExpr->arg.binary.right = expr;
		}

		ast = indexExpr;
		code->insideIndex = insideIndex;
		consumec(code, ']');
	}
	else
	{
		int pos = code->pos;
		ptrs_ast_vtable_t *vtable = readSuffixOperator(code, NULL);
		if(vtable != NULL)
		{
			ptrs_ast_t *opAst = talloc(ptrs_ast_t);
			opAst->codepos = pos;
			opAst->code = code->src;
			opAst->file = code->filename;
			opAst->arg.astval = ast;
			opAst->vtable = vtable;
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
		if(lookahead(code, "_"))
		{
			curr->entry = NULL;
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

static void parseTyping(code_t *code, ptrs_typing_t *typing)
{
	memset(&typing->meta, 0, sizeof(ptrs_meta_t));
	typing->nativetype = NULL;
	
	typing->meta.type = readTypeName(code);
	if(typing->meta.type != PTRS_NUM_TYPES)
		return;

	typing->nativetype = readNativeType(code);
	if(typing->nativetype != NULL)
	{
		if(code->curr == '[' || code->curr == '*')
		{
			parseArrayTyping(code, typing->nativetype, &typing->meta, NULL);
			typing->nativetype = NULL;
		}
		else
		{
			typing->meta.type = typing->nativetype->varType;
		}
		return;
	}

	int oldpos = code->pos;

	char *name = readIdentifier(code);
	ptrs_typing_t *type = getType(code, name);
	free(name);
	if(type != NULL)
	{
		memcpy(typing, type, sizeof(ptrs_typing_t));
		return;
	}

	code->pos = oldpos;
	code->curr = code->src[oldpos];
	unexpected(code, "type name");
}

static void parseOptionalTyping(code_t *code, ptrs_typing_t *typing)
{
	if(lookahead(code, ":"))
	{
		parseTyping(code, typing);
	}
	else
	{
		typing->meta.type = (uint8_t)-1;
		typing->nativetype = NULL;
	}
}

static void parseArrayTyping(code_t *code, ptrs_nativetype_info_t *nativeType, ptrs_meta_t *result, struct ptrs_ast **sizePtr)
{
	result->type = PTRS_TYPE_POINTER;

	if(nativeType == NULL)
		unexpected(code, "native type name");

	result->array.typeIndex = nativeType - ptrs_nativeTypes;

	if(lookahead(code, "*"))
	{
		result->array.size = 1;
		if(sizePtr != NULL)
			*sizePtr = NULL;
		return;
	}

	consumec(code, '[');

	int oldpos = code->pos;
	struct ptrs_ast *size = parseExpression(code, true);
	if(size->vtable == &ptrs_ast_vtable_constant)
	{
		result->array.size = size->arg.constval.value.intval;
		if(sizePtr != NULL)
			*sizePtr = NULL;
	}
	else if(sizePtr != NULL)
	{
		*sizePtr = size;
		result->array.size = 0;
	}
	else
	{
		code->pos = oldpos;
		code->curr = code->src[oldpos];
		unexpected(code, sizePtr == NULL ? "constant array size" : "array size");
	}
	consumec(code, ']');
}


static void parseImport(code_t *code, ptrs_ast_t *stmt)
{
	stmt->vtable = &ptrs_ast_vtable_import;
	stmt->arg.import.imports = NULL;
	stmt->arg.import.lastImport = NULL;
	stmt->arg.import.count = 0;

	struct ptrs_importlist **nextPtr = &stmt->arg.import.imports;

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
			stmt->arg.import.lastImport = curr;
			*nextPtr = curr;
			nextPtr = &curr->next;

			curr->name = name;
			curr->next = NULL;

			struct symbollist *symbol = addSpecialSymbol(code, NULL, PTRS_SYMBOL_IMPORTED);
			symbol->arg.imported.import = stmt;
			symbol->arg.imported.index = stmt->arg.import.count++;

			if(code->curr == ':')
			{
				next(code);

				symbol->text = strdup(name);
				symbol->arg.imported.type = readNativeType(code);

				if(symbol->arg.imported.type == NULL)
					unexpected(code, "Native type name");
			}
			else
			{
				if(lookahead(code, "as"))
					symbol->text = readIdentifier(code);
				else
					symbol->text = strdup(curr->name);

				symbol->arg.imported.type = NULL;
			}
		}

		if(code->curr == ';')
		{
			stmt->arg.import.from = NULL;
			stmt->arg.import.isScriptImport = false;
			break;
		}
		else if(lookahead(code, "from"))
		{
			consumec(code, '"');
			stmt->arg.import.from = readString(code, NULL, NULL, NULL);

			const char *ending = strrchr(stmt->arg.import.from, '.');
			stmt->arg.import.isScriptImport = ending != NULL && strcmp(ending, ".ptrs") == 0;

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

	stmt->vtable = &ptrs_ast_vtable_switch;
	stmt->arg.switchcase.defaultCase = NULL;

	struct ptrs_ast_case first;
	first.next = NULL;

	struct ptrs_astlist *body = NULL;
	struct ptrs_astlist *bodyStart = NULL;
	struct ptrs_ast_case *cases = &first;
	size_t caseCount = 0;
	int64_t totalMin = INT64_MAX;
	int64_t totalMax = INT64_MIN;

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
				expr->vtable = &ptrs_ast_vtable_body;
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
					if(expr->vtable != &ptrs_ast_vtable_constant || expr->arg.constval.meta.type != PTRS_TYPE_INT)
						PTRS_HANDLE_ASTERROR(expr, "Expected integer constant");

					currCase->next = talloc(struct ptrs_ast_case);
					currCase = currCase->next;
					caseCount++;

					currCase->min = expr->arg.constval.value.intval;
					free(expr);

					if(lookahead(code, ".."))
					{
						expr = parseExpression(code, true);
						if(expr->vtable != &ptrs_ast_vtable_constant || expr->arg.constval.meta.type != PTRS_TYPE_INT)
							PTRS_HANDLE_ASTERROR(expr, "Expected integer constant");

						currCase->max = expr->arg.constval.value.intval;
						free(expr);
					}
					else
					{
						currCase->max = currCase->min;
					}

					if(currCase->min < totalMin)
						totalMin = currCase->min;
					if(currCase->max > totalMax)
						totalMax = currCase->max;

					if(code->curr == ':')
						break;
					consumec(code, ',');
				}
				currCase->next = NULL;
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

	stmt->arg.switchcase.caseCount = caseCount;
	stmt->arg.switchcase.min = totalMin;
	stmt->arg.switchcase.max = totalMax;
	stmt->arg.switchcase.cases = first.next;
}

static ptrs_ast_t *parseNew(code_t *code, bool onStack)
{
	ptrs_ast_t *ast = talloc(ptrs_ast_t);

	ptrs_nativetype_info_t *nativeType = readNativeType(code);
	if(nativeType != NULL)
	{
		ast->vtable = &ptrs_ast_vtable_array;
		ast->arg.definearray.onStack = false;
		ast->arg.definearray.location.val = NULL;
		ast->arg.definearray.location.meta = NULL;

		parseArrayTyping(code, nativeType, &ast->arg.definearray.meta, &ast->arg.definearray.length);

		if(lookahead(code, "["))
		{
			ast->arg.definearray.initVal = parseExpressionList(code, ']');
			consumec(code, ']');
		}
	}
	else
	{
		ast->vtable = &ptrs_ast_vtable_new;
		ast->arg.define.location.constType = PTRS_TYPE_STRUCT;
		ast->arg.newexpr.onStack = onStack;
		ast->arg.newexpr.value = parseUnaryExpr(code, true);

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
		size_t i = ptrs_struct_hashName(curr->member.name) % count;
		while(member[i].name != NULL)
		{
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

	ast->vtable = &ptrs_ast_vtable_new;
	ast->arg.newexpr.value = structExpr;
	ast->arg.newexpr.arguments = NULL;

	structExpr->vtable = &ptrs_ast_vtable_struct;
	structExpr->codepos = code->pos;
	structExpr->code = code->src;
	structExpr->file = code->filename;

	struc->location = NULL;
	struc->size = 0;
	struc->name = "(map)";
	struc->overloads = NULL;
	struc->staticData = NULL;

	consumec(code, '{');
	symbolScope_increase(code, false);

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
			symbolScope_increase(code, true);

			ptrs_function_t *func = curr->member.value.function.ast = talloc(ptrs_function_t);
			addSymbol(code, strdup("this"), &func->thisVal);
			func->name = "(map function member)";
			func->args = parseArgumentDefinitionList(code, &func->vararg, &func->retType);

			consume(code, "->");
			if(lookahead(code, "{"))
			{
				func->body = parseStmtList(code, '}');
				consumec(code, '}');
			}
			else
			{
				ptrs_ast_t *retStmt = talloc(ptrs_ast_t);
				retStmt->vtable = &ptrs_ast_vtable_return;
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

	struc->lastCodepos = code->pos;
}

static ptrs_funcparameter_t *createParameterList(code_t *code, size_t count, ...)
{
	va_list ap;
	va_start(ap, count);

	ptrs_funcparameter_t *first;
	ptrs_funcparameter_t **next = &first;

	while(count-- > 0)
	{
		ptrs_funcparameter_t *curr = talloc(ptrs_funcparameter_t);
		*next = curr;
		next = &curr->next;

		curr->name = va_arg(ap, char *);
		curr->arg.val = NULL;
		curr->arg.meta = NULL;
		curr->arg.constType = -1;
		curr->typing.meta.type = va_arg(ap, ptrs_vartype_t);
		curr->typing.nativetype = NULL;
		curr->argv = NULL;
		curr->next = NULL;

		if(curr->typing.meta.type == PTRS_TYPE_POINTER)
		{
			curr->typing.meta.array.typeIndex = va_arg(ap, size_t);
			curr->typing.meta.array.size = va_arg(ap, size_t);
		}

		if(curr->name != NULL)
			addSymbol(code, strdup(curr->name), &curr->arg);
	}

	return first;
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
		if(oldAst->vtable != &ptrs_ast_vtable_identifier)
			PTRS_HANDLE_ASTERROR(NULL, "Cannot redefine special symbol %s as a struct", structName);

		struc->location = oldAst->arg.varval;
		free(oldAst);
	}
	else
	{
		struc->location = talloc(ptrs_jit_var_t);
		addSymbol(code, strdup(structName), struc->location);
	}

	ptrs_typing_t type;
	type.meta.type = PTRS_TYPE_STRUCT;
	ptrs_meta_setPointer(type.meta, struc);
	type.nativetype = NULL;
	addType(code, strdup(structName), &type);

	struc->name = structName;
	struc->overloads = NULL;
	struc->size = 0;
	consumec(code, '{');

	symbolScope_increase(code, false);
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
			symbolScope_increase(code, true);

			const char *nameFormat = NULL;
			char *param0Name = NULL;
			char *param1Name = NULL;

			ptrs_function_t *func = talloc(ptrs_function_t);
			addSymbol(code, strdup("this"), &func->thisVal);
			func->vararg = NULL;

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
				if(curr == '[')
				{
					next(code);
					param0Name = readIdentifier(code);
					consumec(code, ']');

					if(isAddressOf)
					{
						func->args = createParameterList(code, 1, param0Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_CHAR, 0);
						parseOptionalTyping(code, &func->retType);

						nameFormat = "%s.op &this[%s]";
						overload->op = ptrs_ast_vtable_member.addressof;
					}
					else if(lookahead(code, "="))
					{
						param1Name = readIdentifier(code);
						func->args = createParameterList(code, 2,
							param0Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_CHAR, 0,
							param1Name, (ptrs_vartype_t)-1
						);
						func->retType.meta.type = -1;

						nameFormat = "%s.op this[%s] = %s";
						overload->op = ptrs_ast_vtable_member.set;
					}
					else if(code->curr == '(')
					{
						func->args = parseArgumentDefinitionList(code, &func->vararg, &func->retType);
						parseOptionalTyping(code, &func->retType);

						ptrs_funcparameter_t *nameArg = talloc(ptrs_funcparameter_t);
						nameArg->name = param0Name;
						nameArg->arg.val = NULL;
						nameArg->arg.meta = NULL;
						nameArg->arg.constType = -1;
						nameArg->argv = NULL;
						nameArg->next = func->args;

						func->args = nameArg;
						addSymbol(code, strdup(param0Name), &nameArg->arg);

						nameFormat = "%s.op this[%s]()";
						overload->op = ptrs_ast_vtable_member.call;
					}
					else
					{
						func->args = createParameterList(code, 1, param0Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_CHAR, 0);
						parseOptionalTyping(code, &func->retType);

						nameFormat = "%s.op this[%s]";
						overload->op = ptrs_ast_vtable_member.get;
					}
				}
				else if(curr == '(')
				{
					func->args = parseArgumentDefinitionList(code, &func->vararg, &func->retType);
					parseOptionalTyping(code, &func->retType);
					overload->op = ptrs_ast_vtable_call.get;

					nameFormat = "%s.op this()";
				}
				else
				{
					unexpected(code, "missing member overload");
				}
			}
			else if(lookahead(code, "sizeof"))
			{
				consume(code, "this");

				nameFormat = "%s.op sizeof this";
				overload->op = ptrs_ast_vtable_prefix_sizeof.get;

				func->args = NULL;
				func->retType.meta.type = PTRS_TYPE_INT;
			}
			else if(lookahead(code, "foreach"))
			{
				consumec(code, '(');
				param0Name = readIdentifier(code);
				consumec(code, ',');
				param1Name = readIdentifier(code);
				consumec(code, ')');

				consume(code, "in");
				consume(code, "this");

				nameFormat = "%s.op foreach(%s, %s) in this";
				overload->op = ptrs_ast_vtable_forin_step.get;

				func->args = createParameterList(code, 2,
					param0Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_VAR, 1,
					param1Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_VAR, 1
				);
				func->retType.meta.type = PTRS_TYPE_INT;
			}
			else if(lookahead(code, "cast"))
			{
				consumec(code, '<');

				if(lookahead(code, "string"))
				{
					param0Name = NULL;
					nameFormat = "%s.op cast<string>this";
					overload->op = ptrs_ast_vtable_tostring.get;

					func->args = NULL;
					func->retType.meta.type = PTRS_TYPE_POINTER;
					func->retType.meta.array.typeIndex = PTRS_NATIVETYPE_INDEX_CHAR;
					func->retType.meta.array.size = 0;
				}
				else
				{
					param0Name = readIdentifier(code);
					nameFormat = "%s.op cast<%s>this";
					overload->op = ptrs_ast_vtable_cast_builtin.get;

					func->args = createParameterList(code, 1, param0Name, PTRS_TYPE_INT);
					// TODO add seprate overloads for cast<int> and cast<float>?
					func->retType.meta.type = -1;
				}

				consumec(code, '>');
				consume(code, "this");
			}
			else
			{
				param0Name = readIdentifier(code);
				if(lookahead(code, "in"))
				{
					consume(code, "this");
					nameFormat = "%s.op %s in this";
					overload->op = ptrs_ast_vtable_op_in.get;

					func->args = createParameterList(code, 1, param0Name, PTRS_TYPE_POINTER, PTRS_NATIVETYPE_INDEX_CHAR, 0);
					func->retType.meta.type = PTRS_TYPE_INT;
				}
			}

			if(overload->op == NULL)
				unexpected(code, "Operator");

			func->name = malloc(snprintf(NULL, 0, nameFormat, structName, param0Name, param1Name) + 1);
			sprintf(func->name, nameFormat, structName, param0Name, param1Name);

			parseFunctionBody(code, func);
			symbolScope_decrease(code);

			overload->handler = func;
			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}
		else if(lookahead(code, "constructor"))
		{
			name = malloc(structNameLen + strlen(".op constructor") + 1);
			sprintf(name, "%s.op constructor", structName);

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->op = ptrs_ast_vtable_new.get;
			overload->handler = parseFunction(code, name);
			overload->isStatic = isStatic;

			overload->next = struc->overloads;
			struc->overloads = overload;
			continue;
		}
		else if(lookahead(code, "destructor"))
		{
			name = malloc(structNameLen + strlen(".op destructor") + 1);
			sprintf(name, "%s.op destructor", structName);

			struct ptrs_opoverload *overload = talloc(struct ptrs_opoverload);
			overload->op = ptrs_ast_vtable_delete.get;
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

		addSpecialSymbol(code, strdup(name), PTRS_SYMBOL_THISMEMBER);

		if(isProperty > 0)
		{
			symbolScope_increase(code, true);

			ptrs_function_t *func = talloc(ptrs_function_t);
			func->name = malloc(structNameLen + strlen(name) + 6);
			addSymbol(code, strdup("this"), &func->thisVal);
			func->vararg = NULL;

			if(isProperty == 1)
			{
				curr->type = PTRS_STRUCTMEMBER_GETTER;

				func->args = NULL;
				parseOptionalTyping(code, &func->retType);
				sprintf(func->name, "%s.get %s", structName, name);
			}
			else
			{
				curr->type = PTRS_STRUCTMEMBER_SETTER;

				func->args = createParameterList(code, 1, "value", (ptrs_vartype_t)-1);
				func->retType.meta.type = -1;
				sprintf(func->name, "%s.set %s", structName, name);
			}

			parseFunctionBody(code, func);
			curr->value.function.ast = func;

			symbolScope_decrease(code);
		}
		else if(code->curr == '(')
		{
			char *funcName = malloc(structNameLen + strlen(name) + 2);
			sprintf(funcName, "%s.%s", structName, name);

			curr->type = PTRS_STRUCTMEMBER_FUNCTION;
			curr->value.function.ast = parseFunction(code, funcName);
		}
		else if(code->curr == ':')
		{
			consumec(code, ':');
			ptrs_nativetype_info_t *type = readNativeType(code);

			if(code->curr == '[')
			{
				curr->type = PTRS_STRUCTMEMBER_ARRAY;
				parseArrayTyping(code, type, &curr->value.array, NULL);
				size_t arraySize = type->size * curr->value.array.array.size;

				if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_TYPED)
				{
					if(old->value.type->size < type->size)
						curr->offset = (old->offset & ~(type->size - 1)) + type->size;
					else
						curr->offset = old->offset + old->value.type->size;
				}
				else if(old != NULL && old->isStatic == curr->isStatic && old->type == PTRS_STRUCTMEMBER_ARRAY)
				{
					ptrs_nativetype_info_t *oldType = &ptrs_nativeTypes[old->value.array.array.typeIndex];
					size_t oldByteSize = oldType->size * old->value.array.array.size;
					curr->offset = ((old->offset + oldByteSize) & ~(type->size - 1)) + type->size;
				}
				else
				{
					curr->offset = currSize;
				}

				currSize = curr->offset + arraySize;
				if((currSize & 7) != 0)
					currSize = (currSize & ~7) + 8;
			}
			else
			{
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
					ptrs_nativetype_info_t *oldType = &ptrs_nativeTypes[old->value.array.array.typeIndex];
					size_t oldByteSize = oldType->size * old->value.array.array.size;
					curr->offset = ((old->offset + oldByteSize) & ~(type->size - 1)) + type->size;
				}
				else
				{
					curr->offset = currSize;
				}
				currSize = (curr->offset & ~7) + 8;
			}
			consumec(code, ';');
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

	struc->lastCodepos = code->pos;
}

const char * const typeNames[] = {
	[PTRS_TYPE_UNDEFINED] = "undefined",
	[PTRS_TYPE_INT] = "int",
	[PTRS_TYPE_FLOAT] = "float",
	[PTRS_TYPE_POINTER] = "pointer",
	[PTRS_TYPE_STRUCT] = "struct",
	[PTRS_TYPE_FUNCTION] = "function",
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

	return PTRS_NUM_TYPES;
}

static ptrs_nativetype_info_t *readNativeType(code_t *code)
{
	for(int i = 0; i < ptrs_nativeTypeCount; i++)
	{
		if(lookahead(code, ptrs_nativeTypes[i].name))
			return &ptrs_nativeTypes[i];
	}
	return NULL;
}

static ptrs_ast_vtable_t *readOperatorFrom(code_t *code, const char **label, struct opinfo *ops, int opCount)
{
	for(int i = 0; i < opCount; i++)
	{
		if(lookahead(code, ops[i].op))
		{
			if(label != NULL)
				*label = ops[i].op;
			return ops[i].vtable;
		}
	}
	return NULL;
}
static ptrs_ast_vtable_t *readPrefixOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, prefixOps, prefixOpCount);
}
static ptrs_ast_vtable_t *readSuffixOperator(code_t *code, const char **label)
{
	return readOperatorFrom(code, label, suffixedOps, suffixedOpCount);
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
	int buffSize = 32;
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

				if(stmt->lastImport == NULL)
					stmt->imports = import;
				else
					stmt->lastImport->next = import;
				stmt->lastImport = import;

				import->name = strdup(text);
				import->next = NULL;

				struct symbollist *entry = talloc(struct symbollist);
				entry->text = strdup(text);
				entry->type = PTRS_SYMBOL_IMPORTED;
				entry->arg.imported.import = curr->importStmt;
				entry->arg.imported.index = stmt->count++;

				entry->next = symbols->current;
				symbols->current = entry;

				return getSymbol(code, text);
			}

			curr = curr->next;
		}

		symbols = symbols->outer;
	}

	return NULL;
}

static ptrs_ast_t *getSymbol(code_t *code, char *text)
{
	ptrs_ast_t *ast = NULL;
	if(ptrs_ast_getSymbol(code->symbols, text, &ast) == 0)
	{
		ast->file = code->filename;
		ast->codepos = code->pos;
		return ast;
	}

	ast = getSymbolFromWildcard(code, text);
	if(ast != NULL)
		return ast;

	char buff[128];
	sprintf(buff, "Unknown identifier %s", text);
	unexpectedm(code, NULL, buff);
	return ast; //doh
}

static void addType(code_t *code, char *name, ptrs_typing_t *type)
{
	struct typelist *curr = talloc(struct typelist);
	curr->name = name;
	memcpy(&curr->type, type, sizeof(ptrs_typing_t));

	curr->next = code->symbols->types;
	code->symbols->types = curr;
}

static ptrs_typing_t *getType(code_t *code, const char *name)
{
	ptrs_symboltable_t *symbols = code->symbols;
	while(symbols != NULL)
	{
		struct typelist *curr = symbols->types;
		while(curr != NULL)
		{
			if(strcmp(curr->name, name) == 0)
				return &curr->type;
			
			curr = curr->next;
		}

		symbols = symbols->outer;
	}

	return NULL;
}

static void symbolScope_increase(code_t *code, bool functionBoundary)
{
	ptrs_symboltable_t *new = talloc(ptrs_symboltable_t);
	new->outer = code->symbols;
	new->current = NULL;
	new->wildcards = NULL;
	new->types = NULL;
	new->functionBoundary = functionBoundary;

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

	struct typelist *currt = scope->types;
	while(curr != NULL)
	{
		struct typelist *old = currt;
		currt = currt->next;
		free(old->name);
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
		while(curr != '\n' && curr != 0)
		{
			pos++;
			curr = code->src[pos];
		}

		code->pos = pos;
		if(curr != 0)
			code->pos++;
		code->curr = curr;
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
		code->curr = curr;
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
