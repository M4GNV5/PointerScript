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
	uint8_t isFunctionParameter : 1;
	unsigned depth;
	union
	{
		struct ptrs_ast_define *variable;
		ptrs_funcparameter_t *parameter;
	} definition;
	struct ptrs_flowprediction *next;
} ptrs_predictions_t;

typedef struct
{
	bool dryRun;
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

static void freePredictions(ptrs_predictions_t *curr)
{
	while(curr != NULL)
	{
		ptrs_predictions_t *next = curr->next;
		free(curr);
		curr = next;
	}
}

static void mergePredictions(ptrs_flow_t *dest, ptrs_predictions_t *src)
{
	if(dest->predictions == src)
		return;

	if(dest->dryRun)
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
	}
}

static void createVariablePrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var,
	ptrs_prediction_t *prediction, bool isParameter, void *definition)
{
	if(flow->dryRun)
		return;

	ptrs_predictions_t *curr = malloc(sizeof(ptrs_predictions_t));

	memcpy(&curr->prediction, prediction, sizeof(ptrs_prediction_t));
	curr->variable = var;
	curr->addressable = flow->depth == 0;
	curr->depth = flow->depth;

	curr->isFunctionParameter = isParameter;
	if(isParameter)
		curr->definition.parameter = definition;
	else
		curr->definition.variable = definition;

	curr->next = flow->predictions;
	flow->predictions = curr;
}
static void setVariablePrediction(ptrs_flow_t *flow, ptrs_jit_var_t *var, ptrs_prediction_t *prediction)
{
	ptrs_predictions_t *curr = flow->predictions;
	if(curr == NULL)
	{
		if(flow->dryRun)
			return;

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

				if(!flow->dryRun)
					memcpy(&curr->prediction, prediction, sizeof(ptrs_prediction_t));
				return;
			}

			curr = curr->next;
			last = curr;
		}

		if(flow->dryRun)
			return;

		last->next = malloc(sizeof(ptrs_predictions_t));
		curr = last->next;
	}

	memcpy(&curr->prediction, prediction, sizeof(ptrs_prediction_t));
	curr->variable = var;
	curr->addressable = false;
	curr->isFunctionParameter = false;
	curr->depth = flow->depth;
	curr->definition.variable = NULL;
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

