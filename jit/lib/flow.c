#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../../parser/ast.h"
#include "../../parser/common.h"
#include "../include/conversion.h"
#include "../include/error.h"
#include "../ops/intrinsics.h"
#include "../jit.h"

typedef struct
{
	ptrs_val_t value;
	ptrs_meta_t meta;
	uint8_t knownValue : 1;
	uint8_t knownMeta : 1;
	uint8_t knownType : 1;
} ptrs_prediction_t;

typedef struct ptrs_flowprediction
{
	ptrs_jit_var_t *variable;
	ptrs_prediction_t prediction;
	uint8_t addressable : 1;
	unsigned depth;
	struct ptrs_flowprediction *next;
} ptrs_predictions_t;

typedef struct
{
	bool dryRun;
	bool endsInDead;
	unsigned depth;
	ptrs_predictions_t *predictions;
	//...
} ptrs_flow_t;

static void analyzeExpression(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_prediction_t *ret);
static void analyzeStatement(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_prediction_t *ret);

static inline void clearPrediction(ptrs_prediction_t *prediction)
{
	prediction->knownType = false;
	prediction->knownValue = false;
	prediction->knownMeta = false;
}

static void dupFlow(ptrs_flow_t *dest, ptrs_flow_t *src)
{
	memcpy(dest, src, sizeof(ptrs_flow_t)); // copy flags
	dest->predictions = NULL;

	ptrs_predictions_t **ptr = &dest->predictions;
	ptrs_predictions_t *curr = src->predictions;

	while(curr != NULL)
	{
		*ptr = malloc(sizeof(ptrs_predictions_t));
		memcpy(*ptr, curr, sizeof(ptrs_predictions_t));

		curr = curr->next;
		ptr = &((*ptr)->next);
	}
}

static void freePredictions(ptrs_predictions_t *curr)
{
	while(curr != NULL)
	{
		ptrs_predictions_t *next = curr->next;
		free(curr);
		curr = next;
	}
}

