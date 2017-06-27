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
#include "include/run.h"

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
	stmt->location.val = jit_value_create(func, jit_type_long);
	stmt->location.meta = jit_value_create(func, jit_type_ulong);

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

	val.meta = ptrs_jit_arrayMeta(func, jit_const_long(func, ulong, PTRS_TYPE_NATIVE), jit_const_long(func, ulong, false), size);

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
		jit_value_t initSize = ptrs_jit_getArraySize(func, init.meta);
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

	val.meta = ptrs_jit_arrayMeta(func, jit_const_long(func, ulong, PTRS_TYPE_POINTER), jit_const_long(func, ulong, false), size);

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

static void importScript(ptrs_ast_t *node, char *from, ptrs_val_t **values, ptrs_meta_t **metas)
{
	//TODO
}
static void importNative(ptrs_ast_t *node, char *from, ptrs_val_t **values, ptrs_meta_t **metas)
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
		metas[i]->type = PTRS_TYPE_NATIVE;
		metas[i]->array.size = 0;
		metas[i]->array.readOnly = true;
		values[i]->nativeval = dlsym(handle, curr->name);

		//TODO do wildcards need special care?

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);

		curr = curr->next;
	}
}
void ptrs_import(ptrs_ast_t *node, ptrs_val_t fromVal, ptrs_meta_t fromMeta, ptrs_val_t **values, ptrs_meta_t **metas)
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
		importScript(node, path, values, metas);
	else
		importNative(node, path, values, metas);
}

ptrs_jit_var_t ptrs_handle_import(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_import *stmt = &node->arg.import;

	ptrs_jit_var_t from;
	if(stmt->from)
	{
		from = stmt->from->handler(stmt->from, func, scope);
	}
	else
	{
		from.val = jit_const_long(func, long, 0);
		from.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	}

	int len = 0;
	struct ptrs_importlist *curr;

	curr = stmt->imports;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	jit_value_t values = jit_insn_alloca(func, jit_const_int(func, nuint, len * (sizeof(ptrs_val_t) + sizeof(ptrs_meta_t))));
	jit_value_t metas = jit_insn_add(func, values, jit_const_int(func, nuint, len * sizeof(ptrs_val_t)));

	curr = stmt->imports;
	for(int i = 0; i < len; i++)
	{
		jit_value_t index = jit_const_int(func, nuint, i);
		curr->location.val = jit_value_create(func, jit_type_long);
		curr->location.meta = jit_value_create(func, jit_type_ulong);

		jit_value_set_addressable(curr->location.val);
		jit_value_set_addressable(curr->location.meta);

		jit_insn_store_elem(func, values, index, jit_insn_address_of(func, curr->location.val));
		jit_insn_store_elem(func, metas, index, jit_insn_address_of(func, curr->location.meta));

		curr = curr->next;
	}

	jit_value_t params[] = {
		jit_const_int(func, void_ptr, (uintptr_t)node),
		from.val,
		from.meta,
		values,
		metas
	};

	jit_type_t paramDef[] = {
		jit_type_void_ptr,
		jit_type_long,
		jit_type_ulong,
		jit_type_void_ptr,
		jit_type_void_ptr,
	};
	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, paramDef, 5, 1);

	jit_insn_call_native(func, "import", ptrs_import, signature, params, 5, 0);
	jit_type_free(signature);

	return from; //doh
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

ptrs_jit_var_t ptrs_handle_delete(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_throw(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_trycatch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

#ifndef _PTRS_PORTABLE
ptrs_jit_var_t ptrs_handle_asm(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}
#endif

ptrs_jit_var_t ptrs_handle_function(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_function *ast = &node->arg.function;
	ptrs_function_t *funcAst = &ast->func;

	if(!ast->isAnonymous && ast->symbol->val == NULL)
	{
		ast->symbol->val = jit_value_create(func, jit_type_long);
		ast->symbol->meta = jit_value_create(func, jit_type_ulong);
	}

	jit_type_t argDef[funcAst->argc * 2];
	for(int i = 0; i < funcAst->argc; i++)
	{
		argDef[i * 2] = jit_type_long;
		argDef[i * 2 + 1] = jit_type_ulong;
	}

	//TODO variadic functions

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_long, argDef, funcAst->argc, 1);

	jit_function_t self = jit_function_create(ptrs_jit_context, signature);
	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_NAME, funcAst->name, NULL, 0);
	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_FILE, (char *)node->file, NULL, 0);

	jit_type_free(signature);

	ptrs_scope_t selfScope;
	selfScope.continueLabel = jit_label_undefined;
	selfScope.breakLabel = jit_label_undefined;
	selfScope.errors = NULL;

	for(int i = 0; i < funcAst->argc; i++)
	{
		funcAst->args[i].val = jit_value_create(func, jit_type_long);
		funcAst->args[i].meta = jit_value_create(func, jit_type_ulong);

		jit_insn_store(func, funcAst->args[i].val, jit_value_get_param(self, i * 2));
		jit_insn_store(func, funcAst->args[i].meta, jit_value_get_param(self, i * 2 + 1));

		if(funcAst->argv != NULL && funcAst->argv[i] != NULL)
		{
			jit_label_t given = jit_label_undefined;
			jit_insn_branch_if_not(self, ptrs_jit_hasType(self, funcAst->args[i].meta, PTRS_TYPE_UNDEFINED), &given);

			ptrs_jit_var_t val = funcAst->argv[i]->handler(funcAst->argv[i], self, &selfScope);
			jit_insn_store(func, funcAst->args[i].val, val.val);
			jit_insn_store(func, funcAst->args[i].meta, val.meta);

			jit_insn_label(self, &given);
		}
	}

	funcAst->body->handler(funcAst->body, self, &selfScope);

	jit_function_compile(self);

	void *closure = jit_function_to_closure(self);

	ptrs_jit_var_t ret;
	ret.val = jit_const_int(func, void_ptr, (uintptr_t)closure);
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_NATIVE);

	if(!ast->isAnonymous)
	{
		jit_insn_store(func, ast->symbol->val, ret.val);
		jit_insn_store(func, ast->symbol->meta, ret.meta);
	}

	return ret;
}

