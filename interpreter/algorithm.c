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
	ptrs_var_t valv;
	ptrs_var_t *val;

	if(handler->type == PTRS_TYPE_STRUCT)
	{
		ptrs_var_t overload;

		if(ctx->index == 0)
		{
			if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, true)) != NULL)
			{
				overload.type = PTRS_TYPE_FUNCTION;
				ctx->curr = ptrs_callfunc(ctx->node, &ctx->currv, ctx->scope, handler->value.structval, &overload, 0, NULL);

				if(ctx->curr->type == PTRS_TYPE_UNDEFINED)
					return false;
				return true;
			}
		}
		else if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, false)) != NULL)
		{
			overload.type = PTRS_TYPE_FUNCTION;
			val = ptrs_callfunc(ctx->node, &valv, ctx->scope, handler->value.structval, &overload, 1, ctx->curr);

			if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, true)) != NULL)
			{
				overload.type = PTRS_TYPE_FUNCTION;
				int index = ++ctx->index;

				val = ctx->curr;
				while(true)
				{
					ctx->curr = ptrs_callfunc(ctx->node, &valv, ctx->scope, handler->value.structval, &overload, 0, NULL);

					if(ctx->curr->type == PTRS_TYPE_UNDEFINED)
						break;

					ctx->index = index;
					while(ptrs_algorithm_step(ctx))
					{
						if(ctx->index < 0 || ++ctx->index >= ctx->len)
							break;
					}
				}

				ctx->curr = val;
				ctx->index = -1;
				return true;
			}
			else if(val->type == PTRS_TYPE_UNDEFINED)
			{
				return false;
			}
			else if(!ptrs_vartob(val))
			{
				ctx->index = -1;
			}

			return true;
		}

		ptrs_error(ctx->node, ctx->scope, "Struct %s does not overload 'this => val'", handler->value.structval->name);
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

				if(handler->meta.array.readOnly)
					ctx->curr->value = ptrs_callnative(PTRS_TYPE_INT, handler->value.nativeval, 0, NULL);
				else if(ctx->inputIndex < handler->meta.array.size)
					ctx->curr->value.intval = ((uint8_t *)handler->value.nativeval)[ctx->inputIndex++];
				else
					return false;
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
		switch(handler->type)
		{
			case PTRS_TYPE_FUNCTION:
				val = ptrs_callfunc(ctx->node, &valv, ctx->scope, NULL, handler, 1, ctx->curr);
				break;
			case PTRS_TYPE_NATIVE:
				if(!handler->meta.array.readOnly && ctx->index == ctx->len - 1)
				{
					if(ctx->outputIndex >= handler->meta.array.size)
						return false;

					((uint8_t *)handler->value.nativeval)[ctx->outputIndex++] = ptrs_vartoi(ctx->curr);
					return true;
				}
				else
				{
					val = &valv;
					val->type = PTRS_TYPE_INT;
					val->value = ptrs_callnative(PTRS_TYPE_INT, handler->value.nativeval, 1, ctx->curr);
				}
				break;
			case PTRS_TYPE_POINTER:
				if(ctx->index == ctx->len - 1)
				{
					if(ctx->outputIndex >= handler->meta.array.size)
						return false;

					memcpy(handler->value.ptrval + ctx->outputIndex++, ctx->curr, sizeof(ptrs_var_t));
					return true;
				}
				//fallthrough
			default:
				ptrs_error(ctx->node, ctx->scope, "Invalid variable of type %s as algorithm filter", ptrs_typetoa(handler->type));
		}

		if(val->type == PTRS_TYPE_UNDEFINED)
			return false;
		else if(!ptrs_vartob(val))
			ctx->index = -1;

		return true;
	}

	return true;
}

ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_algoContext ctx;
	ctx.index = 0;
	ctx.inputIndex = 0;
	ctx.outputIndex = 0;
	ctx.node = node;
	ctx.scope = scope;

	ctx.len = ptrs_astlist_length(node->arg.astlist, node, scope);
	ptrs_var_t handler[ctx.len];
	ptrs_astlist_handle(node->arg.astlist, handler, scope);

	ctx.handler = handler;

	while(ptrs_algorithm_step(&ctx))
	{
		if(++ctx.index >= ctx.len)
			ctx.index = 0;
	}

	result->type = PTRS_TYPE_UNDEFINED;
	return result;
}
