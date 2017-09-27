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
#include "include/util.h"
#include "include/call.h"
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

	stmt->location.val = jit_value_create(func, jit_type_long);
	stmt->location.meta = jit_value_create(func, jit_type_ulong);
	stmt->location.constType = -1;

	jit_insn_store(func, stmt->location.val, val.val);
	jit_insn_store(func, stmt->location.meta, val.meta);
	return stmt->location;
}

ptrs_jit_var_t ptrs_handle_typeddefine(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_define *stmt = &node->arg.define;
	ptrs_jit_var_t val;
	if(stmt->value != NULL)
	{
		val = stmt->value->handler(stmt->value, func, scope);

		if(val.constType == -1 && stmt->type > PTRS_TYPE_STRUCT)
			ptrs_error(node, "Initializer of untyped let statement has a dynamic type");
		else if(stmt->type > PTRS_TYPE_STRUCT)
			stmt->location.constType = val.constType;
		else if(val.constType == -1)
			stmt->location.constType = stmt->type;
		else if(val.constType != stmt->type)
			ptrs_error(node, "Variable is defined as %t but initializer has type %t",
				stmt->type, val.constType);
	}
	else
	{
		val.val = jit_value_create_long_constant(func, jit_type_long, 0);
		val.meta = ptrs_jit_const_meta(func, stmt->type);
		stmt->location.constType = stmt->type;
	}

	stmt->location.val = jit_value_create(func, jit_type_long);
	jit_insn_store(func, stmt->location.val, val.val);

	if(stmt->location.constType == PTRS_TYPE_NATIVE || stmt->location.constType == PTRS_TYPE_POINTER)
	{
		stmt->location.meta = jit_value_create(func, jit_type_long);
		jit_insn_store(func, stmt->location.meta, val.meta);
	}
	else
	{
		stmt->location.meta = ptrs_jit_const_meta(func, stmt->location.constType);
	}

	return stmt->location;
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
		size = ptrs_jit_vartoi(func, val);
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	ptrs_jit_assert(node, func, scope, jit_insn_le(func, size, jit_const_int(func, nuint, ptrs_arraymax)),
		1, "Cannot create array of size %d", size);

	//allocate memory
	if(stmt->onStack)
	{
		if(jit_value_is_constant(size))
			val.val = jit_insn_array(func, jit_value_get_nint_constant(size));
		else
			val.val = jit_insn_alloca(func, size);
	}
	else
	{
		jit_type_t params[] = {jit_type_nuint};
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, 1, 1);

		val.val = jit_insn_call_native(func, "malloc", malloc, signature, &size, 1, JIT_CALL_NOTHROW);
		jit_type_free(signature);
	}

	val.meta = ptrs_jit_arrayMeta(func,
		jit_const_long(func, ulong, PTRS_TYPE_NATIVE),
		jit_const_long(func, ulong, false),
		size
	);
	val.constType = PTRS_TYPE_NATIVE;

	//store the array
	stmt->location.val = jit_value_create(func, jit_type_long);
	stmt->location.meta = jit_value_create(func, jit_type_ulong);
	stmt->location.constType = PTRS_TYPE_NATIVE;
	jit_insn_store(func, stmt->location.val, val.val);
	jit_insn_store(func, stmt->location.meta, val.meta);

	if(stmt->isInitExpr)
	{
		ptrs_jit_var_t init = stmt->initExpr->handler(stmt->initExpr, func, scope);

		//check type of initExpr
		ptrs_jit_typeCheck(node, func, scope, init, PTRS_TYPE_NATIVE,
			1, "Array init expression must be of type native not %t", TYPECHECK_TYPE);

		//check initExpr.size <= array.size
		jit_value_t initSize = ptrs_jit_getArraySize(func, init.meta);
		ptrs_jit_assert(node, func, scope, jit_insn_le(func, initSize, size),
			2, "Init expression size of %d is too big for array of size %d", initSize, size);

		//copy initExpr memory to array and zero the rest
		jit_insn_memcpy(func, val.val, init.val, initSize);
		jit_insn_memset(func, jit_insn_add(func, val.val, initSize),
			jit_const_int(func, ubyte, 0), jit_insn_sub(func, size, initSize));
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
		size = ptrs_jit_vartoi(func, val);
		byteSize = jit_insn_mul(func, size, jit_const_int(func, nuint, sizeof(ptrs_var_t)));
	}
	else
	{
		//TODO
	}

	//make sure array is not too big
	ptrs_jit_assert(node, func, scope,
		jit_insn_le(func, byteSize, jit_const_int(func, nuint, ptrs_arraymax)),
		1, "Cannot create array of size %d", size);

	//allocate memory
	if(stmt->onStack)
	{
		if(jit_value_is_constant(byteSize))
			val.val = jit_insn_array(func, jit_value_get_nint_constant(byteSize));
		else
			val.val = jit_insn_alloca(func, byteSize);
	}
	else
	{
		jit_type_t params[] = {jit_type_nuint};
		jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_void_ptr, params, 1, 1);

		val.val = jit_insn_call_native(func, "malloc", malloc, signature, &byteSize, 1, JIT_CALL_NOTHROW);
		jit_type_free(signature);
	}

	val.meta = ptrs_jit_arrayMeta(func,
		jit_const_long(func, ulong, PTRS_TYPE_POINTER),
		jit_const_long(func, ulong, false),
		size
	);
	val.constType = PTRS_TYPE_POINTER;

	//store the array
	stmt->location.val = jit_value_create(func, jit_type_long);
	stmt->location.meta = jit_value_create(func, jit_type_ulong);
	stmt->location.constType = PTRS_TYPE_POINTER;
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
static char *resolveRelPath(ptrs_ast_t *node, const char *from)
{
	char *fullPath;
	if(from[0] != '/')
	{
		char dirbuff[strlen(node->file) + 1];
		strcpy(dirbuff, node->file);
		char *dir = dirname(dirbuff);

		char buff[strlen(dir) + strlen(from) + 2];
		sprintf(buff, "%s/%s", dir, from);

		fullPath = realpath(buff, NULL);
	}
	else
	{
		fullPath = realpath(from, NULL);
	}

	if(fullPath == NULL)
		ptrs_error(node, "Could not resolve path '%s'", from);

	return fullPath;
}
static void importScript(ptrs_ast_t *node, ptrs_var_t *values, char *from)
{
	//TODO
}
static void importNative(ptrs_ast_t *node, ptrs_var_t *values, char *from)
{
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		if(from[0] == '.' || from[0] == '/')
			from = resolveRelPath(node, from);

		handle = dlopen(from, RTLD_NOW);
		free(from);

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);
	}

	struct ptrs_importlist *curr = node->arg.import.imports;
	for(int i = 0; curr != NULL; i++)
	{
		values[i].value.nativeval = dlsym(handle, curr->name);
		values[i].meta.type = PTRS_TYPE_NATIVE;
		values[i].meta.array.size = 0;
		values[i].meta.array.readOnly = true;

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);

		curr = curr->next;
	}
}
void ptrs_import(ptrs_ast_t *node, ptrs_var_t *values, ptrs_val_t fromVal, ptrs_meta_t fromMeta)
{
	char *path = NULL;
	if(fromMeta.type != PTRS_TYPE_UNDEFINED)
	{
		if(fromMeta.type == PTRS_TYPE_NATIVE)
		{
			int len = strnlen(fromVal.strval, fromMeta.array.size);
			if(len < fromMeta.array.size)
			{
				path = (char *)fromVal.strval;
			}
			else
			{
				path = alloca(len) + 1;
				memcpy(path, fromVal.strval, len);
				path[len] = 0;
			}
		}
		else
		{
			path = alloca(32);
			ptrs_vartoa(fromVal, fromMeta, path, 32);
		}
	}

	char *ending;
	if(path == NULL)
	 	ending = NULL;
	else
		ending = strrchr(path, '.');

	if(ending != NULL && strcmp(ending, ".ptrs") == 0)
		importScript(node, values, path);
	else
		importNative(node, values, path);
}

