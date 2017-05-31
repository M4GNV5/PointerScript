#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <jit/jit.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/conversion.h"
#include "include/astlist.h"
#include "include/error.h"

ptrs_jit_var_t ptrs_handle_body(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	ptrs_jit_var_t result;

	while(list != NULL)
	{
		result = list->entry->handler(list->entry, func, scope);
		list = list->next;
	}

	return result;
}

ptrs_jit_var_t ptrs_handle_define(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	ptrs_jit_var_t val;

	if(stmt->value != NULL)
	{
		val = stmt->value->handler(stmt->value, func, scope);
	}
	else
	{
		val.val = jit_value_create_long_constant(func, jit_type_long, 0);
		val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	}

	jit_insn_store(func, stmt->location.val, val.val);
	jit_insn_store(func, stmt->location.meta, val.meta);
	return stmt->location;
}

ptrs_jit_var_t ptrs_handle_lazyinit(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

size_t ptrs_arraymax = UINT32_MAX;
ptrs_jit_var_t ptrs_handle_array(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	ptrs_jit_var_t val;
	jit_value_t size;

	if(stmt->value != NULL)
	{
		val = stmt->value->handler(stmt->value, func, scope);
		size = ptrs_jit_vartoi(func, val.val, val.meta);
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	ptrs_jit_assert(node, func, jit_insn_le(func, size, jit_const_int(func, nuint, ptrs_arraymax)),
		1, "Cannot create array of size %d", size);

	//allocate memory
	if(stmt->onStack)
	{
		val.val = jit_insn_alloca(func, size);
	}
	else
	{
		jit_type_t params[] = {jit_type_nuint};
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, 1, 1);

		val.val = jit_insn_call_native(func, "malloc", malloc, signature, &size, 1, JIT_CALL_NOTHROW);
		jit_type_free(signature);
	}

	val.meta = ptrs_jit_arraymeta(func, jit_const_long(func, ulong, PTRS_TYPE_NATIVE), jit_const_long(func, ulong, false), size);

	//store the array
	jit_insn_store(func, stmt->location.val, val.val);
	jit_insn_store(func, stmt->location.meta, val.meta);

	if(stmt->isInitExpr)
	{
		ptrs_jit_var_t init = stmt->initExpr->handler(stmt->initExpr, func, scope);

		//check type of initExpr
		ptrs_jit_assert(node, func, ptrs_jit_hasType(func, init.meta, PTRS_TYPE_NATIVE),
			1, "Array init expression must be of type native not %mt", init.meta);

		//check initExpr.size <= array.size
		jit_value_t initSize = ptrs_jit_get_arraysize(func, init.meta);
		ptrs_jit_assert(node, func, jit_insn_le(func, initSize, size),
			2, "Init expression size of %d is too big for array of size %d", initSize, size);

		//copy initExpr memory to array and zero the rest
		jit_insn_memcpy(func, val.val, init.val, initSize);
		jit_insn_memset(func, jit_insn_add(func, val.val, initSize), jit_const_int(func, ubyte, 0), jit_insn_sub(func, size, initSize));
	}
	else
	{
		ptrs_astlist_handleByte(stmt->initVal, func, scope, val.val, size);
	}

	return val;
}

ptrs_jit_var_t ptrs_handle_vararray(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	ptrs_jit_var_t val;
	jit_value_t size;
	jit_value_t byteSize;

	if(stmt->value != NULL)
	{
		val = stmt->value->handler(stmt->value, func, scope);
		size = ptrs_jit_vartoi(func, val.val, val.meta);
		byteSize = jit_insn_mul(func, size, jit_const_int(func, nuint, sizeof(ptrs_var_t)));
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	ptrs_jit_assert(node, func, jit_insn_le(func, byteSize, jit_const_int(func, nuint, ptrs_arraymax)),
		1, "Cannot create array of size %d", size);

	//allocate memory
	if(stmt->onStack)
	{
		val.val = jit_insn_alloca(func, byteSize);
	}
	else
	{
		jit_type_t params[] = {jit_type_nuint};
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, 1, 1);

		val.val = jit_insn_call_native(func, "malloc", malloc, signature, &byteSize, 1, JIT_CALL_NOTHROW);
		jit_type_free(signature);
	}

	val.meta = ptrs_jit_arraymeta(func, jit_const_long(func, ulong, PTRS_TYPE_POINTER), jit_const_long(func, ulong, false), size);

	//store the array
	jit_insn_store(func, stmt->location.val, val.val);
	jit_insn_store(func, stmt->location.meta, val.meta);

	ptrs_astlist_handle(stmt->initVal, func, scope, val.val, size);
	return val;
}

ptrs_jit_var_t ptrs_handle_structvar(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
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

static void importScript(ptrs_var_t **results, ptrs_ast_t *node, char *from)
{
	//TODO
}
static void importNative(ptrs_var_t **results, ptrs_ast_t *node, char *from)
{
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		handle = dlopen(from, RTLD_NOW);
		free(from);

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);
	}

	struct ptrs_importlist *curr = node->arg.import.imports;
	for(int i = 0; curr != NULL; i++)
	{
		results[i]->meta.type = PTRS_TYPE_NATIVE;
		results[i]->meta.array.size = 0;
		results[i]->value.nativeval = dlsym(handle, curr->name);

		//TODO do wildcards need special care?

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);

		curr = curr->next;
	}
}
void ptrs_import(ptrs_var_t **results, ptrs_ast_t *node, ptrs_val_t fromVal, ptrs_meta_t fromMeta)
{
	char *path = NULL;
	if(fromMeta.type != PTRS_TYPE_UNDEFINED)
	{
		char *from = NULL;
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

			path = realpath(buff, NULL);
		}
		else
		{
			path = realpath(from, NULL);
		}

		if(from == NULL)
			ptrs_error(node, "Error resolving path '%s'", from);
	}

	char *ending;
	if(path == NULL)
	 	ending = NULL;
	else
		ending = strrchr(path, '.');

	if(ending != NULL && strcmp(ending, ".ptrs") == 0)
		importScript(results, node, path);
	else
		importNative(results, node, path);
}

