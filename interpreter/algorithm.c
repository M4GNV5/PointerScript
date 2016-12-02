#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/call.h"
#include "include/struct.h"
#include "include/astlist.h"

ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);
ptrs_var_t *ptrs_handle_yield_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope);

struct ptrs_algoEntry
{
	ptrs_var_t valv;
	ptrs_var_t *val;
	struct ptrs_algorithmlist *entry;
};

struct ptrs_algoContext
{
	int index;
	int len;
	int inputIndex;
	int outputIndex;

	ptrs_var_t yieldVar;
	ptrs_var_t currv;
	ptrs_var_t *curr;
	struct ptrs_algoEntry *handler;

	ptrs_ast_t *node;
	ptrs_scope_t *scope;
};

bool ptrs_algorithm_step(struct ptrs_algoContext *ctx)
{
	ptrs_var_t *handler = ctx->handler[ctx->index].val;
	struct ptrs_algorithmlist *listEntry = ctx->handler[ctx->index].entry;
	ptrs_var_t valv;
	ptrs_var_t *val = listEntry->flags >= 2 ? &ctx->currv : &valv;

	if(listEntry->flags == 3)
	{
		val = ctx->curr;
		if(ctx->index == 0)
		{
			ptrs_error(ctx->node, ctx->scope, "Array index part filter cannot be source to an algorithm expression");
		}
		else if(ctx->index == ctx->len - 1)
		{
			ptrs_error(ctx->node, ctx->scope, "Array index part filter cannot be destination of an algorithm expression");
		}
		else if(val->type == PTRS_TYPE_NATIVE || val->type == PTRS_TYPE_POINTER)
		{
			int64_t index = ptrs_vartoi(handler);

			if(index < 0 || index > val->meta.array.size)
				ptrs_error(ctx->node, ctx->scope, "Index %d is out of range of array of size %d", index, val->meta.array.size);

			if(val->type == PTRS_TYPE_NATIVE)
			{
				ctx->currv.type = PTRS_TYPE_INT;
				ctx->currv.value.intval = val->value.strval[index];
				ctx->curr = &ctx->currv;
			}
			else
			{
				ctx->curr = val->value.ptrval + index;
			}
		}
		else
		{
			ptrs_error(ctx->node, ctx->scope,
				"Cannot use array index filter on a variable of type %s", ptrs_typetoa(val->type));
		}

		return true;
	}

	if(handler->type == PTRS_TYPE_STRUCT)
	{
		ptrs_var_t overload;

		if(ctx->index == 0)
		{
			if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, true)) != NULL)
			{
				ctx->index++;
				overload.type = PTRS_TYPE_FUNCTION;
				ptrs_callfunc(ctx->node, &valv, ctx->scope, handler->value.structval, &overload, 1, &ctx->yieldVar);

				ctx->index = -1;
				return false;
			}
			else
			{
				ptrs_error(ctx->node, ctx->scope,
					"Struct %s does not overload 'this => any'", handler->value.structval->name);
			}
		}
		else if(ctx->index == ctx->len - 1)
		{
			if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_algorithm, false)) != NULL)
			{
				overload.type = PTRS_TYPE_FUNCTION;
				val = ptrs_callfunc(ctx->node, &valv, ctx->scope, handler->value.structval, &overload, 1, ctx->curr);
			}
			else
			{
				ptrs_error(ctx->node, ctx->scope,
					"Struct %s does not overload 'val => this'", handler->value.structval->name);
			}
		}
		else
		{
			if(listEntry->flags != 0)
				ptrs_error(ctx->node, ctx->scope, "Cannot use ? or ! on struct %s in an algorithm expression",
					handler->value.structval->name);

			if(listEntry->orCombine)
				ptrs_error(ctx->node, ctx->scope, "Cannot use || on struct %s in an algorithm expression",
					handler->value.structval->name);

			if((overload.value.funcval = ptrs_struct_getOverload(handler, ptrs_handle_yield_algorithm, false)) != NULL)
			{
				ctx->index++;
				overload.type = PTRS_TYPE_FUNCTION;
				ptrs_var_t args[2];
				memcpy(args, &ctx->yieldVar, sizeof(ptrs_var_t));
				memcpy(args + 1, ctx->curr, sizeof(ptrs_var_t));

				val = ptrs_callfunc(ctx->node, &valv, ctx->scope, handler->value.structval, &overload, 2, args);
			}
			else
			{
				ptrs_error(ctx->node, ctx->scope,
					"Struct %s does not overload 'val => this => any'", handler->value.structval->name);
			}
		}

		if(val->type == PTRS_TYPE_UNDEFINED)
			return false;
		ctx->index = -1;
		return true;
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
				ptrs_error(ctx->node, ctx->scope,
					"Invalid variable of type %s as algorithm input", ptrs_typetoa(handler->type));
		}
	}
	else
	{
		switch(handler->type)
		{
			case PTRS_TYPE_FUNCTION:
				val = ptrs_callfunc(ctx->node, val, ctx->scope, NULL, handler, 1, ctx->curr);
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
				ptrs_error(ctx->node, ctx->scope,
					"Invalid variable of type %s as algorithm filter", ptrs_typetoa(handler->type));
		}

		if(val->type == PTRS_TYPE_UNDEFINED)
			return false;

		if(listEntry->flags == 2)
		{
			ctx->curr = val;
		}
		else
		{
			bool shouldContinue = ptrs_vartob(val);
			if(listEntry->flags == 1)
				shouldContinue = !shouldContinue;

			if(listEntry->orCombine)
			{
				ctx->index++;
				if(!shouldContinue)
				{
					shouldContinue = true;

					if(!ptrs_algorithm_step(ctx))
						return false;
				}
			}

			if(!shouldContinue)
				ctx->index = -1;
		}

		return true;
	}

	return true;
}

