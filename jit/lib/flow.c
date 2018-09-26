#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/conversion.h"
#include "../include/error.h"
#include "../jit.h"

typedef struct ptrs_flowprediction
{
	ptrs_jit_var_t *variable;
	int8_t type;
	uint8_t multipleTypes : 1;
	uint8_t lockPrediction : 1;
	uint8_t isFunctionParameter : 1;
	union
	{
		struct ptrs_ast_define *variable;
		ptrs_funcparameter_t *parameter;
	} definition;
	struct ptrs_flowprediction *next;
} ptrs_predictions_t;

typedef struct
{
	bool updatePredictions;
	ptrs_predictions_t *predictions;
	//...
} ptrs_flow_t;

static int8_t analyzeExpression(ptrs_flow_t *flow, ptrs_ast_t *node);
static void analyzeStatement(ptrs_flow_t *flow, ptrs_ast_t *node);

static ptrs_predictions_t *dupPredictions(ptrs_predictions_t *curr)
{
	ptrs_predictions_t *start;
	ptrs_predictions_t **ptr = &start;

	while(curr != NULL)
	{
		*ptr = malloc(sizeof(ptrs_predictions_t));
		memcpy(*ptr, curr, sizeof(ptrs_predictions_t));

		curr = curr->next;
		ptr = &((*ptr)->next);
	}

	*ptr = NULL;
	return start;
}

static void applyAndFreePredictions(ptrs_predictions_t *curr)
{
	while(curr != NULL)
	{
		if(curr->isFunctionParameter)
		{
			if(curr->definition.parameter != NULL && !curr->multipleTypes)
				curr->definition.parameter->type = curr->type;
		}
		else
		{
			if(curr->definition.variable != NULL)
				curr->definition.variable->type = curr->multipleTypes ? -1 : curr->type;
		}

		ptrs_predictions_t *next = curr->next;
		free(curr);
		curr = next;
	}
}

static void mergePredictions(ptrs_flow_t *dest, ptrs_predictions_t *src)
{
	while(src != NULL)
	{
		bool found = false;
		ptrs_predictions_t *curr = dest->predictions;
		while(curr != NULL)
		{
			if(curr->variable == src->variable)
			{
				if(curr->type != src->type)
				{
					curr->multipleTypes = true;
					curr->type = -1;
				}

				found = true;
				break;
			}

			curr = curr->next;
		}

		curr = src;
		src = src->next;

		if(found)
		{
			free(curr);
		}
		else
		{
			curr->next = dest->predictions;
			dest->predictions = curr;
		}
	}
}

static void createPrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var,
	int8_t type, bool isParameter, void *definition)
{
	if(!flow->updatePredictions)
		return;

	ptrs_predictions_t *curr = malloc(sizeof(ptrs_predictions_t));
	curr->variable = var;
	curr->type = type;
	curr->multipleTypes = type == -1;
	curr->lockPrediction = false;
	curr->isFunctionParameter = isParameter;
	if(isParameter)
		curr->definition.parameter = definition;
	else
		curr->definition.variable = definition;

	curr->next = flow->predictions;
	flow->predictions = curr;
}
static void setPrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var, int8_t type)
{
	if(!flow->updatePredictions)
		return;

	ptrs_predictions_t *curr = flow->predictions;
	if(curr == NULL)
	{
		if(type == -1)
			return;

		curr = malloc(sizeof(ptrs_predictions_t));
		flow->predictions = curr;
	}
	else
	{
		ptrs_predictions_t **prev = &flow->predictions;
		for(; curr->next != NULL; prev = &curr->next, curr = curr->next)
		{
			if(curr->variable == var)
			{
				if(curr->type != type && !curr->lockPrediction)
				{
					curr->type = type;
					curr->multipleTypes = true;
				}
				return;
			}
		}

		curr->next = malloc(sizeof(ptrs_predictions_t));
		curr = curr->next;
	}

	curr->variable = var;
	curr->type = type;
	curr->multipleTypes = type == -1;
	curr->lockPrediction = false;
	curr->isFunctionParameter = false;
	curr->definition.variable = NULL;
	curr->next = NULL;
}
static int8_t getPrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var)
{
	ptrs_predictions_t *curr = flow->predictions;
	while(curr != NULL)
	{
		if(curr->variable == var)
			return curr->type;

		curr = curr->next;
	}

	return -1;
}
static void lockPrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var)
{
	ptrs_predictions_t *curr = flow->predictions;
	while(curr != NULL)
	{
		if(curr->variable == var)
		{
			curr->lockPrediction = true;
			curr->multipleTypes = true;
			curr->type = -1;
			return;
		}

		curr = curr->next;
	}
}

