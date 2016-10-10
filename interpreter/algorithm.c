#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/stack.h"
#include "include/call.h"
#include "include/struct.h"
#include "include/astlist.h"

ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

struct ptrs_algoContext
{
	int index;
	int len;
	int inputIndex;
	int outputIndex;

	ptrs_var_t currv;
	ptrs_var_t *curr;
	ptrs_var_t *handler;

	ptrs_ast_t *node;
	ptrs_scope_t *scope;
};

bool ptrs_algorithm_step(struct ptrs_algoContext *ctx)
{
	ptrs_var_t *handler = ctx->handler + ctx->index;

	if(handler->type == PTRS_TYPE_STRUCT)
	{
		//TODO

		ptrs_var_t overload;
		if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, false)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;
			ptrs_callfunc(ctx->node, NULL, ctx->scope, handler->value.structval, &overload, 1, ctx->curr);
		}
	}
	
	if(ctx->index == 0)
	{
		switch(handler->type)
		{
			case PTRS_TYPE_FUNCTION:
				ctx->curr = ptrs_callfunc(ctx->node, &ctx->currv, ctx->scope, NULL, handler, 0, NULL);
				break;
			case PTRS_TYPE_NATIVE:
				ctx->curr = &ctx->currv;
				ctx->curr->type = PTRS_TYPE_INT;
				ctx->curr->value = ptrs_callnative(PTRS_TYPE_INT, handler->value.nativeval, 0, NULL);
				break;
			case PTRS_TYPE_POINTER:
				if(ctx->inputIndex < handler->meta.array.size)
					ctx->curr = handler->value.ptrval + ctx->inputIndex++;
				else
					return false;
				break;
			default:
				ptrs_error(ctx->node, ctx->scope, "Invalid variable of type %s as algorithm input", ptrs_typetoa(handler->type));
		}
	}
	else
	{
		ptrs_var_t valv;
		ptrs_var_t *val;
		switch(handler->type)
		{
			case PTRS_TYPE_FUNCTION:
				val = ptrs_callfunc(ctx->node, &valv, ctx->scope, NULL, handler, 1, ctx->curr);
				break;
			case PTRS_TYPE_NATIVE:
				val = &valv;
				val->type = PTRS_TYPE_INT;
				val->value = ptrs_callnative(PTRS_TYPE_INT, handler->value.nativeval, 1, ctx->curr);
				break;
			case PTRS_TYPE_POINTER:
				if(ctx->index == ctx->len - 1)
				{
					if(ctx->outputIndex < handler->meta.array.size)
						memcpy(handler->value.ptrval + ctx->outputIndex++, ctx->curr, sizeof(ptrs_var_t));
					else
						return false;
					break;
				}
				//fallthrough
			default:
				ptrs_error(ctx->node, ctx->scope, "Invalid variable of type %s as algorithm filter", ptrs_typetoa(handler->type));
		}
	}

	return true;
}

ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	int len = ptrs_astlist_length(node->arg.astlist, node, scope);
	ptrs_var_t handler[len];
	ptrs_astlist_handle(node->arg.astlist, handler, scope);

	//TODO

	result->type = PTRS_TYPE_UNDEFINED;
	return result;
}