ptrs_var_t *ptrs_handle_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_algoContext ctx = {
		.index = 0,
		.len = 0,
		.inputIndex = 0,
		.outputIndex = 0,

		.node = node,
		.scope = scope,
		.yieldVar = {
			.type = PTRS_TYPE_NATIVE,
			.value = {.nativeval = &ctx}
		}
	};

	struct ptrs_algorithmlist *curr = node->arg.algolist;
	while(curr != NULL)
	{
		ctx.len++;
		curr = curr->next;
	}

	struct ptrs_algoEntry handler[ctx.len];
	curr = node->arg.algolist;
	for(int i = 0; i < ctx.len; i++)
	{
		handler[i].val = curr->entry->handler(curr->entry, &(handler[i].valv), scope);
		handler[i].entry = curr;
		curr = curr->next;
	}

	ctx.handler = handler;

	while(ptrs_algorithm_step(&ctx))
	{
		if(++ctx.index >= ctx.len)
			ctx.index = 0;
	}

	result->type = PTRS_TYPE_UNDEFINED;
	return result;
}

ptrs_var_t *ptrs_handle_yield_algorithm(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_yield expr = node->arg.yield;
	struct ptrs_algoContext *ctx = ptrs_scope_get(scope, expr.yieldVal)->value.nativeval;
	ctx->curr = expr.value->handler(expr.value, &ctx->currv, scope);

	int index = ctx->index;
	ptrs_ast_t *oldAst = ctx->node;
	ptrs_scope_t *oldScope = ctx->scope;
	ctx->node = node;
	ctx->scope = scope;

	result->type = PTRS_TYPE_INT;
	while(ptrs_algorithm_step(ctx))
	{
		if(ctx->index < index || ++ctx->index >= ctx->len)
		{
			ctx->index = index;
			ctx->node = oldAst;
			ctx->scope = oldScope;
			result->value.intval = 0;
			return result;
		}
	}

	ctx->index = index;
	ctx->node = oldAst;
	ctx->scope = oldScope;
	result->value.intval = 1;
	return result;
}