ptrs_jit_var_t ptrs_handle_import(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_import *stmt = &node->arg.import;

	int len = 0;
	struct ptrs_importlist *curr;

	curr = stmt->imports;
	while(curr != NULL)
	{
		len++;
		curr = curr->next;
	}

	ptrs_jit_var_t from;
	if(stmt->from != NULL)
		from = stmt->from->handler(stmt->from, func, scope);

	if(stmt->from == NULL
		|| (jit_value_is_constant(from.val) && jit_value_is_constant(from.meta)))
	{
		ptrs_var_t constFrom;
		if(stmt->from == NULL)
		{
			constFrom.meta.type = PTRS_TYPE_UNDEFINED;
		}
		else
		{
			constFrom.value = ptrs_jit_value_getValConstant(from.val);
			constFrom.meta = ptrs_jit_value_getMetaConstant(from.meta);
		}

		ptrs_var_t *values = malloc(len * sizeof(ptrs_var_t));
		ptrs_import(node, values, constFrom.value, constFrom.meta);

		stmt->location = jit_const_int(func, void_ptr, (uintptr_t)values);
	}
	else
	{
		stmt->location = jit_insn_array(func, len * sizeof(ptrs_var_t));

		jit_value_t params[] = {
			jit_const_int(func, void_ptr, (uintptr_t)node),
			stmt->location,
			from.val,
			from.meta,
		};

		static jit_type_t importSignature = NULL;
		if(importSignature == NULL)
		{
			jit_type_t paramDef[] = {
				jit_type_void_ptr,
				jit_type_void_ptr,
				jit_type_long,
				jit_type_ulong,
			};
			importSignature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, paramDef, 4, 1);
		}

		jit_insn_call_native(func, "import", ptrs_import, importSignature, params, 4, 0);
	}

	return from;
}