ptrs_jit_var_t ptrs_handle_struct(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_if(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;

	ptrs_jit_var_t condition = stmt->condition->handler(stmt->condition, func, scope);

	jit_label_t isFalse = jit_label_undefined;
	ptrs_jit_branch_if_not(func, &isFalse, condition.val, condition.meta);

	stmt->ifBody->handler(stmt->ifBody, func, scope);

	if(stmt->elseBody != NULL)
	{
		jit_label_t end = jit_label_undefined;
		jit_insn_branch(func, &end);

		jit_insn_label(func, &isFalse);
		stmt->elseBody->handler(stmt->elseBody, func, scope);

		jit_insn_label(func, &end);
	}
	else
	{
		jit_insn_label(func, &isFalse);
	}

	return condition;
}

ptrs_jit_var_t ptrs_handle_switch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_while(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_control *stmt = &node->arg.control;
	ptrs_jit_var_t val;

	jit_label_t oldContinue = scope->continueLabel;
	jit_label_t oldBreak = scope->breakLabel;
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;

	jit_insn_label(func, &scope->continueLabel);

	//evaluate the condition
	val = stmt->condition->handler(stmt->condition, func, scope);

	jit_label_t end = jit_label_undefined;
	ptrs_jit_branch_if_not(func, &end, val.val, val.meta);

	//run the while body, jumping back the condition check afterwords
	val = stmt->body->handler(stmt->body, func, scope);
	jit_insn_branch(func, &scope->continueLabel);

	//after the loop - patch the end and breaks
	jit_insn_label(func, &end);
	jit_insn_label(func, &scope->breakLabel);

	scope->continueLabel = oldContinue;
	scope->breakLabel = oldBreak;

	return val;
}

ptrs_jit_var_t ptrs_handle_dowhile(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_control *stmt = &node->arg.control;

	jit_label_t oldContinue = scope->continueLabel;
	jit_label_t oldBreak = scope->breakLabel;
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;

	jit_label_t start = jit_label_undefined;
	jit_insn_label(func, &start);

	//run the while body
	ptrs_jit_var_t val = stmt->body->handler(stmt->body, func, scope);

	//patch all continues to after the body & before the condition check
	jit_insn_label(func, &scope->continueLabel);

	//evaluate the condition
	ptrs_jit_var_t condition = stmt->condition->handler(stmt->condition, func, scope);
	ptrs_jit_branch_if(func, &start, condition.val, condition.meta);

	//after the loop - patch the breaks
	jit_insn_label(func, &scope->breakLabel);

	scope->continueLabel = oldContinue;
	scope->breakLabel = oldBreak;

	return val;
}

ptrs_jit_var_t ptrs_handle_for(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_for *stmt = &node->arg.forstatement;
	ptrs_jit_var_t val;

	stmt->init->handler(stmt->init, func, scope);

	jit_label_t oldContinue = scope->continueLabel;
	jit_label_t oldBreak = scope->breakLabel;
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;

	jit_insn_label(func, &scope->continueLabel);

	//evaluate the condition
	val = stmt->condition->handler(stmt->condition, func, scope);

	jit_label_t end = jit_label_undefined;
	ptrs_jit_branch_if_not(func, &end, val.val, val.meta);

	//run the while body, jumping back the condition check afterwords
	val = stmt->body->handler(stmt->body, func, scope);

	//run the step expression
	stmt->step->handler(stmt->step, func, scope);

	jit_insn_branch(func, &scope->continueLabel);

	//after the loop - patch the end and breaks
	jit_insn_label(func, &end);
	jit_insn_label(func, &scope->breakLabel);

	scope->continueLabel = oldContinue;
	scope->breakLabel = oldBreak;

	return val;
}

ptrs_jit_var_t ptrs_handle_forin(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_file(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.scopestatement.body;

	ptrs_jit_var_t val = body->handler(body, func, scope);

	//TODO return val via jit_insn_return
	return val;
}

ptrs_jit_var_t ptrs_handle_scopestatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_exprstatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		return expr->handler(expr, func, scope);
}