static bool structMemberPrediction(ptrs_flow_t *flow,
	struct ptrs_structmember *member, ptrs_prediction_t *ret)
{
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

static void analyzeFunction(ptrs_flow_t *flow, ptrs_function_t *ast)
{
	flow->depth++;

	ptrs_prediction_t prediction;
	clearPrediction(&prediction);

	ptrs_funcparameter_t *curr = ast->args;
	for(; curr != NULL; curr = curr->next)
	{
		createVariablePrediction(flow, &curr->arg, &prediction, true, curr);
	}

	createVariablePrediction(flow, &ast->thisVal, &prediction, true, NULL);
	createVariablePrediction(flow, ast->vararg, &prediction, true, NULL);

	analyzeStatement(flow, ast->body, &prediction);

	flow->depth--;
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

struct vtableIntrinsicMapping
{
	void *vtable;
	ptrs_var_t (*intrinsic)(ptrs_ast_t *node, ptrs_val_t left, ptrs_meta_t leftMeta,
		ptrs_val_t right, ptrs_meta_t rightMeta);
};

static const struct vtableIntrinsicMapping binaryIntrinsicHandler[] = {
	{&ptrs_ast_vtable_op_typeequal, ptrs_intrinsic_typeequal},
	{&ptrs_ast_vtable_op_typeinequal, ptrs_intrinsic_typeinequal},
	{&ptrs_ast_vtable_op_equal, ptrs_intrinsic_equal},
	{&ptrs_ast_vtable_op_inequal, ptrs_intrinsic_inequal},
	{&ptrs_ast_vtable_op_lessequal, ptrs_intrinsic_lessequal},
	{&ptrs_ast_vtable_op_greaterequal, ptrs_intrinsic_greaterequal},
	{&ptrs_ast_vtable_op_less, ptrs_intrinsic_less},
	{&ptrs_ast_vtable_op_greater, ptrs_intrinsic_greater},
	{&ptrs_ast_vtable_op_or, ptrs_intrinsic_or},
	{&ptrs_ast_vtable_op_xor, ptrs_intrinsic_xor},
	{&ptrs_ast_vtable_op_and, ptrs_intrinsic_and},
	{&ptrs_ast_vtable_op_ushr, ptrs_intrinsic_ushr},
	{&ptrs_ast_vtable_op_sshr, ptrs_intrinsic_sshr},
	{&ptrs_ast_vtable_op_shl, ptrs_intrinsic_shl},
	{&ptrs_ast_vtable_op_add, ptrs_intrinsic_add},
	{&ptrs_ast_vtable_op_sub, ptrs_intrinsic_sub},
	{&ptrs_ast_vtable_op_mul, ptrs_intrinsic_mul},
	{&ptrs_ast_vtable_op_div, ptrs_intrinsic_div},
	{&ptrs_ast_vtable_op_mod, ptrs_intrinsic_mod},
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

			if(ret->knownMeta)
			{
				expr->metaPredicted = true;
				memcpy(&expr->metaPrediction, &ret->meta, sizeof(ptrs_meta_t));
			}

			if(ret->knownType)
			{
				expr->typePredicted = true;
				expr->metaPrediction.type = ret->meta.type;
			}
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
			analyzeExpression(flow, stmt->value, ret);
			knownSize = prediction2int(ret, &size);
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
			analyzeExpression(flow, stmt->value, ret);

			int64_t size;
			if(prediction2int(ret, &size))
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
		analyzeFunction(flow, &node->arg.function.func);

		ret->knownValue = true;
		ret->knownType = true;
		ret->value.nativeval = &node->arg.function.func;
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
			clearAddressablePredictions(flow);
			// TODO only reset predictions of varibles used in the function
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
		}
		else
		{
			clearPrediction(ret);
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
			struct ptrs_structmember *member = ptrs_struct_find(struc,
				expr->name, expr->namelen, PTRS_STRUCTMEMBER_SETTER, node);

			structMemberPrediction(flow, member, ret);
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

		//TODO &ptrs_ast_vtable_member
		if(target->vtable == &ptrs_ast_vtable_identifier)
		{
			struct ptrs_ast_identifier *expr = &target->arg.identifier;
			//setAddressable(flow, expr->location);

			ret->knownType = true;
			ret->knownValue = false;
			ret->knownMeta = true;

			ret->meta.type = PTRS_TYPE_POINTER;
			ret->meta.array.readOnly = false;
			ret->meta.array.size = 1;
		}
		//TODO &ptrs_ast_vtable_importedsymbol
		//TODO &ptrs_ast_vtable_index
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
			struct ptrs_structmember *member = ptrs_struct_find(struc,
				name, 32, PTRS_STRUCTMEMBER_SETTER, node);

			structMemberPrediction(flow, member, ret);
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

		// TODO prediction2str for ret
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
	/*
	TODO
	&ptrs_ast_vtable_op_in,
	&ptrs_ast_vtable_op_instanceof,
	&ptrs_ast_vtable_op_logicor,
	&ptrs_ast_vtable_op_logicand,
	&ptrs_ast_vtable_op_logicxor,
	*/
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
					ptrs_var_t result = binaryIntrinsicHandler[i].intrinsic(
						node, ret->value, ret->meta, dummy.value, dummy.meta
					);
				}
				// TODO else if(ret->knownType && dummy.knownType)
				else
				{
					clearPrediction(ret);
				}

				return;
			}
		}

		for(int i = 0; i < sizeof(unaryIntFloatHandler) / sizeof(void *); i++)
		{
			if(node->vtable == unaryIntFloatHandler[i])
			{
				analyzeExpression(flow, node->arg.astval, ret);
				ret->knownValue = false; // TODO

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
		ptrs_predictions_t *predictions = dupPredictions(flow->predictions); \
		body \
		mergePredictions(flow, predictions); \
		\
		predictions = dupPredictions(flow->predictions); \
		body \
		mergePredictions(flow, predictions); \
		\
		predictions = dupPredictions(flow->predictions); \
		body \
		mergePredictions(flow, predictions); \
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

		createVariablePrediction(flow, &stmt->location, ret, false, stmt);
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
		|| node->vtable == &ptrs_ast_vtable_delete
		|| node->vtable == &ptrs_ast_vtable_throw)
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
				analyzeFunction(flow, curr->value.function.ast);
			}
		}

		struct ptrs_opoverload *curr = struc->overloads;
		while(curr != NULL)
		{
			analyzeFunction(flow, curr->handler);
			curr = curr->next;
		}

		ret->knownType = true;
		ret->knownValue = true;
		ret->knownMeta = true;
		ret->value.structval = NULL;
		ptrs_meta_setPointer(ret->meta, struc);
		setVariablePrediction(flow, struc->location, ret);
	}
	else if(node->vtable == &ptrs_ast_vtable_function)
	{
		analyzeFunction(flow, &node->arg.function.func);
	}
	else if(node->vtable == &ptrs_ast_vtable_if)
	{
		struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;
		analyzeExpression(flow, stmt->condition, &dummy);

		bool condition;
		bool conditionPredicted = prediction2bool(&dummy, &condition);

		ptrs_predictions_t *ifPredictions = flow->predictions;
		ptrs_predictions_t *elsePredictions;
		if(conditionPredicted || flow->dryRun)
			elsePredictions = flow->predictions;
		else
			elsePredictions = dupPredictions(flow->predictions);

		if(!conditionPredicted || condition)
		{
			flow->predictions = ifPredictions;
			analyzeStatement(flow, stmt->ifBody, ret);
		}
		
		if((!conditionPredicted || !condition) && stmt->elseBody != NULL)
		{
			flow->predictions = elsePredictions;
			analyzeStatement(flow, stmt->elseBody, ret);
		}

		if(!conditionPredicted && !flow->dryRun)
		{
			flow->predictions = ifPredictions;
			mergePredictions(flow, elsePredictions);
		}
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

			flow->dryRun = foundCase;
			analyzeStatement(flow, stmt->defaultCase, &dummy);

			flow->dryRun = orginalDryRun;
		}
		else
		{
			analyzeStatement(flow, stmt->defaultCase, &dummy);

			while(curr)
			{
				ptrs_predictions_t *prevPredictions = dupPredictions(flow->predictions);
				analyzeStatement(flow, curr->body, ret);
				mergePredictions(flow, prevPredictions);

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
			if(prediction2bool(&dummy, &condition) && condition)
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
			if(prediction2bool(&dummy, &condition) && condition)
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
			if(prediction2bool(&dummy, &condition) && condition)
				return;

			analyzeStatement(flow, stmt->body, &dummy);
			analyzeExpression(flow, stmt->step, &dummy);
		);
	}
	else if(node->vtable == &ptrs_ast_vtable_forin)
	{
		struct ptrs_ast_forin *stmt = &node->arg.forin;

		loopPredictionsRemerge(
			analyzeExpression(flow, stmt->value, &dummy);
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
	flow.dryRun = false;

	struct ptrs_astlist *curr = ast->arg.astlist;
	while(curr != NULL)
	{
		analyzeStatement(&flow, curr->entry, &ret);
		curr = curr->next;
	}

	freePredictions(flow.predictions);
}