static void analyzeList(ptrs_flow_t *flow, struct ptrs_astlist *list)
{
	while(list != NULL)
	{
		analyzeExpression(flow, list->entry);
		list = list->next;
	}
}

static void analyzeFunction(ptrs_flow_t *flow, ptrs_function_t *ast)
{
	ptrs_funcparameter_t *curr = ast->args;
	for(; curr != NULL; curr = curr->next)
	{
		createPrediction(flow, &curr->arg, curr->type, true, curr);
	}

	createPrediction(flow, &ast->thisVal, PTRS_TYPE_STRUCT, true, NULL);
	createPrediction(flow, ast->vararg, PTRS_TYPE_POINTER, true, NULL);

	analyzeStatement(flow, ast->body);
}

static void analyzeLValue(ptrs_flow_t *flow, ptrs_ast_t *node, int8_t type)
{
	if(node->vtable == &ptrs_ast_vtable_identifier)
	{
		setPrediction(flow, node->arg.varval, type);
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_dereference)
	{
		analyzeExpression(flow, node->arg.astval);
	}
	else if(node->vtable == &ptrs_ast_vtable_index)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		analyzeExpression(flow, expr->left);
		analyzeExpression(flow, expr->right);
	}
	else if(node->vtable == &ptrs_ast_vtable_member)
	{
		struct ptrs_ast_member *expr = &node->arg.member;
		analyzeExpression(flow, expr->base);
	}
	else if(node->vtable == &ptrs_ast_vtable_importedsymbol)
	{
		//ignore
		//TODO this can cause problems when setting values from other scripts
		//	as they might be predicted as a different type
	}
	else
	{
		analyzeExpression(flow, node);
	}
}

static const void *comparasionHandler[] = {
	&ptrs_ast_vtable_op_typeequal,
	&ptrs_ast_vtable_op_typeinequal,
	&ptrs_ast_vtable_op_equal,
	&ptrs_ast_vtable_op_inequal,
	&ptrs_ast_vtable_op_lessequal,
	&ptrs_ast_vtable_op_greaterequal,
	&ptrs_ast_vtable_op_less,
	&ptrs_ast_vtable_op_greater,
	&ptrs_ast_vtable_op_in,
	&ptrs_ast_vtable_op_instanceof,
	&ptrs_ast_vtable_op_logicor,
	&ptrs_ast_vtable_op_logicand,
	&ptrs_ast_vtable_op_logicxor,
};
static const void *binaryIntOnlyHandler[] = {
	&ptrs_ast_vtable_op_or,
	&ptrs_ast_vtable_op_xor,
	&ptrs_ast_vtable_op_and,
	&ptrs_ast_vtable_op_ushr,
	&ptrs_ast_vtable_op_sshr,
	&ptrs_ast_vtable_op_shl,
	&ptrs_ast_vtable_op_mod
};
static const void *binaryIntFloatHandler[] = {
	&ptrs_ast_vtable_op_mul,
	&ptrs_ast_vtable_op_div
};
static const void *unaryIntFloatHandler[] = {
	&ptrs_ast_vtable_prefix_inc,
	&ptrs_ast_vtable_prefix_dec,
	&ptrs_ast_vtable_prefix_not,
	&ptrs_ast_vtable_prefix_plus,
	&ptrs_ast_vtable_prefix_minus,
	&ptrs_ast_vtable_suffix_inc,
	&ptrs_ast_vtable_suffix_dec
};