static void mergePredictions(ptrs_flow_t *dest, ptrs_flow_t *srcFlow)
{
	ptrs_predictions_t *src = srcFlow->predictions;

	if(dest == srcFlow || dest->predictions == src)
		return;

	if(dest->dryRun || srcFlow->dryRun)
	{
		freePredictions(src);
		return;
	}

	if(dest->endsInDead)
	{
		freePredictions(dest->predictions);
		dest->predictions = src;
		return;
	}
	if(srcFlow->endsInDead)
	{
		freePredictions(src);
		return;
	}

	while(src != NULL)
	{
		bool found = false;
		ptrs_predictions_t *curr = dest->predictions;
		while(curr != NULL)
		{
			if(curr->variable == src->variable)
			{
				int8_t currType = curr->prediction.meta.type;

				if(!curr->prediction.knownMeta || !src->prediction.knownMeta
					|| memcmp(&curr->prediction.meta, &src->prediction.meta, sizeof(ptrs_meta_t)))
				{
					curr->prediction.knownMeta = false;
					memset(&curr->prediction.meta, 0, sizeof(ptrs_meta_t));
				}

				if(!curr->prediction.knownType || !src->prediction.knownType
					|| currType != src->prediction.meta.type)
				{
					curr->prediction.knownType = false;
				}
				else
				{
					curr->prediction.meta.type = currType;
				}

				if(!curr->prediction.knownMeta || !src->prediction.knownMeta
					|| memcmp(&curr->prediction.value, &src->prediction.value, sizeof(ptrs_val_t)))
				{
					curr->prediction.knownValue = false;
					memset(&curr->prediction.value, 0, sizeof(ptrs_val_t));
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

static void clearAddressablePredictions(ptrs_flow_t *flow)
{
	if(flow->dryRun)
		return;

	ptrs_predictions_t *curr = flow->predictions;
	while(curr != NULL)
	{
		if(curr->addressable)
			clearPrediction(&curr->prediction);

		curr = curr->next;
	}
}
static void setAddressable(ptrs_flow_t *flow, ptrs_jit_var_t *var)
{
	ptrs_predictions_t *curr = flow->predictions;

	while(curr != NULL)
	{
		if(curr->variable == var)
		{
			curr->addressable = true;
			break;
		}

		curr = curr->next;
	}
}

static void setVariablePrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var, ptrs_prediction_t *prediction)
{
	ptrs_predictions_t *curr = flow->predictions;
	if(curr == NULL)
	{
		curr = malloc(sizeof(ptrs_predictions_t));
		flow->predictions = curr;
	}
	else
	{
		ptrs_predictions_t *last;
		while(curr != NULL)
		{
			if(curr->variable == var)
			{
				if(curr->depth != flow->depth)
					curr->addressable = true;

				if(flow->dryRun)
					clearPrediction(&curr->prediction);
				else
					memcpy(&curr->prediction, prediction, sizeof(ptrs_prediction_t));
				return;
			}

			last = curr;
			curr = curr->next;
		}

		last->next = malloc(sizeof(ptrs_predictions_t));
		curr = last->next;
	}

	if(flow->dryRun)
		clearPrediction(&curr->prediction);
	else
		memcpy(&curr->prediction, prediction, sizeof(ptrs_prediction_t));
	curr->variable = var;
	curr->addressable = false;
	curr->depth = flow->depth;
	curr->next = NULL;
}
static void getVariablePrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var, ptrs_prediction_t *ret)
{
	if(flow->dryRun)
	{
		clearPrediction(ret);
		return;
	}

	ptrs_predictions_t *curr = flow->predictions;
	while(curr != NULL)
	{
		if(curr->variable == var)
		{
			if(flow->depth != curr->depth)
				curr->addressable = true;

			memcpy(ret, &curr->prediction, sizeof(ptrs_prediction_t));
			return;
		}

		curr = curr->next;
	}

	clearPrediction(ret);
}

static void clearAddressablePredictionsIfOverloadExists(ptrs_flow_t *flow, ptrs_struct_t *struc, void *overload)
{
	struct ptrs_opoverload *overloadInfo = ptrs_struct_getOverloadInfo(struc, overload, true);
	if(overloadInfo != NULL)
	{
		clearAddressablePredictions(flow);
	}
}
static void clearAddressablePredictionsIfOverloadExistsOrUnavailable(ptrs_flow_t *flow, ptrs_prediction_t *prediction, void *overload)
{
	if(prediction->knownType && prediction->knownMeta
		&& prediction->meta.type == PTRS_TYPE_STRUCT)
	{
		ptrs_struct_t *struc = ptrs_meta_getPointer(prediction->meta);
		clearAddressablePredictionsIfOverloadExists(flow, struc, overload);
	}
	else
	{
		clearAddressablePredictions(flow);
	}
}

static bool prediction2bool(ptrs_prediction_t *prediction, bool *ret)
{
	if(!prediction->knownType || !prediction->knownValue)
		return false;

	if(prediction->meta.type == PTRS_TYPE_STRUCT)
		return false; // TODO handle cast<int> overload

	*ret = ptrs_vartob(prediction->value, prediction->meta);
	return true;
}

static bool prediction2int(ptrs_prediction_t *prediction, int64_t *ret)
{
	if(!prediction->knownType || !prediction->knownValue)
		return false;

	if(prediction->meta.type == PTRS_TYPE_STRUCT)
		return false; // TODO handle cast<int> overload

	*ret = ptrs_vartoi(prediction->value, prediction->meta);
	return true;
}

static bool prediction2float(ptrs_prediction_t *prediction, double *ret)
{
	if(!prediction->knownType || !prediction->knownValue)
		return false;

	if(prediction->meta.type == PTRS_TYPE_STRUCT)
		return false; // TODO handle cast<int> overload

	*ret = ptrs_vartof(prediction->value, prediction->meta);
	return true;
}

static char *prediction2str(ptrs_prediction_t *prediction, char *buff, size_t len)
{
	if(!prediction->knownType || !prediction->knownValue)
		return NULL;

	if(prediction->meta.type == PTRS_TYPE_STRUCT)
		return NULL; // TODO handle cast<string> overload

	ptrs_var_t ret = ptrs_vartoa(prediction->value, prediction->meta, buff, len);

	return ret.value.nativeval;
}

static void structMemberPrediction(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_struct_t *struc,
	const char *name, size_t namelen, ptrs_prediction_t *ret)
{
	struct ptrs_structmember *member = ptrs_struct_find(struc,
		name, namelen, PTRS_STRUCTMEMBER_SETTER, node);

	if(member == NULL)
	{
		clearAddressablePredictionsIfOverloadExists(flow, struc, ptrs_handle_member);
		clearPrediction(ret);
		return;
	}

	switch(member->type)
	{
		case PTRS_STRUCTMEMBER_GETTER:
		case PTRS_STRUCTMEMBER_SETTER:
			// TODO analyze function
			clearAddressablePredictions(flow);
			break;

		case PTRS_STRUCTMEMBER_FUNCTION:
			ret->knownType = true;
			ret->knownValue = true;
			ret->knownMeta = false;

			ret->value.nativeval = member->value.function.ast;
			ret->meta.type = PTRS_TYPE_FUNCTION;
			break;

		case PTRS_STRUCTMEMBER_TYPED:
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			memset(&ret->meta, 0, sizeof(ptrs_meta_t));
			ret->meta.type = member->value.type->varType;
			break;

		case PTRS_STRUCTMEMBER_VAR:
			clearPrediction(ret);
			break;

		case PTRS_STRUCTMEMBER_ARRAY:
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.array.size = member->value.array.size;
			ret->meta.array.readOnly = false;
			ret->meta.type = PTRS_TYPE_NATIVE;
			break;

		case PTRS_STRUCTMEMBER_VARARRAY:
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.array.size = member->value.array.size;
			ret->meta.array.readOnly = false;
			ret->meta.type = PTRS_TYPE_POINTER;
			break;
	}
}

static void analyzeList(ptrs_flow_t *flow, struct ptrs_astlist *list, ptrs_prediction_t *ret)
{
	while(list != NULL)
	{
		analyzeExpression(flow, list->entry, ret);
		list = list->next;
	}
}

static void analyzeStatementList(ptrs_flow_t *flow, struct ptrs_astlist *list, ptrs_prediction_t *ret)
{
	while(list != NULL)
	{
		analyzeStatement(flow, list->entry, ret);
		list = list->next;
	}
}

static void analyzeFunction(ptrs_flow_t *outerFlow, ptrs_function_t *ast, ptrs_struct_t *thisType)
{
	ptrs_prediction_t prediction;
	clearPrediction(&prediction);

	if(outerFlow->dryRun)
	{
		//in a dry run we analyze the function in the outer flow to make sure addressable bits
		// for values used by the inner function are set globally
		outerFlow->depth++;
		analyzeStatement(outerFlow, ast->body, &prediction);
		outerFlow->depth--;
		return;
	}

	ptrs_flow_t functionFlow;
	dupFlow(&functionFlow, outerFlow);
	functionFlow.depth++;

	clearPrediction(&prediction);

	ptrs_funcparameter_t *curr = ast->args;
	for(; curr != NULL; curr = curr->next)
	{
		setVariablePrediction(&functionFlow, &curr->arg, &prediction);
	}

	setVariablePrediction(&functionFlow, ast->vararg, &prediction);

	if(thisType != NULL)
	{
		prediction.knownMeta = true;
		ptrs_meta_setPointer(prediction.meta, thisType);
	}
	prediction.knownType = true;
	prediction.meta.type = PTRS_TYPE_STRUCT;
	setVariablePrediction(&functionFlow, &ast->thisVal, &prediction);

	analyzeStatement(&functionFlow, ast->body, &prediction);

	// instead of merging predictions we just drop the inner prediction
	freePredictions(functionFlow.predictions);
}

static void analyzeLValue(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_prediction_t *value)
{
	ptrs_prediction_t dummy;

	if(node->vtable == &ptrs_ast_vtable_identifier)
	{
		setVariablePrediction(flow, node->arg.varval, value);
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_dereference)
	{
		analyzeExpression(flow, node->arg.astval, &dummy);
		clearAddressablePredictions(flow);
	}
	else if(node->vtable == &ptrs_ast_vtable_index)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		analyzeExpression(flow, expr->left, &dummy);
		analyzeExpression(flow, expr->right, &dummy);
	}
	else if(node->vtable == &ptrs_ast_vtable_member)
	{
		struct ptrs_ast_member *expr = &node->arg.member;
		analyzeExpression(flow, expr->base, &dummy);
		clearAddressablePredictionsIfOverloadExistsOrUnavailable(flow, &dummy, ptrs_assign_member);
	}
	else if(node->vtable == &ptrs_ast_vtable_importedsymbol)
	{
		//ignore
	}
	else
	{
		analyzeExpression(flow, node, &dummy);
	}
}

static ptrs_var_t specialLogicXorIntrinsic(ptrs_ast_t *node, ptrs_val_t left, ptrs_meta_t leftMeta,
	ptrs_val_t right, ptrs_meta_t rightMeta)
{
	ptrs_var_t ret;

	bool leftB = ptrs_vartob(left, leftMeta);
	bool rightB = ptrs_vartob(right, rightMeta);

	memset(&ret.meta, 0, sizeof(ptrs_meta_t));
	ret.meta.type = PTRS_TYPE_INT;
	ret.value.intval = (leftB || rightB) && !(leftB && rightB);

	return ret;
}

#define NUM_TYPECOMPS (PTRS_NUM_TYPES << 3)
#define typecomp(a, b) ((PTRS_TYPE_##a << 3) | PTRS_TYPE_##b)
#define calc_typecomp(a, b) ((a << 3) | b)
#define typetablecomp(a, b, c) [typecomp(a, b)] = PTRS_TYPE_##c, PTRS_TYPE_UNDEFINED

static const uint8_t comparasionTypeTable[NUM_TYPECOMPS] = {
	PTRS_TYPE_INT,
};
static const uint8_t intOnlyTypeTable[NUM_TYPECOMPS] = {
	PTRS_TYPE_UNDEFINED,
	typetablecomp(INT, INT, INT),
};
static const uint8_t intFloatTypeTable[NUM_TYPECOMPS] = {
	PTRS_TYPE_UNDEFINED,
	typetablecomp(INT, INT, INT),
	typetablecomp(INT, FLOAT, FLOAT),
	typetablecomp(FLOAT, INT, FLOAT),
	typetablecomp(FLOAT, FLOAT, FLOAT),
};
static const uint8_t addTypeTable[NUM_TYPECOMPS] = {
	PTRS_TYPE_UNDEFINED,
	typetablecomp(INT, INT, INT),
	typetablecomp(INT, FLOAT, FLOAT),
	typetablecomp(FLOAT, INT, FLOAT),
	typetablecomp(FLOAT, FLOAT, FLOAT),

	typetablecomp(NATIVE, INT, NATIVE),
	typetablecomp(INT, NATIVE, NATIVE),
	typetablecomp(POINTER, INT, POINTER),
	typetablecomp(INT, POINTER, POINTER),
};
static const uint8_t subTypeTable[NUM_TYPECOMPS] = {
	PTRS_TYPE_UNDEFINED,
	typetablecomp(INT, INT, INT),
	typetablecomp(INT, FLOAT, FLOAT),
	typetablecomp(FLOAT, INT, FLOAT),
	typetablecomp(FLOAT, FLOAT, FLOAT),

	typetablecomp(NATIVE, INT, NATIVE),
	typetablecomp(INT, NATIVE, NATIVE),
	typetablecomp(POINTER, INT, POINTER),
	typetablecomp(INT, POINTER, POINTER),

	typetablecomp(NATIVE, NATIVE, INT),
	typetablecomp(POINTER, POINTER, INT),
};

struct vtableIntrinsicMapping
{
	void *vtable;
	union {
		ptrs_var_t (*intrinsic)(ptrs_ast_t *node, ptrs_val_t left, ptrs_meta_t leftMeta,
			ptrs_val_t right, ptrs_meta_t rightMeta);
		int64_t (*compare_intrinsic)(ptrs_ast_t *node, ptrs_val_t left, ptrs_meta_t leftMeta,
			ptrs_val_t right, ptrs_meta_t rightMeta);
	};
	bool isComparasion;
	const uint8_t *typeTable;
};

static const struct vtableIntrinsicMapping binaryIntrinsicHandler[] = {
	{&ptrs_ast_vtable_op_typeequal, ptrs_intrinsic_typeequal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_typeinequal, ptrs_intrinsic_typeinequal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_equal, ptrs_intrinsic_equal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_inequal, ptrs_intrinsic_inequal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_lessequal, ptrs_intrinsic_lessequal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_greaterequal, ptrs_intrinsic_greaterequal, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_less, ptrs_intrinsic_less, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_greater, ptrs_intrinsic_greater, true, comparasionTypeTable},
	{&ptrs_ast_vtable_op_or, ptrs_intrinsic_or, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_xor, ptrs_intrinsic_xor, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_and, ptrs_intrinsic_and, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_ushr, ptrs_intrinsic_ushr, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_sshr, ptrs_intrinsic_sshr, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_shl, ptrs_intrinsic_shl, false, intOnlyTypeTable},
	{&ptrs_ast_vtable_op_add, ptrs_intrinsic_add, false, addTypeTable},
	{&ptrs_ast_vtable_op_sub, ptrs_intrinsic_sub, false, subTypeTable},
	{&ptrs_ast_vtable_op_mul, ptrs_intrinsic_mul, false, intFloatTypeTable},
	{&ptrs_ast_vtable_op_div, ptrs_intrinsic_div, false, intFloatTypeTable},
	{&ptrs_ast_vtable_op_mod, ptrs_intrinsic_mod, false, intOnlyTypeTable},

	{&ptrs_ast_vtable_op_logicxor, specialLogicXorIntrinsic, false, intOnlyTypeTable},
};

struct unaryChangingHandler
{
	void *vtable;
	bool isSuffix;
	int change;
};
static const struct unaryChangingHandler unaryIntFloatChangingHandler[] = {
	{&ptrs_ast_vtable_prefix_inc, false, 1},
	{&ptrs_ast_vtable_prefix_dec, false, -1},
	{&ptrs_ast_vtable_suffix_inc, true, 1},
	{&ptrs_ast_vtable_suffix_dec, true, -1},
};

static void analyzeTypeCheckCondition(ptrs_flow_t *flow, ptrs_ast_t *typeofExpr, ptrs_ast_t *typeExpr)
{
	ptrs_prediction_t prediction;

	ptrs_ast_t *identifierAst = typeofExpr->arg.astval;
	if(identifierAst->vtable != &ptrs_ast_vtable_identifier)
		return;

	struct ptrs_ast_identifier *identifierExpr = &identifierAst->arg.identifier;

	bool oldDryRun = flow->dryRun;
	flow->dryRun = true;
	analyzeExpression(flow, typeExpr, &prediction);
	flow->dryRun = oldDryRun;

	int64_t type;
	if(prediction2int(&prediction, &type))
	{
		getVariablePrediction(flow, identifierExpr->location, &prediction);

		prediction.knownType = true;
		prediction.meta.type = type;
		setVariablePrediction(flow, identifierExpr->location, &prediction);
	}
}

static void analyzeValueCheckCondition(ptrs_flow_t *flow, ptrs_ast_t *identifier, ptrs_ast_t *value)
{
	ptrs_prediction_t prediction;
	ptrs_prediction_t oldPrediction;

	analyzeExpression(flow, value, &prediction);

	struct ptrs_ast_identifier *identifierExpr = &identifier->arg.identifier;
	getVariablePrediction(flow, identifierExpr->location, &oldPrediction);

	if(!prediction.knownValue && oldPrediction.knownValue)
	{
		prediction.knownValue = true;
		memcpy(&prediction.value, &oldPrediction.value, sizeof(ptrs_val_t));
	}
	if(!prediction.knownMeta && oldPrediction.knownMeta)
	{
		prediction.knownMeta = true;
		memcpy(&prediction.meta, &oldPrediction.meta, sizeof(ptrs_meta_t));
	}
	if(!prediction.knownType && oldPrediction.knownType)
	{
		prediction.knownType = true;
		prediction.meta.type = oldPrediction.meta.type;
	}

	setVariablePrediction(flow, identifierExpr->location, &prediction);
}

static void analyzeCondition(ptrs_flow_t *flow, ptrs_ast_t *node, bool isElse)
{
	if(node->vtable == &ptrs_ast_vtable_prefix_logicnot)
	{
		analyzeCondition(flow, node->arg.astval, !isElse);
	}
	else if((node->vtable == &ptrs_ast_vtable_op_equal && !isElse)
		|| (node->vtable == &ptrs_ast_vtable_op_typeequal && !isElse)
		|| (node->vtable == &ptrs_ast_vtable_op_inequal && isElse)
		|| (node->vtable == &ptrs_ast_vtable_op_typeinequal && isElse))
	{
		bool isTypeSafeComparasion = node->vtable == &ptrs_ast_vtable_op_typeequal
			|| node->vtable == &ptrs_ast_vtable_op_typeinequal;

		struct ptrs_ast_binary *expr = &node->arg.binary;
		if(expr->left->vtable == &ptrs_ast_vtable_prefix_typeof)
		{
			analyzeTypeCheckCondition(flow, expr->left, expr->right);
		}
		else if(expr->right->vtable == &ptrs_ast_vtable_prefix_typeof)
		{
			analyzeTypeCheckCondition(flow, expr->right, expr->left);
		}
		else if(isTypeSafeComparasion && expr->left->vtable == &ptrs_ast_vtable_identifier)
		{
			analyzeValueCheckCondition(flow, expr->left, expr->right);
		}
		else if(isTypeSafeComparasion && expr->right->vtable == &ptrs_ast_vtable_identifier)
		{
			analyzeValueCheckCondition(flow, expr->right, expr->left);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_op_logicand)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		analyzeCondition(flow, expr->left, isElse);
		analyzeCondition(flow, expr->right, isElse);
	}
}

static void analyzeExpression(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_prediction_t *ret)
{
	ptrs_prediction_t dummy;
	clearPrediction(ret);

	if(node == NULL)
		return;

	if(node->vtable == &ptrs_ast_vtable_constant)
	{
		ret->knownType = true;
		ret->knownValue = true;
		ret->knownMeta = true;
		ret->value = node->arg.constval.value;
		ret->meta = node->arg.constval.meta;
	}
	else if(node->vtable == &ptrs_ast_vtable_identifier)
	{
		struct ptrs_ast_identifier *expr = &node->arg.identifier;

		getVariablePrediction(flow, expr->location, ret);

		if(!flow->dryRun)
		{
			if(ret->knownValue)
			{
				expr->valuePredicted = true;
				memcpy(&expr->valuePrediction, &ret->value, sizeof(ptrs_val_t));
			}
			else
			{
				expr->valuePredicted = false;
			}

			if(ret->knownMeta)
			{
				expr->metaPredicted = true;
				memcpy(&expr->metaPrediction, &ret->meta, sizeof(ptrs_meta_t));
			}
			else
			{
				expr->metaPredicted = false;
			}

			if(ret->knownType)
			{
				expr->typePredicted = true;
				expr->metaPrediction.type = ret->meta.type;
			}
			else
			{
				expr->typePredicted = false;
			}
		}
		else
		{
			expr->valuePredicted = false;
			expr->metaPredicted = false;
			expr->typePredicted = false;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_functionidentifier)
	{
		ret->knownValue = true;
		ret->knownType = true;
		ret->value.nativeval = &node->arg.funcval->func;
		ret->meta.type = PTRS_TYPE_FUNCTION;
	}
	else if(node->vtable == &ptrs_ast_vtable_array)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;
		bool knownSize = false;
		int64_t size;

		if(stmt->isInitExpr)
		{
			analyzeExpression(flow, stmt->initExpr, ret);

			if(ret->knownType && ret->knownMeta
				&& ret->meta.type == PTRS_TYPE_NATIVE)
			{
				knownSize = true;
				size = ret->meta.array.size;
			}
		}

		if(stmt->value != NULL) //size
		{
			analyzeExpression(flow, stmt->value, &dummy);
			knownSize = prediction2int(&dummy, &size);
		}

		if(!stmt->isInitExpr) // TODO derive array size
			analyzeList(flow, stmt->initVal, &dummy);

		if(knownSize)
		{
			ret->knownMeta = true;
			ret->meta.array.size = size;
			ret->meta.array.readOnly = false;
		}

		ret->knownType = true;
		ret->meta.type = PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_vararray)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;
		ret->meta.array.readOnly = false;

		if(stmt->value != NULL) //size
		{
			analyzeExpression(flow, stmt->value, &dummy);

			int64_t size;
			if(prediction2int(&dummy, &size))
			{
				ret->knownMeta = true;
				ret->meta.array.size = size;
				ret->meta.array.readOnly = false;
			}
		}

		// TODO derive array size
		analyzeList(flow, stmt->initVal, &dummy);

		ret->knownType = 1;
		ret->meta.type = PTRS_TYPE_POINTER;
	}
	else if(node->vtable == &ptrs_ast_vtable_function)
	{
		analyzeFunction(flow, &node->arg.function.func, NULL);

		ret->knownType = true;
		ret->meta.type = PTRS_TYPE_FUNCTION;
	}
	else if(node->vtable == &ptrs_ast_vtable_call)
	{
		struct ptrs_ast_call *expr = &node->arg.call;
		analyzeExpression(flow, expr->value, ret);

		analyzeList(flow, expr->arguments, &dummy);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_NATIVE)
		{
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			memset(&ret->meta, 0, sizeof(ptrs_meta_t));

			if(expr->retType != NULL)
				ret->meta.type = expr->retType->varType;
			else
				ret->meta.type = PTRS_TYPE_INT;
		}
		else if(ret->knownType && ret->knownValue
			&& ret->meta.type == PTRS_TYPE_FUNCTION)
		{
			// TODO only reset predictions of varibles used in the function
			clearAddressablePredictions(flow);
			clearPrediction(ret);
		}
		else
		{
			clearPrediction(ret);
		}

		clearAddressablePredictions(flow);
	}
	else if(node->vtable == &ptrs_ast_vtable_stringformat)
	{
		struct ptrs_ast_strformat *expr = &node->arg.strformat;

		struct ptrs_stringformat *curr = expr->insertions;
		while(curr != NULL)
		{
			analyzeExpression(flow, curr->entry, ret);
			curr = curr->next;
		}

		ret->knownType = true;
		ret->knownMeta = false;
		ret->knownValue = false;
		ret->meta.type = PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_new)
	{
		struct ptrs_ast_new *expr = &node->arg.newexpr;

		analyzeExpression(flow, expr->value, ret);
		analyzeList(flow, expr->arguments, &dummy);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_STRUCT)
		{
			ret->knownValue = false;
			// keep meta and type from the constructor

			if(ret->knownMeta)
			{
				ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);
				clearAddressablePredictionsIfOverloadExists(flow, struc, ptrs_handle_new);
			}
			else
			{
				clearAddressablePredictions(flow);
			}
		}
		else
		{
			clearPrediction(ret);
			clearAddressablePredictions(flow);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_member)
	{
		struct ptrs_ast_member *expr = &node->arg.member;
		analyzeExpression(flow, expr->base, ret);

		if(ret->knownType && ret->knownMeta
			&& ret->meta.type == PTRS_TYPE_STRUCT)
		{
			ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);

			structMemberPrediction(flow, node, struc, expr->name, expr->namelen, ret);
		}
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_sizeof)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		ret->knownValue = false;
		if(ret->knownType && ret->knownMeta)
		{
			if(ret->meta.type == PTRS_TYPE_NATIVE || ret->meta.type == PTRS_TYPE_POINTER)
			{
				ret->knownValue = true;
				ret->value.intval = ret->meta.array.size;
			}
			else if(ret->meta.type == PTRS_TYPE_STRUCT)
			{
				ret->knownValue = true;
				ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);
				ret->value.intval = struc->size;
			}
		}

		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_address)
	{
		ptrs_ast_t *target = node->arg.astval;

		//TODO do we need analyzeAddrOfExpression?
		analyzeExpression(flow, target, ret);

		if(target->vtable == &ptrs_ast_vtable_identifier)
		{
			struct ptrs_ast_identifier *expr = &target->arg.identifier;
			setAddressable(flow, expr->location);

			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.type = PTRS_TYPE_POINTER;
			ret->meta.array.readOnly = false;
			ret->meta.array.size = 1;
		}
		else if(target->vtable == &ptrs_ast_vtable_member)
		{
			struct ptrs_ast_member *expr = &target->arg.member;
			analyzeExpression(flow, expr->base, ret);

			if(ret->knownType && ret->knownMeta && ret->meta.type == PTRS_TYPE_STRUCT)
			{
				ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);

				struct ptrs_structmember *member = ptrs_struct_find(struc,
					expr->name, expr->namelen, PTRS_STRUCTMEMBER_SETTER, node);

				if(member != NULL && member->type == PTRS_STRUCTMEMBER_VAR)
				{
					ret->knownType = true;
					ret->knownMeta = true;
					ret->knownValue = false;
					ret->meta.type = PTRS_TYPE_POINTER;
					ret->meta.array.readOnly = false;
					ret->meta.array.size = 1;
				}
				else if(member != NULL && member->type == PTRS_STRUCTMEMBER_TYPED)
				{
					ret->knownType = true;
					ret->knownMeta = true;
					ret->knownValue = false;
					ret->meta.type = PTRS_TYPE_NATIVE;
					ret->meta.array.readOnly = false;
					ret->meta.array.size = member->value.type->size;
				}
				else
				{
					clearAddressablePredictionsIfOverloadExists(flow, struc, ptrs_handle_member);
					clearPrediction(ret);
				}
			}
			else
			{
				// we need to also clear addressable predictions as an overload might be called
				clearAddressablePredictions(flow);
				clearPrediction(ret);
			}
		}
		else if(target->vtable == &ptrs_ast_vtable_index)
		{
			struct ptrs_ast_binary *expr = &node->arg.binary;

			analyzeExpression(flow, expr->left, ret);
			analyzeExpression(flow, expr->right, &dummy);

			if(ret->knownType && ret->meta.type == PTRS_TYPE_NATIVE)
			{
				ret->knownType = true;
				ret->knownMeta = false;
				ret->knownValue = false;
				ret->meta.type = PTRS_TYPE_NATIVE;
				// TODO predict array size when index and size are known
			}
			else if(ret->knownType && ret->meta.type == PTRS_TYPE_POINTER)
			{
				ret->knownType = true;
				ret->knownMeta = false;
				ret->knownValue = false;
				ret->meta.type = PTRS_TYPE_NATIVE;
				// TODO predict array size when index and size are known
			}
			else
			{
				clearAddressablePredictionsIfOverloadExistsOrUnavailable(flow, ret, ptrs_handle_prefix_address);
				clearPrediction(ret);
			}
		}
		//TODO &ptrs_ast_vtable_importedsymbol
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_dereference)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_NATIVE)
		{
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.type = PTRS_TYPE_INT;
			ret->meta.array.readOnly = false;
			ret->meta.array.size = 1;
		}
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_indexlength)
	{
		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_index)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		analyzeExpression(flow, expr->left, ret);
		analyzeExpression(flow, expr->right, &dummy);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_NATIVE)
		{
			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.type = PTRS_TYPE_INT;
			ret->meta.array.readOnly = false;
			ret->meta.array.size = 1;
		}
		else if(ret->knownType && ret->knownMeta && ret->meta.type == PTRS_TYPE_STRUCT)
		{
			char buff[32];
			char *name = prediction2str(&dummy, buff, 32);

			ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);
			structMemberPrediction(flow, node, struc, name, strlen(name) + 1, ret);
		}
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_slice)
	{
		struct ptrs_ast_slice *expr = &node->arg.slice;

		analyzeExpression(flow, expr->base, ret);
		bool isArray = ret->knownType
			&& (ret->meta.type == PTRS_TYPE_NATIVE || ret->meta.type == PTRS_TYPE_POINTER);

		if(!isArray)
			clearPrediction(ret);

		analyzeExpression(flow, expr->start, &dummy);
		int64_t start;
		if(isArray && ret->knownMeta && dummy.knownType && dummy.knownValue)
		{
			if(!prediction2int(&dummy, &start))
				ret->knownMeta = false;
		}

		analyzeExpression(flow, expr->end, &dummy);
		int64_t end;
		if(isArray && ret->knownMeta && dummy.knownType && dummy.knownValue)
		{
			if(prediction2int(&dummy, &end))
				ret->meta.array.size = end - start;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_as)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;

		analyzeExpression(flow, expr->value, ret);

		ret->knownType = true;
		ret->knownMeta = true;
		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->meta.type = expr->builtinType;
	}
	else if(node->vtable == &ptrs_ast_vtable_cast_builtin)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;

		analyzeExpression(flow, expr->value, ret);

		if(!ret->knownType || ret->meta.type == PTRS_TYPE_STRUCT)
			clearAddressablePredictions(flow); // TODO cast<int> overload

		if(expr->builtinType == PTRS_TYPE_INT)
		{
			int64_t value;
			if(prediction2int(ret, &value))
			{
				ret->knownValue = true;
				ret->value.intval = value;
			}
			else
			{
				ret->knownValue = false;
			}
		}
		else if(expr->builtinType == PTRS_TYPE_FLOAT)
		{
			double value;
			if(prediction2float(ret, &value))
			{
				ret->knownValue = true;
				ret->value.floatval = value;
			}
			else
			{
				ret->knownValue = false;
			}
		}
		else
		{
			ret->knownValue = false;
		}

		ret->knownType = true;
		ret->knownMeta = true;
		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->meta.type = expr->builtinType;
	}
	else if(node->vtable == &ptrs_ast_vtable_tostring)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;
		analyzeExpression(flow, expr->value, ret);

		char buff[32];
		char *retStr = prediction2str(ret, buff, 32);

		if(retStr == buff)
			retStr = strdup(buff); //TODO this never gets free'd

		if(retStr == NULL)
		{
			ret->knownValue = false;
		}
		else
		{
			ret->knownValue = true;
			ret->value.nativeval = retStr;
		}

		ret->knownType = true;
		ret->knownMeta = false;
		ret->meta.type = PTRS_TYPE_NATIVE;
	}
	else if(node->vtable == &ptrs_ast_vtable_cast)
	{
		struct ptrs_ast_cast *expr = &node->arg.cast;
		analyzeExpression(flow, expr->value, &dummy);
		analyzeExpression(flow, expr->type, ret);

		if(!ret->knownType || ret->meta.type != PTRS_TYPE_STRUCT)
		{
			clearPrediction(ret);
		}
		else if(dummy.knownValue)
		{
			ret->knownValue = true;
			ret->value = dummy.value;
		}
		else
		{
			ret->knownValue = false;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_importedsymbol)
	{
		struct ptrs_ast_importedsymbol *expr = &node->arg.importedsymbol;

		if(expr->type != NULL)
		{
			ret->knownType = true;
			ret->meta.type = expr->type->varType;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_typeof)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		if(ret->knownType)
		{
			ret->knownValue = true;
			ret->value.intval = ret->meta.type;
		}

		ret->knownType = true;
		ret->knownMeta = true;
		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_yield)
	{
		struct ptrs_ast_yield *expr = &node->arg.yield;
		analyzeList(flow, expr->values, ret);

		ret->knownType = true;
		ret->knownValue = false;
		ret->knownMeta = true;
		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_assign)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;
		analyzeExpression(flow, expr->right, ret);
		analyzeLValue(flow, expr->left, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_op_ternary)
	{
		struct ptrs_ast_ternary *expr = &node->arg.ternary;
		analyzeExpression(flow, expr->condition, ret);

		bool result;
		if(prediction2bool(ret, &result))
		{
			if(result)
				analyzeExpression(flow, expr->trueVal, ret);
			else
				analyzeExpression(flow, expr->trueVal, &dummy);

			if(result)
				analyzeExpression(flow, expr->falseVal, &dummy);
			else
				analyzeExpression(flow, expr->falseVal, ret);
		}
		else
		{
			analyzeExpression(flow, expr->trueVal, &dummy);
			analyzeExpression(flow, expr->falseVal, &dummy);
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_logicnot)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		bool result;
		if(prediction2bool(ret, &result))
		{
			ret->knownValue = true;
			ret->value.intval = !result;
		}
		else
		{
			ret->knownValue = false;
		}

		ret->knownType = true;
		ret->knownMeta = true;
		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_in)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		analyzeExpression(flow, expr->left, &dummy);
		analyzeExpression(flow, expr->right, ret);

		char buff[32];
		char *key = prediction2str(&dummy, buff, 32);

		if(ret->knownType && ret->knownMeta && key != NULL
			&& ret->meta.type == PTRS_TYPE_STRUCT)
		{
			ptrs_struct_t *struc = ptrs_meta_getPointer(ret->meta);

			ret->knownValue = true;
			ret->value.intval = ptrs_struct_find(struc, key, strlen(key) + 1, -1, node) != NULL;
		}
		else
		{
			ret->knownValue = false;
		}

		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_instanceof)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		analyzeExpression(flow, expr->left, &dummy);
		analyzeExpression(flow, expr->right, ret);

		if(ret->knownType && ret->meta.type != PTRS_TYPE_STRUCT)
		{
			// cause an error?
			ret->knownValue = false;
		}
		else if(dummy.knownMeta && ret->knownMeta)
		{
			ret->knownValue = true;
			ret->value.intval = memcmp(&ret->meta, &dummy.meta, sizeof(ptrs_meta_t)) == 0;
		}

		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_logicor)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		analyzeExpression(flow, expr->left, &dummy);

		bool value;
		if(prediction2bool(&dummy, &value) && value)
		{
			ret->knownValue = true;
			ret->value.intval = true;
		}
		else
		{
			analyzeExpression(flow, expr->right, &dummy);

			bool value;
			if(prediction2bool(&dummy, &value))
			{
				ret->knownValue = true;
				ret->value.intval = value;
			}
			else
			{
				ret->knownValue = false;
			}
		}

		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_op_logicand)
	{
		struct ptrs_ast_binary *expr = &node->arg.binary;

		analyzeExpression(flow, expr->left, &dummy);

		bool value;
		if(prediction2bool(&dummy, &value) && !value)
		{
			ret->knownValue = true;
			ret->value.intval = false;
		}
		else
		{
			analyzeExpression(flow, expr->right, &dummy);

			bool value;
			if(prediction2bool(&dummy, &value))
			{
				ret->knownValue = true;
				ret->value.intval = value;
			}
			else
			{
				ret->knownValue = false;
			}
		}

		memset(&ret->meta, 0, sizeof(ptrs_meta_t));
		ret->knownType = true;
		ret->knownMeta = true;
		ret->meta.type = PTRS_TYPE_INT;
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_not)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_INT)
		{
			if(ret->knownValue)
				ret->value.intval = ~ret->value.intval;
		}
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_minus)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		if(ret->knownType && ret->meta.type == PTRS_TYPE_INT)
		{
			if(ret->knownValue)
				ret->value.intval = -ret->value.intval;
		}
		else if(ret->knownType && ret->meta.type == PTRS_TYPE_FLOAT)
		{
			if(ret->knownValue)
				ret->value.floatval = -ret->value.floatval;
		}
		else
		{
			clearPrediction(ret);
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_prefix_plus)
	{
		analyzeExpression(flow, node->arg.astval, ret);
	}
	else
	{
		for(int i = 0; i < sizeof(binaryIntrinsicHandler) / sizeof(struct vtableIntrinsicMapping); i++)
		{
			if(node->vtable == binaryIntrinsicHandler[i].vtable)
			{
				struct ptrs_ast_binary *expr = &node->arg.binary;
				analyzeExpression(flow, expr->left, ret);
				analyzeExpression(flow, expr->right, &dummy);

				if(ret->knownType && ret->knownMeta && ret->knownValue
					&& dummy.knownType && dummy.knownMeta && dummy.knownValue)
				{
					ptrs_var_t result;

					if(binaryIntrinsicHandler[i].isComparasion)
					{
						result.value.intval = binaryIntrinsicHandler[i].compare_intrinsic(
							node, ret->value, ret->meta, dummy.value, dummy.meta
						);

						memset(&result.meta, 0, sizeof(ptrs_meta_t));
						result.meta.type = PTRS_TYPE_INT;
					}
					else
					{
						result = binaryIntrinsicHandler[i].intrinsic(
							node, ret->value, ret->meta, dummy.value, dummy.meta
						);
					}

					memcpy(&ret->value, &result.value, sizeof(ptrs_val_t));
					memcpy(&ret->meta, &result.meta, sizeof(ptrs_meta_t));
				}
				else if(binaryIntrinsicHandler[i].isComparasion)
				{
					clearPrediction(ret);
					ret->knownType = true;
					ret->meta.type = PTRS_TYPE_INT;
				}
				else if(ret->knownType && dummy.knownType)
				{
					const uint8_t *typeTable = binaryIntrinsicHandler[i].typeTable;
					size_t comp = calc_typecomp(ret->meta.type, dummy.meta.type);
					clearPrediction(ret);

					if(typeTable[comp] != PTRS_TYPE_UNDEFINED)
					{
						ret->knownType = true;
						ret->meta.type = typeTable[comp];
					}
				}
				else
				{
					clearPrediction(ret);
				}

				return;
			}
		}

		for(int i = 0; i < sizeof(unaryIntFloatChangingHandler) / sizeof(struct unaryChangingHandler); i++)
		{
			if(node->vtable == unaryIntFloatChangingHandler[i].vtable)
			{
				analyzeExpression(flow, node->arg.astval, ret);

				if(ret->knownType && ret->knownMeta && ret->knownValue)
				{
					if(ret->meta.type == PTRS_TYPE_INT)
					{
						ret->value.intval += unaryIntFloatChangingHandler[i].change;
						analyzeLValue(flow, node->arg.astval, ret);

						if(!unaryIntFloatChangingHandler[i].isSuffix)
							ret->value.intval -= unaryIntFloatChangingHandler[i].change;

						return;
					}
					else if(ret->meta.type == PTRS_TYPE_FLOAT)
					{
						ret->value.floatval += unaryIntFloatChangingHandler[i].change;
						analyzeLValue(flow, node->arg.astval, ret);

						if(!unaryIntFloatChangingHandler[i].isSuffix)
							ret->value.floatval -= unaryIntFloatChangingHandler[i].change;

						return;
					}
				}

				if(ret->knownType
					&& (ret->meta.type == PTRS_TYPE_INT || ret->meta.type == PTRS_TYPE_FLOAT))
				{
					ret->knownValue = false;
					return;
				}
				else
				{
					clearPrediction(ret);
				}

				analyzeLValue(flow, node->arg.astval, ret);
				return;
			}
		}

		ptrs_error(node, "Cannot analyze expression");
	}
}