ptrs_jit_var_t ptrs_handle_import(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_return(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_break(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	jit_insn_branch(func, &scope->breakLabel);
	ptrs_jit_var_t ret = {NULL, NULL};
	return ret;
}

ptrs_jit_var_t ptrs_handle_continue(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	jit_insn_branch(func, &scope->continueLabel);
	ptrs_jit_var_t ret = {NULL, NULL};
	return ret;
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
	ptrs_jit_vartob(jit, R(condition), R(condition + 1));
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
	ptrs_jit_vartob(jit, R(condition), R(condition + 1));
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
	ptrs_jit_vartob(jit, R(condition), R(condition + 1));
	jit_bnei(jit, (uintptr_t)body, R(condition), 0);

	ptrs_scope_patch(jit, scope->breakPatches);
	ptrs_scope_restorePatches(&store, scope);
}

unsigned ptrs_handle_for(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	struct ptrs_ast_for *stmt = &node->arg.forstatement;

	stmt->init->handler(stmt->init, jit, scope);

	ptrs_patchstore_t store;
	ptrs_scope_storePatches(&store, scope);

	jit_label *check = jit_get_label(jit);

	//evaluate the condition
	unsigned condition = stmt->condition->handler(stmt->condition, jit, scope);
	ptrs_jit_vartob(jit, R(condition), R(condition + 1));
	jit_op *isFalse = jit_beqi(jit, JIT_FORWARD, R(condition), 0);

	//run the while body, jumping back th 'check' at the end
	stmt->body->handler(stmt->body, jit, scope);

	ptrs_scope_patch(jit, scope->continuePatches);
	stmt->step->handler(stmt->step, jit, scope);

	jit_jmpi(jit, check);

	//after the loop - patch the end and breaks
	jit_patch(jit, isFalse);
	ptrs_scope_patch(jit, scope->breakPatches);
	ptrs_scope_restorePatches(&store, scope);
}

unsigned ptrs_handle_forin(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_file(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.scopestatement.body;

	scope->fpOffset = jit_allocai(jit, node->arg.scopestatement.stackOffset);
	unsigned ret = body->handler(body, jit, scope);
	jit_reti(jit, 0);

	ptrs_jit_compileErrors(jit, scope);
}

unsigned ptrs_handle_scopestatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	//TODO
}

unsigned ptrs_handle_exprstatement(ptrs_ast_t *node, jit_state_t *jit, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		expr->handler(expr, jit, scope);
}