#define typecomp(a, b) ((PTRS_TYPE_##a << 3) | PTRS_TYPE_##b)
#define calc_typecomp(a, b) ((a << 3) | b)

static int8_t analyzeExpression(ptrs_flow_t *flow, ptrs_ast_t *node)
{
	if(node == NULL)
		return PTRS_TYPE_UNDEFINED;

	if(node->vtable == &ptrs_ast_vtable_constant)
	{
		return node->arg.constval.meta.type;
	}
	else if(node->vtable == &ptrs_ast_vtable_identifier)
	{
		struct ptrs_ast_identifier *expr = &node->arg.identifier;

		expr->predictedType = getPrediction(flow, expr->location);
		return expr->predictedType;
	}
	else if(node->vtable == &ptrs_ast_vtable_functionidentifier)
	{
		return PTRS_TYPE_FUNCTION;
	}
	else if(node->vtable == &ptrs_ast_vtable_array)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		if(stmt->isInitExpr)
			analyzeExpression(flow, stmt->initExpr);

		if(stmt->value != NULL) //size
			analyzeExpression(flow, stmt->value);

		if(!stmt->isInitExpr)
			analyzeList(flow, stmt->initVal);

		return PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_vararray)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		if(stmt->value != NULL) //size
			analyzeExpression(flow, stmt->value);

		analyzeList(flow, stmt->initVal);

		return PTRS_TYPE_POINTER;
	}
	else if(node->vtable == &ptrs_ast_vtable_function)
	{
		analyzeFunction(flow, &node->arg.function.func);
		return PTRS_TYPE_FUNCTION;
	}
	else if(node->vtable == &ptrs_ast_vtable_call)
	{
		struct ptrs_ast_call *expr = &node->arg.call;
		int8_t value = analyzeExpression(flow, expr->value);

		analyzeList(flow, expr->arguments);

		if(value == PTRS_TYPE_NATIVE && expr->retType != NULL)
			return expr->retType->varType;
		else if(value == PTRS_TYPE_NATIVE)
			return PTRS_TYPE_INT;
		else
			return -1;
	}
	else if(node->vtable == &ptrs_ast_vtable_stringformat)
	{
		struct ptrs_ast_strformat *expr = &node->arg.strformat;

		struct ptrs_stringformat *curr = expr->insertions;
		while(curr != NULL)
		{
			analyzeExpression(flow, curr->entry);
			curr = curr->next;
		}

		return PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_new)
	{
		struct ptrs_ast_new *expr = &node->arg.newexpr;
		analyzeExpression(flow, expr->value);
		analyzeList(flow, expr->arguments);

		return PTRS_TYPE_STRUCT;
	}
	else if(node->vtable == &ptrs_ast_vtable_member)
	{
		struct ptrs_ast_member *expr = &node->arg.member;
		analyzeExpression(flow, expr->base);
		return -1; //TODO
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_sizeof)
	{
		analyzeExpression(flow, node->arg.astval);
		return PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_address)
	{
		ptrs_ast_t *target = node->arg.astval;

		//TODO do we need analyzeAddrOfExpression?
		analyzeExpression(flow, target);

		//TODO &ptrs_ast_vtable_member
		if(target->vtable == &ptrs_ast_vtable_identifier)
		{
			struct ptrs_ast_identifier *expr = &target->arg.identifier;
			lockPrediction(flow, expr->location);
			return PTRS_TYPE_POINTER;
		}
		//TODO &ptrs_ast_vtable_importedsymbol
		else
		{
			return -1;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_dereference)
	{
		int8_t target = analyzeExpression(flow, node->arg.astval);

		if(target == PTRS_TYPE_NATIVE)
			return PTRS_TYPE_INT;
		else if(target == PTRS_TYPE_POINTER || target == -1)
			return -1;
		else
			return -2;
	}
	else if(node->vtable == &ptrs_ast_vtable_indexlength)
	{
		return PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_index)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		int8_t base = analyzeExpression(flow, expr->left);
		int8_t index = analyzeExpression(flow, expr->right);

		if(base == PTRS_TYPE_NATIVE)
			return PTRS_TYPE_INT;
		else if(base == PTRS_TYPE_POINTER || base == PTRS_TYPE_STRUCT)
			return -1;
		else
			return -2;
	}
	else if(node->vtable == &ptrs_ast_vtable_slice)
	{
		struct ptrs_ast_slice *expr = &node->arg.slice;

		int8_t base = analyzeExpression(flow, expr->base);
		analyzeExpression(flow, expr->start);
		analyzeExpression(flow, expr->end);

		if(base == PTRS_TYPE_NATIVE)
			return PTRS_TYPE_NATIVE;
		else if(base == PTRS_TYPE_POINTER)
			return PTRS_TYPE_POINTER;
		else if(base == -1)
			return -1;
		else
			return -2;
	}
	else if(node->vtable == &ptrs_ast_vtable_as
		|| node->vtable == &ptrs_ast_vtable_cast_builtin)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;

		analyzeExpression(flow, expr->value);
		return expr->builtinType;
	}
	else if(node->vtable == &ptrs_ast_vtable_tostring)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;
		analyzeExpression(flow, expr->value);

		return PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_cast)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;
		analyzeExpression(flow, expr->value);

		return PTRS_TYPE_STRUCT;
	}
	else if(node->vtable == &ptrs_ast_vtable_importedsymbol)
	{
		struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;
		struct ptrs_ast_import *stmt = &expr->import->arg.import;

		if(stmt->isScriptImport)
			return -1;
		else if(expr->type == NULL)
			return PTRS_TYPE_NATIVE;
		else
			return expr->type->varType;
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_typeof)
	{
		analyzeExpression(flow, node->arg.astval);

		return PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_yield)
	{
		struct ptrs_ast_yield *expr = &node->arg.yield;
		analyzeList(flow, expr->values);

		return PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_assign)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		analyzeLValue(flow, expr->left, analyzeExpression(flow, expr->right));
	}
	else if(node->vtable == &ptrs_ast_vtable_op_ternary)
	{
		struct ptrs_ast_ternary *expr = &node->arg.ternary;
		analyzeExpression(flow, expr->condition);
		int8_t left = analyzeExpression(flow, expr->trueVal);
		int8_t right = analyzeExpression(flow, expr->falseVal);

		if(left == right)
			return left;
		else
			return -1;
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_logicnot)
	{
		analyzeExpression(flow, node->arg.astval);
		return PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_add)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		int8_t left = analyzeExpression(flow, expr->left);
		int8_t right = analyzeExpression(flow, expr->right);

		switch(calc_typecomp(left, right))
		{
			case typecomp(INT, INT):
				return PTRS_TYPE_INT;

			case typecomp(INT, FLOAT):
			case typecomp(FLOAT, INT):
			case typecomp(FLOAT, FLOAT):
				return PTRS_TYPE_FLOAT;

			case typecomp(NATIVE, INT):
			case typecomp(POINTER, INT):
				return left;

			case typecomp(INT, NATIVE):
			case typecomp(INT, POINTER):
				return right;

			default:
				return -2;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_op_sub)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		int8_t left = analyzeExpression(flow, expr->left);
		int8_t right = analyzeExpression(flow, expr->right);

		switch(calc_typecomp(left, right))
		{
			case typecomp(INT, INT):
				return PTRS_TYPE_INT;

			case typecomp(INT, FLOAT):
			case typecomp(FLOAT, INT):
			case typecomp(FLOAT, FLOAT):
				return PTRS_TYPE_FLOAT;

			case typecomp(NATIVE, INT):
			case typecomp(POINTER, INT):
				return left;

			case typecomp(NATIVE, NATIVE):
			case typecomp(POINTER, POINTER):
				return PTRS_TYPE_INT;

			default:
				return -2;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_op_mul
		|| node->vtable == &ptrs_ast_vtable_op_div)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		int8_t left = analyzeExpression(flow, expr->left);
		int8_t right = analyzeExpression(flow, expr->right);

		switch(calc_typecomp(left, right))
		{
			case typecomp(INT, INT):
				return PTRS_TYPE_INT;

			case typecomp(INT, FLOAT):
			case typecomp(FLOAT, INT):
			case typecomp(FLOAT, FLOAT):
				return PTRS_TYPE_FLOAT;

			default:
				return -2;
		}
	}
	else
	{
		for(int i = 0; i < sizeof(comparasionHandler) / sizeof(void *); i++)
		{
			if(node->vtable == comparasionHandler[i])
			{
				struct ptrs_ast_binary *expr = &node->arg.binary;
				analyzeExpression(flow, expr->left);
				analyzeExpression(flow, expr->right);

				return PTRS_TYPE_INT;
			}
		}

		for(int i = 0; i < sizeof(binaryIntOnlyHandler) / sizeof(void *); i++)
		{
			if(node->vtable == binaryIntOnlyHandler[i])
			{
				struct ptrs_ast_binary *expr = &node->arg.binary;
				int8_t left = analyzeExpression(flow, expr->left);
				int8_t right = analyzeExpression(flow, expr->right);

				if(left == PTRS_TYPE_INT && right == PTRS_TYPE_INT)
					return PTRS_TYPE_INT;
				else
					return -2;
			}
		}

		for(int i = 0; i < sizeof(unaryIntFloatHandler) / sizeof(void *); i++)
		{
			if(node->vtable == unaryIntFloatHandler[i])
			{
				return analyzeExpression(flow, node->arg.astval);
			}
		}

		ptrs_error(node, "Cannot analyze expression");
		return -1; //derp
	}
}