ptrs_jit_var_t ptrs_handle_return(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t ret = node->handler(node, func, scope);

	jit_value_t val = ptrs_jit_varToVal(func, ret);
	jit_insn_return(func, val);

	return ret;
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
	ptrs_ast_t *ast = node->arg.astval;
	ptrs_jit_var_t val = ast->handler(ast, func, scope);

	jit_value_t type = ptrs_jit_getType(func, val.meta);
	ptrs_jit_assert(node, func, scope, jit_insn_ge(func, type, jit_const_long(func, ulong, PTRS_TYPE_NATIVE)),
		1, "Cannot delete value of type %t", type);

	//TODO structs

	static jit_type_t freeSignature = NULL;
	if(freeSignature == NULL)
	{
		jit_type_t arg = jit_type_void_ptr;
		freeSignature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, &arg, 1, 0);
	}

	jit_insn_call_native(func, "free", free, freeSignature, &val.val, 1, 0);
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

	jit_type_t argDef[funcAst->argc * 2 + 1];
	argDef[0] = jit_type_void_ptr; //reserved

	for(int i = 0; i < funcAst->argc; i++)
	{
		argDef[i * 2 + 1] = jit_type_long;
		argDef[i * 2 + 2] = jit_type_ulong;
	}

	//TODO variadic functions

	jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, ptrs_jit_getVarType(), argDef,
		funcAst->argc * 2 + 1, 1);
	jit_function_t self = jit_function_create_nested(ptrs_jit_context, signature, func);

	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_NAME, funcAst->name, NULL, 0);
	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_FILE, (char *)node->file, NULL, 0);
	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_AST, node, NULL, 0);
	jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_CLOSURE, NULL, NULL, 0);

	*ast->symbol = self;

	ptrs_scope_t selfScope;
	ptrs_initScope(&selfScope, scope);

	for(int i = 0; i < funcAst->argc; i++)
	{
		funcAst->args[i].val = jit_value_create(self, jit_type_long);
		funcAst->args[i].meta = jit_value_create(self, jit_type_ulong);
		funcAst->args[i].constType = -1;

		jit_insn_store(self, funcAst->args[i].val, jit_value_get_param(self, i * 2 + 1));
		jit_insn_store(self, funcAst->args[i].meta, jit_value_get_param(self, i * 2 + 2));

		if(funcAst->argv != NULL && funcAst->argv[i] != NULL)
		{
			jit_label_t given = jit_label_undefined;
			jit_insn_branch_if_not(self, ptrs_jit_hasType(self, funcAst->args[i].meta, PTRS_TYPE_UNDEFINED), &given);

			ptrs_jit_var_t val = funcAst->argv[i]->handler(funcAst->argv[i], self, &selfScope);
			jit_insn_store(self, funcAst->args[i].val, val.val);
			jit_insn_store(self, funcAst->args[i].meta, val.meta);

			jit_insn_label(self, &given);
		}
	}

	funcAst->body->handler(funcAst->body, self, &selfScope);

	jit_insn_default_return(self);

	ptrs_jit_placeAssertions(self, &selfScope);

	if(ptrs_compileAot && jit_function_compile(self) == 0)
		ptrs_error(node, "Failed compiling function %s", funcAst->name);



	ptrs_jit_var_t ret;
	if(ast->isAnonymous)
	{
		jit_function_t closure = ptrs_jit_createTrampoline(node, scope, funcAst, func);
		jit_function_set_meta(self, PTRS_JIT_FUNCTIONMETA_CLOSURE, closure, NULL, 0);

		if(ptrs_compileAot && jit_function_compile(closure) == 0)
			ptrs_error(node, "Failed compiling closure of function %s", funcAst->name);

		void *closurePtr = jit_function_to_closure(closure);
		ret.val = jit_const_int(func, void_ptr, (uintptr_t)closurePtr);
		ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_NATIVE);
		ret.constType = PTRS_TYPE_NATIVE;
	}
	else
	{
		ret.val = jit_const_long(func, long, 0);
		ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		ret.constType = PTRS_TYPE_UNDEFINED;
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
	ptrs_jit_branch_if_not(func, &isFalse, condition);

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
	struct ptrs_ast_switch *stmt = &node->arg.switchcase;
	struct ptrs_ast_case *curr = stmt->cases;
	jit_label_t done = jit_label_undefined;
	int64_t interval = stmt->max - stmt->min + 1;

	ptrs_jit_var_t condition = stmt->condition->handler(stmt->condition, func, scope);
	jit_value_t val = ptrs_jit_vartoi(func, condition);

	if(jit_value_is_constant(condition.val))
	{
		int64_t _val = jit_value_get_long_constant(condition.val);
		bool hadCase = false;

		while(curr != NULL)
		{
			if(curr->min > _val && _val < curr->max)
			{
				curr->body->handler(curr->body, func, scope);
				hadCase = true;
			}
			curr = curr->next;
		}

		if(!hadCase)
			stmt->defaultCase->handler(stmt->defaultCase, func, scope);
	}
	else if(stmt->caseCount > 2 && interval < 0x1000 && interval / stmt->caseCount < 50)
	{
		jit_label_t table[interval];
		for(int i = 0; i < interval; i++)
			table[i] = jit_label_undefined;

		bool hasCase[interval];
		memset(hasCase, 0, sizeof(bool) * interval);

		if(stmt->min != 0)
			val = jit_insn_sub(func, val, jit_const_int(func, long, stmt->min));
		jit_insn_jump_table(func, val, table, interval);

		while(curr != NULL)
		{
			ptrs_ast_t *body = curr->body;
			while(curr != NULL && curr->body == body)
			{
				for(int i = 0; i < curr->max - stmt->min; i++)
				{
					jit_insn_label(func, table + i);
					hasCase[i] = true;
				}
				curr = curr->next;
			}

			body->handler(body, func, scope);

			jit_insn_branch(func, &done);
		}

		if(stmt->defaultCase != NULL)
		{
			jit_label_t defaultCase = jit_label_undefined;
			jit_insn_label(func, &defaultCase);

			for(int i = 0; i < interval; i++)
			{
				if(!hasCase[i])
					jit_insn_label(func, table + i);
			}
			stmt->defaultCase->handler(stmt->defaultCase, func, scope);
			jit_insn_branch(func, &done);

			//let all values outside of the jump table jump to the default label
			jit_insn_branch(func, &defaultCase);
		}
	}
	else
	{
		jit_label_t cases[stmt->caseCount];
		for(int i = 0; i < stmt->caseCount; i++)
			cases[i] = jit_label_undefined;

		for(int i = 0; curr != NULL; i++)
		{
			jit_value_t caseCondition;
			if(curr->min == curr->max)
			{
				caseCondition = jit_insn_eq(func, val, jit_const_int(func, long, curr->min));
			}
			else
			{
				caseCondition = jit_insn_and(func,
					jit_insn_ge(func, val, jit_const_int(func, long, curr->min)),
					jit_insn_le(func, val, jit_const_int(func, long, curr->max))
				);
			}

			jit_insn_branch_if(func, caseCondition, cases + i);
			curr = curr->next;
		}

		if(stmt->defaultCase != NULL)
		{
			stmt->defaultCase->handler(stmt->defaultCase, func, scope);
			jit_insn_branch(func, &done);
		}

		curr = stmt->cases;
		int i = 0;
		while(curr != NULL)
		{
			ptrs_ast_t *body = curr->body;
			while(curr != NULL && curr->body == body)
			{
				jit_insn_label(func, cases + i);
				curr = curr->next;
				i++;
			}

			body->handler(body, func, scope);

			jit_insn_branch(func, &done);
		}
	}

	jit_insn_label(func, &done);
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
	ptrs_jit_branch_if_not(func, &end, val);

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
	ptrs_jit_branch_if(func, &start, condition);

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
	ptrs_jit_branch_if_not(func, &end, val);

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

static void forinArray(struct ptrs_ast_forin *stmt, jit_function_t func, ptrs_jit_var_t val,
	jit_function_t body, jit_type_t bodySignature, int totalArgCount,
	jit_value_t retAddr, jit_label_t *ret, jit_label_t *done, bool isNative)
{
	jit_label_t loop = jit_label_undefined;
	jit_value_t size = ptrs_jit_getArraySize(func, val.meta);
	jit_value_t index = jit_value_create(func, jit_type_nuint);
	jit_insn_store(func, index, jit_const_int(func, nuint, 0));

	jit_insn_label(func, &loop);
	jit_insn_branch_if_not(func, jit_insn_lt(func, index, size), done);

	jit_value_t args[totalArgCount];
	args[0] = retAddr;
	args[1] = index;
	args[2] = ptrs_jit_const_meta(func, PTRS_TYPE_INT);

	if(totalArgCount > 3)
	{
		if(isNative)
		{
			jit_value_t curr = jit_insn_load_elem(func, val.val, index, jit_type_ubyte);
			args[3] = curr;
			args[4] = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
		}
		else
		{
			jit_value_t actualIndex = jit_insn_shl(func, index, jit_const_int(func, ubyte, 1));
			args[3] = jit_insn_load_elem(func, val.val, actualIndex, jit_type_long);
			actualIndex = jit_insn_add(func, actualIndex, jit_const_int(func, nuint, 1));
			args[4] = jit_insn_load_elem(func, val.val, actualIndex, jit_type_ulong);
		}
	}

	for(int i = 6; i < totalArgCount; i++)
		args[i] = jit_const_long(func, ulong, 0);

	jit_value_t breaking = jit_insn_call(func, "(foreach body)",
		body, bodySignature, args, totalArgCount, 0);

	jit_insn_branch_if(func, jit_insn_eq(func, breaking, jit_const_int(func, ubyte, 2)), done); //break;
	jit_insn_branch_if(func, jit_insn_eq(func, breaking, jit_const_int(func, ubyte, 3)), ret); //return;

	jit_insn_store(func, index, jit_insn_add(func, index, jit_const_int(func, nuint, 1)));
	jit_insn_branch(func, &loop);
}
ptrs_jit_var_t ptrs_handle_forin(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_forin *stmt = &node->arg.forin;

	int totalArgCount = stmt->varcount * 2 + 1;
	jit_type_t argDef[totalArgCount];
	argDef[0] = jit_type_void_ptr; //reserved

	for(int i = 0; i < stmt->varcount; i++)
	{
		argDef[i * 2 + 1] = jit_type_long;
		argDef[i * 2 + 2] = jit_type_ulong;
	}

	//create the body function so we have something to call
	jit_type_t bodySignature = jit_type_create_signature(jit_abi_cdecl, jit_type_ubyte, argDef, totalArgCount, 0);
	jit_function_t body = jit_function_create_nested(ptrs_jit_context, bodySignature, func);

	jit_function_set_meta(body, PTRS_JIT_FUNCTIONMETA_NAME, "(foreach body)", NULL, 0);
	jit_function_set_meta(body, PTRS_JIT_FUNCTIONMETA_FILE, (char *)node->file, NULL, 0);
	jit_function_set_meta(body, PTRS_JIT_FUNCTIONMETA_AST, node, NULL, 0);
	jit_function_set_meta(body, PTRS_JIT_FUNCTIONMETA_CLOSURE, NULL, NULL, 0);



	//set up the iteration process
	ptrs_jit_var_t val = stmt->value->handler(stmt->value, func, scope);

	jit_label_t returnVal = jit_label_undefined;
	jit_label_t done = jit_label_undefined;
	jit_value_t ret = jit_value_create(func, ptrs_jit_getVarType());
	jit_value_set_addressable(ret);
	jit_value_t retAddr = jit_insn_address_of(func, ret);

	ptrs_jit_typeSwitch(node, func, scope, val,
		(1, "Cannot iterate over value of type %t", val.constType),
		(PTRS_TYPE_NATIVE, PTRS_TYPE_POINTER),

		case PTRS_TYPE_NATIVE:
			stmt->varsymbols[0].constType = PTRS_TYPE_INT;
			stmt->varsymbols[1].constType = PTRS_TYPE_INT;

			forinArray(stmt, func, val,
				body, bodySignature, totalArgCount,
				retAddr, &returnVal, &done, true);
			break;

		case PTRS_TYPE_POINTER:
			stmt->varsymbols[0].constType = PTRS_TYPE_INT;

			forinArray(stmt, func, val,
				body, bodySignature, totalArgCount,
				retAddr, &returnVal, &done, false);
			break;
	);

	jit_insn_label(func, &returnVal);
	jit_insn_return_ptr(func, retAddr, ptrs_jit_getVarType());

	jit_insn_label(func, &done);



	//handle the body function
	for(int i = 0; i < stmt->varcount; i++)
	{
		stmt->varsymbols[i].val = jit_value_create(body, jit_type_long);
		stmt->varsymbols[i].meta = jit_value_create(body, jit_type_ulong);
		stmt->varsymbols[i].constType = -1;

		jit_insn_store(body, stmt->varsymbols[i].val, jit_value_get_param(body, i * 2 + 1));
		jit_insn_store(body, stmt->varsymbols[i].meta, jit_value_get_param(body, i * 2 + 2));
	}

	ptrs_scope_t bodyScope;
	ptrs_initScope(&bodyScope, scope);

	stmt->body->handler(stmt->body, body, &bodyScope);

	jit_insn_return(body, jit_const_int(body, ubyte, 0));

	ptrs_jit_placeAssertions(body, &bodyScope);

	if(ptrs_compileAot && jit_function_compile(body) == 0)
		ptrs_error(node, "Failed compiling foreach body");

	jit_type_free(bodySignature);
	return val;
}

ptrs_jit_var_t ptrs_handle_scopestatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.astval;

	ptrs_jit_var_t val = body->handler(body, func, scope);

	jit_insn_default_return(func);
	ptrs_jit_placeAssertions(func, scope);

	return val;
}

ptrs_jit_var_t ptrs_handle_exprstatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		return expr->handler(expr, func, scope);
}
