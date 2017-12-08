#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <jit/jit.h>

#include "jit.h"
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

	val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);

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

		if(val.constType == -1 && stmt->type >= PTRS_NUM_TYPES)
			ptrs_error(node, "Initializer of untyped let statement has a dynamic type");
		else if(stmt->type >= PTRS_NUM_TYPES)
			stmt->location.constType = val.constType;
		else if(val.constType == -1)
			stmt->location.constType = stmt->type;
		else if(val.constType != stmt->type)
			ptrs_error(node, "Variable is defined as %t but initializer has type %t",
				stmt->type, val.constType);
		else
			stmt->location.constType = val.constType;
	}
	else
	{
		val.val = jit_value_create_long_constant(func, jit_type_long, 0);
		val.meta = ptrs_jit_const_meta(func, stmt->type);
		stmt->location.constType = stmt->type;
	}

	if(stmt->location.constType == PTRS_TYPE_FLOAT)
	{
		stmt->location.val = jit_value_create(func, jit_type_float64);
		val.val = jit_insn_convert(func, val.val, jit_type_float64, 0);
	}
	else
	{
		stmt->location.val = jit_value_create(func, jit_type_long);
		val.val = jit_insn_convert(func, val.val, jit_type_long, 0);
	}
	jit_insn_store(func, stmt->location.val, val.val);

	if(stmt->location.constType == PTRS_TYPE_STRUCT && jit_value_is_constant(val.meta))
	{
		stmt->location.meta = val.meta;
	}
	else if(stmt->location.constType == PTRS_TYPE_INT
		|| stmt->location.constType == PTRS_TYPE_FLOAT)
	{
		stmt->location.meta = ptrs_jit_const_meta(func, stmt->location.constType);
	}
	else
	{
		stmt->location.meta = jit_value_create(func, jit_type_long);
		jit_insn_store(func, stmt->location.meta, val.meta);
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
	val.val = ptrs_jit_allocate(func, size, stmt->onStack, true);
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
	val.val = ptrs_jit_allocate(func, byteSize, stmt->onStack, true);
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

typedef struct ptrs_cache
{
	const char *path;
	ptrs_ast_t *ast;
	ptrs_symboltable_t *symbols;
	struct ptrs_cache *next;
} ptrs_cache_t;
ptrs_cache_t *ptrs_cache = NULL;

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
static void importScript(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope,
	ptrs_ast_t **expressions, char *from)
{
	from = resolveRelPath(node, from);

	ptrs_cache_t *cache = ptrs_cache;
	while(cache != NULL)
	{
		if(strcmp(cache->path, from) == 0)
		{
			free(from);
			break;
		}
		cache = cache->next;
	}

	if(cache == NULL)
	{
		char *src = ptrs_readFile(from);

		cache = malloc(sizeof(ptrs_cache_t));
		cache->symbols = NULL;
		cache->ast = ptrs_parse(src, from, &cache->symbols);

		cache->ast->handler(cache->ast, func, scope);

		cache->next = ptrs_cache;
		ptrs_cache = cache;
	}

	struct ptrs_importlist *curr = node->arg.import.imports;
	for(int i = 0; curr != NULL; i++)
	{
		if(ptrs_ast_getSymbol(cache->symbols, curr->name, expressions + i) != 0)
			ptrs_error(node, "Script '%s' has no property '%s'", from, curr->name);

		curr = curr->next;
	}
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
	char *path;
	if(stmt->from == NULL)
	{
		from.val = jit_const_long(func, long, 0);
		from.meta = jit_const_long(func, ulong, 0);
		path = NULL;
	}
	else
	{
		from = stmt->from->handler(stmt->from, func, scope);

		if(!jit_value_is_constant(from.val) || !jit_value_is_constant(from.meta))
			ptrs_error(node, "Dynamic imports are not supported");

		ptrs_val_t val = ptrs_jit_value_getValConstant(from.val);
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(from.meta);

		if(meta.type == PTRS_TYPE_NATIVE)
		{
			int len = strnlen(val.strval, meta.array.size);
			if(len < meta.array.size)
			{
				path = (char *)val.strval;
			}
			else
			{
				path = alloca(len) + 1;
				memcpy(path, val.strval, len);
				path[len] = 0;
			}
		}
		else
		{
			path = alloca(32);
			ptrs_vartoa(val, meta, path, 32);
		}
	}

	char *ending = NULL;
	if(path != NULL)
		ending = strrchr(path, '.');

	if(ending != NULL && strcmp(ending, ".ptrs") == 0)
	{
		stmt->isScriptImport = true;
		stmt->expressions = malloc(len * sizeof(ptrs_ast_t *));
		importScript(node, func, scope, stmt->expressions, path);
	}
	else
	{
		stmt->isScriptImport = false;
		stmt->symbols = malloc(len * sizeof(ptrs_var_t));
		importNative(node, stmt->symbols, path);
	}

	return from;
}

ptrs_jit_var_t ptrs_handle_return(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	node = node->arg.astval;
	ptrs_jit_var_t ret = node->handler(node, func, scope);

	jit_insn_return_struct_from_values(func, ret.val, ret.meta);
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

	ptrs_jit_reusableCallVoid(func, free, (jit_type_void_ptr), (val.val));
}

ptrs_jit_var_t ptrs_handle_throw(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_trycatch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	//TODO
}

ptrs_jit_var_t ptrs_handle_function(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_function *ast = &node->arg.function;

	jit_function_t self = ptrs_jit_compileFunction(node, func, scope, &ast->func, NULL);
	*ast->symbol = self;

	ptrs_jit_var_t ret;
	ret.val = jit_const_long(func, long, (uintptr_t)jit_function_to_closure(self));
	ret.meta = ptrs_jit_pointerMeta(func,
		jit_const_long(func, ulong, PTRS_TYPE_FUNCTION),
		jit_insn_get_frame_pointer(func)
	);
	ret.constType = PTRS_TYPE_FUNCTION;

	return ret;
}

ptrs_jit_var_t ptrs_handle_struct(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_struct_t *struc = &node->arg.structval;
	struc->ast = node;

	jit_value_t staticData = NULL;
	if(struc->staticData != NULL)
		staticData = jit_const_int(func, void_ptr, (uintptr_t)struc->staticData);

	jit_insn_store_relative(func, jit_const_int(func, void_ptr, (uintptr_t)struc),
		offsetof(ptrs_struct_t, parentFrame), jit_insn_get_frame_pointer(func));

	int len = snprintf(NULL, 0, "%s.(data initializer)", struc->name);
	char *ctorName = malloc(len);
	sprintf(ctorName, "%s.(data initializer)", struc->name);

	ptrs_jit_reusableSignature(func, ctorSignature, ptrs_jit_getVarType(), (jit_type_void_ptr));
	jit_function_t ctor = ptrs_jit_createFunction(node, func, ctorSignature, ctorName);
	jit_value_t ctorData = jit_value_get_param(ctor, 0);
	ptrs_scope_t ctorScope;
	ptrs_initScope(&ctorScope, scope);

	for(int i = 0; i < struc->memberCount; i++)
	{
		struct ptrs_structmember *curr = &struc->member[i];
		if(curr->name == NULL) //hashmap filler entry
			continue;

		jit_function_t currFunc;
		jit_value_t currData;
		ptrs_scope_t *currScope;
		if(curr->isStatic)
		{
			currFunc = func;
			currData = staticData;
			currScope = scope;
		}
		else
		{
			currFunc = ctor;
			currData = ctorData;
			currScope = &ctorScope;
		}

		if(curr->type == PTRS_STRUCTMEMBER_FUNCTION
			|| curr->type == PTRS_STRUCTMEMBER_GETTER
			|| curr->type == PTRS_STRUCTMEMBER_SETTER)
		{
			curr->value.function.func = ptrs_jit_compileFunction(node, func,
				scope, curr->value.function.ast, struc);
		}
		else if(curr->type == PTRS_STRUCTMEMBER_VAR)
		{
			ptrs_ast_t *ast = curr->value.startval;
			if(ast != NULL)
			{
				ptrs_jit_var_t startVal = ast->handler(ast, currFunc, currScope);
				jit_value_t addr = jit_insn_add_relative(currFunc, currData, curr->offset);
				jit_insn_store_relative(currFunc, addr, 0, startVal.val);
				jit_insn_store_relative(currFunc, addr, sizeof(ptrs_val_t), startVal.meta);
			}
		}
		else if(curr->type == PTRS_STRUCTMEMBER_ARRAY)
		{
			ptrs_astlist_handleByte(curr->value.array.init, currFunc, currScope,
				jit_insn_add(currFunc, currData, jit_const_int(currFunc, nuint, curr->offset)),
				jit_const_int(currFunc, nuint, curr->value.array.size)
			);
		}
		else if(curr->type == PTRS_STRUCTMEMBER_VARARRAY)
		{
			ptrs_astlist_handle(curr->value.array.init, currFunc, currScope,
				jit_insn_add(currFunc, currData, jit_const_int(currFunc, nuint, curr->offset)),
				jit_const_int(currFunc, nuint, curr->value.array.size / sizeof(ptrs_var_t))
			);
		}
	}

	jit_insn_default_return(ctor);
	ptrs_jit_placeAssertions(ctor, &ctorScope);

	if(ptrs_compileAot && jit_function_compile(ctor) == 0)
		ptrs_error(node, "Failed compiling the constructor of function %s", struc->name);

	bool hasCtor = false;
	struct ptrs_opoverload *curr = struc->overloads;
	while(curr != NULL)
	{
		//TODO include non static member initializers in the ptrs_handle_new overload
		curr->handlerFunc = ptrs_jit_compileFunction(node, func,
			scope, curr->handler, struc);

		if(curr->op == (void *)ptrs_handle_new)
		{
			jit_label_t initStart = jit_label_undefined;
			jit_insn_label(curr->handlerFunc, &initStart);

			jit_value_t thisPtr = jit_value_get_param(curr->handlerFunc, 0);
			jit_insn_call(curr->handlerFunc, ctorName, ctor, NULL, &thisPtr, 1, 0);

			jit_label_t initEnd = jit_label_undefined;
			jit_insn_label(curr->handlerFunc, &initEnd);
			jit_insn_move_blocks_to_start(curr->handlerFunc, initStart, initEnd);

			const char *name = jit_function_get_meta(curr->handlerFunc, PTRS_JIT_FUNCTIONMETA_NAME);
			if(ptrs_compileAot && jit_function_compile(curr->handlerFunc) == 0)
				ptrs_error(node, "Failed compiling function %s", name);

			hasCtor = true;
		}

		curr = curr->next;
	}

	if(!hasCtor)
	{
		struct ptrs_opoverload *ctorOverload = malloc(sizeof(struct ptrs_opoverload));
		ctorOverload->op = ptrs_handle_new;
		ctorOverload->isStatic = true;
		ctorOverload->handler = NULL;
		ctorOverload->handlerFunc = ctor;

		ctorOverload->next = struc->overloads;
		struc->overloads = ctorOverload;
	}

	struc->location->val = jit_const_long(func, long, 0);
	struc->location->meta = ptrs_jit_const_pointerMeta(func, PTRS_TYPE_STRUCT, struc);
	struc->location->constType = PTRS_TYPE_STRUCT;

	return *struc->location;
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
	jit_function_t body = ptrs_jit_createFunction(node, func, bodySignature, "(foreach body)");



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
	static jit_type_t bodySignature = NULL;
	if(bodySignature == NULL)
		bodySignature = jit_type_create_signature(jit_abi_cdecl, jit_type_void, NULL, 0, 0);

	ptrs_ast_t *body = node->arg.astval;
	jit_function_t bodyFunc = jit_function_create_nested(ptrs_jit_context, bodySignature, func);
	ptrs_scope_t bodyScope;
	ptrs_initScope(&bodyScope, scope);

	ptrs_jit_var_t val = body->handler(body, bodyFunc, &bodyScope);

	jit_value_t ret = jit_insn_call(func, "(scoped body)", bodyFunc, bodySignature, NULL, 0, 0);
	//TODO handle ret

	jit_insn_default_return(bodyFunc);
	ptrs_jit_placeAssertions(bodyFunc, scope);

	if(ptrs_compileAot && jit_function_compile(bodyFunc) == 0)
		ptrs_error(node, "Failed compiling the scoped statement body");

	val.val = ret;
	val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	val.constType = PTRS_TYPE_UNDEFINED;
	return val;
}

ptrs_jit_var_t ptrs_handle_exprstatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		return expr->handler(expr, func, scope);
}