#define loopPredictionsRemerge(body) \
	if(flow->updatePredictions) \
	{ \
		ptrs_predictions_t *predictions = dupPredictions(flow->predictions); \
		body \
		mergePredictions(flow, predictions); \
		\
		flow->updatePredictions = false; \
		body \
		flow->updatePredictions = true; \
		\
		/* TODO do we need to do this a third time? */ \
	} \
	else \
	{ \
		body \
	}

static void analyzeStatement(ptrs_flow_t *flow, ptrs_ast_t *node)
{
	if(node == NULL)
		return;

	if(node->vtable == &ptrs_ast_vtable_define)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;
		int8_t type = analyzeExpression(flow, stmt->value);

		createPrediction(flow, &stmt->location, type, false, stmt);
	}
	else if(node->vtable == &ptrs_ast_vtable_array)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		if(stmt->isInitExpr)
			analyzeExpression(flow, stmt->initExpr);

		if(stmt->value != NULL) //size
			analyzeExpression(flow, stmt->value);

		if(!stmt->isInitExpr)
			analyzeList(flow, stmt->initVal);

		setPrediction(flow, &stmt->location, PTRS_TYPE_NATIVE);
	}
	else if(node->vtable == &ptrs_ast_vtable_vararray)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		if(stmt->value != NULL) //size
			analyzeExpression(flow, stmt->value);

		analyzeList(flow, stmt->initVal);
		setPrediction(flow, &stmt->location, PTRS_TYPE_NATIVE);
	}
	else if(node->vtable == &ptrs_ast_vtable_import)
	{
		//TODO
	}
	else if(node->vtable == &ptrs_ast_vtable_return
		|| node->vtable == &ptrs_ast_vtable_delete
		|| node->vtable == &ptrs_ast_vtable_throw)
	{
		analyzeExpression(flow, node->arg.astval);
	}
	else if(node->vtable == &ptrs_ast_vtable_continue
		|| node->vtable == &ptrs_ast_vtable_break)
	{
		//nothing
	}
	else if(node->vtable == &ptrs_ast_vtable_trycatch)
	{
		//TODO
	}
	else if(node->vtable == &ptrs_ast_vtable_struct)
	{
		ptrs_struct_t *struc = &node->arg.structval;

		for(int i = 0; i < struc->memberCount; i++)
		{
			struct ptrs_structmember *curr = &struc->member[i];
			if(curr->name == NULL) //hashmap filler entry
				continue;

			if(curr->type == PTRS_STRUCTMEMBER_FUNCTION
				|| curr->type == PTRS_STRUCTMEMBER_GETTER
				|| curr->type == PTRS_STRUCTMEMBER_SETTER)
			{
				analyzeFunction(flow, curr->value.function.ast);
			}
		}

		struct ptrs_opoverload *curr = struc->overloads;
		while(curr != NULL)
		{
			analyzeFunction(flow, curr->handler);
			curr = curr->next;
		}

		createPrediction(flow, struc->location, PTRS_TYPE_STRUCT, false, NULL);
	}
	else if(node->vtable == &ptrs_ast_vtable_function)
	{
		analyzeFunction(flow, &node->arg.function.func);
	}
	else if(node->vtable == &ptrs_ast_vtable_if)
	{
		struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;
		analyzeExpression(flow, stmt->condition);

		ptrs_predictions_t *predictions;
		if(flow->updatePredictions)
		 	predictions = dupPredictions(flow->predictions);
		else
			predictions = flow->predictions;

		analyzeStatement(flow, stmt->ifBody);

		if(stmt->elseBody != NULL)
		{
			ptrs_predictions_t *ifPredictions = flow->predictions;
			flow->predictions = predictions;
			analyzeStatement(flow, stmt->elseBody);
			flow->predictions = ifPredictions;
		}

		if(flow->updatePredictions)
			mergePredictions(flow, predictions);
	}
	else if(node->vtable == &ptrs_ast_vtable_switch)
	{
		struct ptrs_ast_switch *stmt = &node->arg.switchcase;
		struct ptrs_ast_case *curr = stmt->cases;

		analyzeStatement(flow, stmt->defaultCase);

		while(curr)
		{
			ptrs_predictions_t *casePredictions;
			if(flow->updatePredictions)
				casePredictions = dupPredictions(flow->predictions);
			else
				casePredictions = flow->predictions;

			ptrs_predictions_t *defaultPredictions = flow->predictions;
			flow->predictions = casePredictions;
			analyzeStatement(flow, curr->body);

			flow->predictions = defaultPredictions;
			if(flow->updatePredictions)
				mergePredictions(flow, casePredictions);

			curr = curr->next;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_while)
	{
		struct ptrs_ast_control *stmt = &node->arg.control;

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->condition);
			analyzeStatement(flow, stmt->body);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_dowhile)
	{
		struct ptrs_ast_control *stmt = &node->arg.control;

		loopPredictionsRemerge(
			analyzeStatement(flow, stmt->body);
			analyzeExpression(flow, stmt->condition);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_for)
	{
		struct ptrs_ast_for *stmt = &node->arg.forstatement;

		analyzeStatement(flow, stmt->init);

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->condition);
			analyzeStatement(flow, stmt->body);
			analyzeExpression(flow, stmt->step);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_forin)
	{
		struct ptrs_ast_forin *stmt = &node->arg.forin;

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->value);
			analyzeStatement(flow, stmt->body);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_scopestatement)
	{
		analyzeStatement(flow, node->arg.astval);
	}
	else if(node->vtable == &ptrs_ast_vtable_body)
	{
		struct ptrs_astlist *curr = node->arg.astlist;
		while(curr != NULL)
		{
			analyzeStatement(flow, curr->entry);
			curr = curr->next;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_exprstatement)
	{
		analyzeExpression(flow, node->arg.astval);
	}
	else
	{
		ptrs_error(node, "Cannot analyze statement");
	}
}

void ptrs_flow_analyze(ptrs_ast_t *ast)
{
	ptrs_flow_t flow;
	flow.predictions = NULL;
	flow.updatePredictions = true;

	struct ptrs_astlist *curr = ast->arg.astlist;
	while(curr != NULL)
	{
		analyzeStatement(&flow, curr->entry);
		curr = curr->next;
	}

	applyAndFreePredictions(flow.predictions);
}