#define loopPredictionsRemerge(body) \
	if(flow->dryRun) \
	{ \
		body \
	} \
	else \
	{ \
		ptrs_flow_t other; \
		\
		dupFlow(&other, flow); \
		body \
		mergePredictions(flow, &other); \
		\
		dupFlow(&other, flow); \
		body \
		mergePredictions(flow, &other); \
		\
		dupFlow(&other, flow); \
		body \
		mergePredictions(flow, &other); \
		\
		/* TODO replace this by a fix point algorithm? */ \
	}

static void analyzeStatement(ptrs_flow_t *flow, ptrs_ast_t *node, ptrs_prediction_t *ret)
{
	ptrs_prediction_t dummy;

	clearPrediction(ret);
	if(node == NULL)
		return;


	if(node->vtable == &ptrs_ast_vtable_define)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;
		analyzeExpression(flow, stmt->value, ret);

		setVariablePrediction(flow, &stmt->location, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_array)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		ret->knownType = true;
		ret->knownValue = false;
		ret->knownMeta = false;
		ret->meta.type = PTRS_TYPE_NATIVE;

		if(stmt->isInitExpr)
			analyzeExpression(flow, stmt->initExpr, &dummy);

		if(stmt->value != NULL)
		{
			analyzeExpression(flow, stmt->value, &dummy);

			int64_t size;
			if(prediction2int(&dummy, &size))
			{
				ret->knownMeta = true;
				ret->meta.array.readOnly = false;
				ret->meta.array.size = size;
			}
		}

		if(!stmt->isInitExpr)
			analyzeList(flow, stmt->initVal, &dummy);

		setVariablePrediction(flow, &stmt->location, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_vararray)
	{
		struct ptrs_ast_define *stmt = &node->arg.define;

		ret->knownType = true;
		ret->knownValue = false;
		ret->knownMeta = false;
		ret->meta.type = PTRS_TYPE_POINTER;

		if(stmt->value != NULL)
		{
			analyzeExpression(flow, stmt->value, &dummy);

			int64_t size;
			if(prediction2int(&dummy, &size))
			{
				ret->knownMeta = true;
				ret->meta.array.readOnly = false;
				ret->meta.array.size = size;
			}
		}

		analyzeList(flow, stmt->initVal, &dummy);
		setVariablePrediction(flow, &stmt->location, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_import)
	{
		//ignore
	}
	else if(node->vtable == &ptrs_ast_vtable_return
		|| node->vtable == &ptrs_ast_vtable_throw)
	{
		analyzeExpression(flow, node->arg.astval, ret);

		//TODO additionally flag for throwing which is unset by try/catch?
		// try/catch isn't implemented at the moment anyways though
		flow->endsInDead = true;
	}
	else if(node->vtable == &ptrs_ast_vtable_delete)
	{
		analyzeExpression(flow, node->arg.astval, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_continue
		|| node->vtable == &ptrs_ast_vtable_break)
	{
		//nothing
	}
	else if(node->vtable == &ptrs_ast_vtable_trycatch)
	{
		struct ptrs_ast_trycatch *stmt = &node->arg.trycatch;

		analyzeStatement(flow, stmt->tryBody, &dummy);

		//TODO catchBody

		analyzeStatement(flow, stmt->finallyBody, &dummy);
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
				analyzeFunction(flow, curr->value.function.ast, struc);
			}
		}

		struct ptrs_opoverload *curr = struc->overloads;
		while(curr != NULL)
		{
			analyzeFunction(flow, curr->handler, struc);
			curr = curr->next;
		}

		ret->knownType = true;
		ret->knownValue = true;
		ret->knownMeta = true;
		ret->value.structval = NULL;
		ret->meta.type = PTRS_TYPE_STRUCT;
		ptrs_meta_setPointer(ret->meta, struc);
		setVariablePrediction(flow, struc->location, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_function)
	{
		analyzeFunction(flow, &node->arg.function.func, NULL);
	}
	else if(node->vtable == &ptrs_ast_vtable_if)
	{
		struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;
		analyzeExpression(flow, stmt->condition, &dummy);

		bool condition;
		bool conditionPredicted = prediction2bool(&dummy, &condition);

		ptrs_flow_t elseFlowVal;
		ptrs_flow_t *elseFlow = flow;

		if(!conditionPredicted && !flow->dryRun)
		{
			dupFlow(&elseFlowVal, flow);
			elseFlow = &elseFlowVal;
		}

		if(!conditionPredicted || condition)
		{
			analyzeCondition(flow, stmt->condition, false);

			analyzeStatement(flow, stmt->ifBody, ret);
		}

		if(!conditionPredicted || !condition)
		{
			analyzeCondition(elseFlow, stmt->condition, true);

			if(stmt->elseBody != NULL)
				analyzeStatement(elseFlow, stmt->elseBody, ret);
		}

		if(!conditionPredicted && !flow->dryRun)
			mergePredictions(flow, &elseFlowVal);
	}
	else if(node->vtable == &ptrs_ast_vtable_switch)
	{
		struct ptrs_ast_switch *stmt = &node->arg.switchcase;
		struct ptrs_ast_case *curr = stmt->cases;

		int64_t value;
		if(prediction2int(&dummy, &value))
		{
			ptrs_predictions_t *orginalPredictions = NULL;
			bool orginalDryRun = flow->dryRun;
			bool foundCase = false;

			while(curr)
			{
				if(curr->min >= value && value <= curr->max)
				{
					flow->dryRun = orginalDryRun;
					foundCase = true;
				}
				else
				{
					flow->dryRun = true;
				}

				analyzeStatement(flow, curr->body, &dummy);

				curr = curr->next;
			}

			flow->dryRun = foundCase || orginalDryRun;
			analyzeStatement(flow, stmt->defaultCase, &dummy);

			flow->dryRun = orginalDryRun;
		}
		else
		{
			analyzeStatement(flow, stmt->defaultCase, &dummy);

			while(curr)
			{
				ptrs_flow_t otherFlowVal;
				ptrs_flow_t *otherFlow = flow;
				if(!flow->dryRun)
				{
					dupFlow(&otherFlowVal, flow);
					otherFlow = &otherFlowVal;
				}

				analyzeStatement(flow, curr->body, ret);
				mergePredictions(flow, otherFlow);

				curr = curr->next;
			}
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_while)
	{
		struct ptrs_ast_control *stmt = &node->arg.control;
		bool condition;

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->condition, &dummy);
			if(!flow->dryRun && prediction2bool(&dummy, &condition) && !condition)
				return;

			analyzeStatement(flow, stmt->body, &dummy);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_dowhile)
	{
		struct ptrs_ast_control *stmt = &node->arg.control;
		bool condition;

		loopPredictionsRemerge(
			analyzeStatement(flow, stmt->body, &dummy);

			analyzeExpression(flow, stmt->condition, &dummy);
			if(!flow->dryRun && prediction2bool(&dummy, &condition) && !condition)
				return;
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_for)
	{
		struct ptrs_ast_for *stmt = &node->arg.forstatement;
		bool condition;

		analyzeStatement(flow, stmt->init, &dummy);

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->condition, &dummy);
			if(!flow->dryRun && prediction2bool(&dummy, &condition) && !condition)
				return;

			analyzeStatement(flow, stmt->body, &dummy);
			analyzeExpression(flow, stmt->step, &dummy);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_forin)
	{
		struct ptrs_ast_forin *stmt = &node->arg.forin;
		analyzeExpression(flow, stmt->value, &dummy);

		loopPredictionsRemerge(
			analyzeStatement(flow, stmt->body, &dummy);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_scopestatement)
	{
		analyzeStatement(flow, node->arg.astval, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_body)
	{
		struct ptrs_astlist *curr = node->arg.astlist;
		while(curr != NULL)
		{
			analyzeStatement(flow, curr->entry, ret);
			curr = curr->next;
		}
	}
	else if(node->vtable == &ptrs_ast_vtable_exprstatement)
	{
		analyzeExpression(flow, node->arg.astval, ret);
	}
	else
	{
		ptrs_error(node, "Cannot analyze statement");
	}
}

void ptrs_flow_analyze(ptrs_ast_t *ast)
{
	ptrs_prediction_t ret;

	ptrs_flow_t flow;
	flow.predictions = NULL;
	flow.depth = 0;
	flow.endsInDead = false;

	// make a dry run first to set addressable for variables used accross functions
	flow.dryRun = true;
	analyzeStatementList(&flow, ast->arg.astlist, &ret);

	flow.dryRun = false;
	analyzeStatementList(&flow, ast->arg.astlist, &ret);

	freePredictions(flow.predictions);
}
