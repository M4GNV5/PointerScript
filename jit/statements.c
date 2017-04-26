#include <stdio.h>
#include <dlfcn.h>
#include <libgen.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/conversion.h"
#include "include/astlist.h"
#include "include/error.h"
#include "include/scope.h"

unsigned ptrs_handle_body(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	unsigned result = -1;

	while(list != NULL)
	{
		result = list->entry->handler(list->entry, jit, scope);
		list = list->next;
	}

	return result;
}

unsigned ptrs_handle_define(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	unsigned val;

	if(stmt->value != NULL)
	{
		val = stmt->value->handler(stmt->value, jit, scope);
	}
	else
	{
		val = scope->usedRegCount;

		jit_movi(jit, R(val), 0);
		jit_movi(jit, R(val + 1), PTRS_TYPE_UNDEFINED);
	}

	ptrs_scope_store(jit, scope, stmt->symbol, R(val), R(val + 1));
	return val;
}

unsigned ptrs_handle_lazyinit(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

size_t ptrs_arraymax = UINT32_MAX;
unsigned ptrs_handle_array(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	long val = R(scope->usedRegCount++);
	long meta = R(scope->usedRegCount++);

	if(stmt->value != NULL)
	{
		unsigned size = stmt->value->handler(stmt->value, jit, scope);
		ptrs_jit_convert(jit, ptrs_vartoi, meta, R(size), R(size + 1));
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	jit_bgti_u(jit, ptrs_jiterror, meta, ptrs_arraymax);

	//allocate memory
	jit_prepare(jit);
	jit_putargr(jit, meta);
	jit_call(jit, stmt->onStack ? alloca : malloc);
	jit_retval(jit, val);

	//store the array
	ptrs_jit_seti_type(jit, meta, PTRS_TYPE_NATIVE);
	ptrs_scope_store(jit, scope, stmt->symbol, val, meta);

	if(stmt->isInitExpr)
	{
		long init = stmt->initExpr->handler(stmt->initExpr, jit, scope);
		long initMeta = R(init + 1);
		init = R(init);

		//check type of initExpr
		jit_bmci(jit, ptrs_jiterror, initMeta, (uint64_t)PTRS_TYPE_NATIVE << 56);

		//check initExpr.size <= array.size
		jit_andi(jit, initMeta, initMeta, 0xFFFFFFFF);
		jit_andi(jit, meta, meta, 0xFFFFFFFF);
		jit_bgtr_u(jit, ptrs_jiterror, meta, initMeta);

		//copy initExpr memory to array
		jit_prepare(jit);
		jit_putargr(jit, val); //dst
		jit_putargr(jit, init); //src
		jit_putargr(jit, initMeta); //length
		jit_call(jit, memcpy);
	}
	else
	{
		ptrs_astlist_handleByte(stmt->initVal, val, meta, jit, scope);
	}

	scope->usedRegCount -= 2;
	return scope->usedRegCount;
}

unsigned ptrs_handle_vararray(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	long val = R(scope->usedRegCount++);
	long size = R(scope->usedRegCount++);

	if(stmt->value != NULL)
	{
		unsigned sizeVal = stmt->value->handler(stmt->value, jit, scope);
		ptrs_jit_convert(jit, ptrs_vartoi, size, R(sizeVal), R(sizeVal + 1));
	}
	else
	{
		//TODO
	}

	//calculate array size in bytes and make sure its not too much
	jit_muli(jit, size, size, 16); //TODO use sizeof(ptrs_var_t) instead?
	jit_bgti_u(jit, ptrs_jiterror, size, ptrs_arraymax);

	//allocate memory
	jit_prepare(jit);
	jit_putargr(jit, size);
	jit_call(jit, stmt->onStack ? alloca : malloc);
	jit_retval(jit, val);

	ptrs_astlist_handle(stmt->initVal, val, size, jit, scope);

	//store the array
	jit_divi(jit, size, size, 16);
	ptrs_jit_seti_type(jit, size, PTRS_TYPE_POINTER);
	ptrs_scope_store(jit, scope, stmt->symbol, val, size);

	scope->usedRegCount -= 2;
	return scope->usedRegCount;
}

unsigned ptrs_handle_structvar(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

/*
typedef struct ptrs_cache
{
	const char *path;
	ptrs_ast_t *ast;
	ptrs_scope_t *scope;
	ptrs_symboltable_t *symbols;
	struct ptrs_cache *next;
} ptrs_cache_t;
ptrs_cache_t *ptrs_cache = NULL;
ptrs_cache_t *importCachedScript(char *path, ptrs_ast_t *node, ptrs_scope_t *scope)
{
	ptrs_cache_t *cache = ptrs_cache;
	while(cache != NULL)
	{
		if(strcmp(cache->path, path) == 0)
		{
			free(path);
			return cache;
		}
		cache = cache->next;
	}

	ptrs_scope_t *_scope = calloc(1, sizeof(ptrs_scope_t));
	ptrs_symboltable_t *symbols = NULL;
	ptrs_var_t valuev;

	ptrs_lastast = NULL;
	ptrs_lastscope = NULL;
	ptrs_ast_t *ast = ptrs_dofile(path, &valuev, _scope, &symbols);
	ptrs_lastast = node;
	ptrs_lastscope = scope;

	cache = malloc(sizeof(ptrs_cache_t));
	cache->path = path;
	cache->ast = ast;
	cache->scope = _scope;
	cache->symbols = symbols;
	cache->next = ptrs_cache;
	ptrs_cache = cache;
	return cache;
}
*/

static const char *importScript(void **output, ptrs_ast_t *node, char *from)
{
	//TODO
	return NULL;
}
static const char *importNative(void **output, ptrs_ast_t *node, char *from)
{
	struct ptrs_ast_import *stmt = &node->arg.import;
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		handle = dlopen(from, RTLD_NOW);
		free(from);

		error = dlerror();
		if(error != NULL)
			return error;
	}

	struct ptrs_importlist *curr = node->arg.import.imports;
	for(int i = 0; curr != NULL; i++)
	{
		output[i] = dlsym(handle, curr->name);
		//TODO do wildcards need special care?

		error = dlerror();
		if(error != NULL)
			return error;

		curr = curr->next;
	}

	return NULL;
}
const char *ptrs_import(void **output, ptrs_ast_t *node, ptrs_val_t fromVal, ptrs_meta_t fromMeta)
{
	char *from = NULL;
	if(fromMeta.type != PTRS_TYPE_UNDEFINED)
	{
		if(fromMeta.type == PTRS_TYPE_NATIVE)
		{
			int len = strnlen(fromVal.strval, fromMeta.array.size);
			if(len < fromMeta.array.size)
			{
				from = (char *)fromVal.strval;
			}
			else
			{
				from = alloca(len) + 1;
				memcpy(from, fromVal.strval, len);
				from[len] = 0;
			}
		}
		else
		{
			from = alloca(32);
			ptrs_vartoa(fromVal, fromMeta, from, 32);
		}

		if(from[0] != '/')
		{
			char dirbuff[strlen(node->file) + 1];
			strcpy(dirbuff, node->file);
			char *dir = dirname(dirbuff);

			char buff[strlen(dir) + strlen(from) + 2];
			sprintf(buff, "%s/%s", dir, from);

			from = realpath(buff, NULL);
		}
		else
		{
			from = realpath(from, NULL);
		}

		if(from == NULL)
			return "Error resolving path";
	}

	char *ending = strrchr(from, '.');
	if(ending != NULL && strcmp(ending, ".ptrs") == 0)
		return importScript(output, node, from);
	else
		return importNative(output, node, from);
}

unsigned ptrs_handle_import(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_import *stmt = &node->arg.import;
	//R(0) = from.value
	//R(1) = from.meta
	//R(2) = output ptr

	unsigned from = stmt->from->handler(stmt->from, jit, scope);
	long ptr = R(scope->usedRegCount);

	jit_addi(jit, ptr, R_FP, scope->fpOffset);

	jit_prepare(jit);
	jit_putargr(jit, ptr);
	jit_putargi(jit, node);
	jit_putargr(jit, R(from));
	jit_putargr(jit, R(from + 1));
	jit_call(jit, ptrs_import);
	jit_retval(jit, ptr);

	jit_bnei(jit, ptrs_jiterror, ptr, 0);
}

unsigned ptrs_handle_return(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	if(node->arg.astval == NULL)
	{
		jit_reti(jit, 0);
	}
	else
	{
		unsigned val = node->arg.astval->handler(node->arg.astval, jit, scope);
		//TODO how to return multiple values?
		jit_retr(jit, R(0));
	}
}

unsigned ptrs_handle_break(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	if(scope->breakLabel == 0)
	{
		ptrs_patchlist_t *entry = malloc(sizeof(ptrs_patchlist_t));
		entry->patch = jit_jmpi(jit, JIT_FORWARD);
		entry->next = scope->breakPatches;
		scope->breakPatches = entry;
	}
	else
	{
		jit_jmpi(jit, scope->breakLabel);
	}
	return -1;
}

unsigned ptrs_handle_continue(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	if(scope->continueLabel == 0)
	{
		ptrs_patchlist_t *entry = malloc(sizeof(ptrs_patchlist_t));
		entry->patch = jit_jmpi(jit, JIT_FORWARD);
		entry->next = scope->continuePatches;
		scope->continuePatches = entry;
	}
	else
	{
		jit_jmpi(jit, scope->continueLabel);
	}
	return -1;
}

unsigned ptrs_handle_delete(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_throw(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_trycatch(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

#ifndef _PTRS_PORTABLE
unsigned ptrs_handle_asm(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
#endif

unsigned ptrs_handle_function(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_struct(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_if(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;

	unsigned condition = stmt->condition->handler(stmt->condition, jit, scope);
	ptrs_jit_vartob(jit, R(condition), R(condition - 1));
	jit_op *isFalse = jit_beqi(jit, JIT_FORWARD, R(condition), 0);

	stmt->ifBody->handler(stmt->ifBody, jit, scope);

	if(stmt->elseBody != NULL)
	{
		//let the end of the true body jump over the else body
		jit_op *end = jit_jmpi(jit, JIT_FORWARD);

		//patch the jumps to the else body
		jit_patch(jit, isFalse);
		stmt->elseBody->handler(stmt->elseBody, jit, scope);

		jit_patch(jit, end);
	}
	else
	{
		jit_patch(jit, isFalse);
	}

	return -1;
}

unsigned ptrs_handle_switch(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_while(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_control *stmt = &node->arg.control;
	ptrs_patchstore_t store;
	ptrs_scope_storePatches(&store, scope);

	jit_label *check = jit_get_label(jit);
	scope->continueLabel = (uintptr_t)check;

	//evaluate the condition
	unsigned condition = stmt->condition->handler(stmt->condition, jit, scope);
	ptrs_jit_vartob(jit, R(condition), R(condition - 1));
	jit_op *isFalse = jit_beqi(jit, JIT_FORWARD, R(condition), 0);

	//run the while body, jumping back th 'check' at the end
	stmt->body->handler(stmt->body, jit, scope);
	jit_jmpi(jit, check);

	//after the loop - patch the end and breaks
	jit_patch(jit, isFalse);
	ptrs_scope_patch(jit, scope->breakPatches);
	ptrs_scope_restorePatches(&store, scope);
}

unsigned ptrs_handle_dowhile(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_control *stmt = &node->arg.control;
	ptrs_patchstore_t store;
	ptrs_scope_storePatches(&store, scope);

	//run the while body
	jit_label *body = jit_get_label(jit);
	stmt->body->handler(stmt->body, jit, scope);

	//patch all continues to after the body & before the condition check
	ptrs_scope_patch(jit, scope->continuePatches);

	//evaluate the condition
	unsigned condition = stmt->condition->handler(stmt->condition, jit, scope);
	ptrs_jit_vartob(jit, R(condition), R(condition - 1));
	jit_bnei(jit, (uintptr_t)body, R(condition), 0);

	ptrs_scope_patch(jit, scope->breakPatches);
	ptrs_scope_restorePatches(&store, scope);
}

unsigned ptrs_handle_for(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_forin(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_scopestatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.scopestatement.body;

	scope->fpOffset = jit_allocai(jit, node->arg.scopestatement.stackOffset);
	body->handler(body, jit, scope);
	//TODO make sure FP is decremented
}

unsigned ptrs_handle_exprstatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		expr->handler(expr, jit, scope);
}
