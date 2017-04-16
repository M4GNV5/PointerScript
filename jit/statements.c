#include <stdio.h>
#include <dlfcn.h>
#include <libgen.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/conversion.h"
#include "include/astlist.h"
#include "include/error.h"

void ptrs_handle_body(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	while(list != NULL)
	{
		 list->entry->handler(list->entry, jit, scope);
		 list = list->next;
	}
}

void ptrs_handle_define(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;

	if(stmt->value != NULL)
	{
		stmt->value->handler(stmt->value, jit, scope);
		jit_stxi(jit, R_FP, stmt->fpOffset, R(0), 8);
		jit_stxi(jit, R_FP, stmt->fpOffset + 8, R(1), 1);
	}
	else
	{
		jit_movi(jit, R(1), PTRS_TYPE_UNDEFINED);
		jit_stxi(jit, R_FP, stmt->fpOffset + 8, R(1), 1);
	}
}

void ptrs_handle_lazyinit(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

size_t ptrs_arraymax = PTRS_STACK_SIZE;
void ptrs_handle_array(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	const ptrs_meta_t metaInit = {
		.type = PTRS_TYPE_NATIVE,
		.array = {
			.readOnly = false,
			.size = 0
		}
	};

	if(stmt->value != NULL)
	{
		stmt->value->handler(stmt->value, jit, scope);
		ptrs_jit_convert(jit, ptrs_vartoi, R(0), R(0), R(1));
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	jit_bgti_u(jit, ptrs_jiterror, R(0), ptrs_arraymax);
	ptrs_jit_store_arraysize(stmt->fpOffset, R(0));

	//allocate memory
	jit_prepare(jit);
	jit_putargr(jit, R(0));
	jit_call(jit, stmt->onStack ? alloca : malloc);
	jit_retval(jit, R(0));

	//store the array
	ptrs_jit_load_meta(R(1), stmt->fpOffset);
	jit_ori(jit, R(1), R(1), *(uintptr_t *)&metaInit);

	ptrs_jit_store_val(stmt->fpOffset, R(0));
	ptrs_jit_store_meta(stmt->fpOffset, R(1));

	if(stmt->isInitExpr)
	{
		stmt->initExpr->handler(stmt->initExpr, jit, scope);

		//check type of initExpr
		jit_bmci(jit, ptrs_jiterror, R(1), (uint64_t)PTRS_TYPE_NATIVE << 56);

		//check initExpr.size <= array.size
		jit_andi(jit, R(1), R(1), 0xFFFFFFFF);
		ptrs_jit_load_arraysize(R(2), stmt->fpOffset);
		jit_bgtr_u(jit, ptrs_jiterror, R(0), R(2));

		//copy initExpr memory to aray
		ptrs_jit_load_val(R(2), stmt->fpOffset);
		jit_prepare(jit);
		jit_putargr(jit, R(2)); //dst
		jit_putargr(jit, R(0)); //src
		jit_putargr(jit, R(1)); //length
		jit_call(jit, memcpy);
	}
	else
	{
		ptrs_astlist_handleByte(stmt->initVal, stmt->fpOffset, jit, scope);
	}
}

void ptrs_handle_vararray(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;

	if(stmt->value != NULL)
	{
		stmt->value->handler(stmt->value, jit, scope);
		ptrs_jit_convert(jit, ptrs_vartoi, R(0), R(0), R(1));
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	jit_bgti_u(jit, ptrs_jiterror, R(0), ptrs_arraymax);
	ptrs_jit_store_arraysize(stmt->fpOffset, R(0));

	jit_muli(jit, R(0), R(0), 16); //use sizeof(ptrs_var_t) instead?

	//allocate memory
	jit_prepare(jit);
	jit_putargr(jit, R(0));
	jit_call(jit, stmt->onStack ? alloca : malloc);
	jit_retval(jit, R(0));

	//store the array
	ptrs_jit_store_type(stmt->fpOffset, PTRS_TYPE_POINTER);
	ptrs_jit_store_val(stmt->fpOffset, R(0));

	ptrs_astlist_handle(stmt->initVal, stmt->fpOffset, jit, scope);
}

void ptrs_handle_structvar(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
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

void ptrs_handle_import(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_import *stmt = &node->arg.import;
	//R(0) = from.value
	//R(1) = from.meta
	//R(2) = output ptr

	stmt->from->handler(stmt->from, jit, scope);

	jit_movr(jit, R(2), R_FP);
	jit_addi(jit, R(2), R(2), scope->fpOffset);

	jit_prepare(jit);
	jit_putargr(jit, R(2));
	jit_putargi(jit, node);
	jit_putargr(jit, R(0));
	jit_putargr(jit, R(1));
	jit_call(jit, ptrs_import);
	jit_retval(jit, R(0));

	jit_bnei(jit, ptrs_jiterror, R(0), 0);
}

void ptrs_handle_return(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_break(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_continue(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_delete(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_throw(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_trycatch(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

#ifndef _PTRS_PORTABLE
void ptrs_handle_asm(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}
#endif

void ptrs_handle_function(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_struct(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_if(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_switch(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_while(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_dowhile(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_for(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_forin(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

void ptrs_handle_scopestatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.scopestatement.body;

	scope->fpOffset = jit_allocai(jit, node->arg.scopestatement.stackOffset);
	body->handler(body, jit, scope);
	//TODO make sure FP is decremented
}

void ptrs_handle_exprstatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		expr->handler(expr, jit, scope);
}
