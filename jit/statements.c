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

ptrs_jit_var_t ptrs_handle_initroot(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_init_root *stmt = &node->arg.initroot;

	// initialize try/catch for the root function
	if(stmt->hasTryCatch)
		jit_insn_uses_catcher(func);

	// initialize the arguments array
	stmt->argumentsLocation.val = jit_value_get_param(func, 0);
	stmt->argumentsLocation.meta = jit_value_get_param(func, 1);
	stmt->argumentsLocation.constType = PTRS_TYPE_POINTER;
	stmt->argumentsLocation.addressable = false;

	// initialize the root scope
	jit_insn_store_relative(func,
		jit_const_int(func, void_ptr, (uintptr_t)scope->rootFrame), 0,
		jit_insn_get_frame_pointer(func)
	);
}

ptrs_jit_var_t ptrs_handle_body(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_astlist *list = node->arg.astlist;
	ptrs_jit_var_t result;

	while(list != NULL)
	{
		ptrs_lastAst = list->entry;
		jit_insn_mark_offset(func, list->entry->codepos);

		result = list->entry->vtable->get(list->entry, func, scope);
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
		val = stmt->value->vtable->get(stmt->value, func, scope);
	}
	else
	{
		val.val = jit_value_create_long_constant(func, jit_type_long, 0);
		val.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	}

	if(stmt->location.addressable)
	{
		stmt->location.val = ptrs_jit_varToVal(func, val);
		stmt->location.meta = NULL;

		return val;
	}

	//stmt->type is set by flow.c to -1 if the type is dynamic or changes
	// or a constant when it's the same over the whole lifetime
	stmt->location.constType = stmt->type;

	if(stmt->type == PTRS_TYPE_FLOAT)
	{
		stmt->location.val = jit_value_create(func, jit_type_float64);
		val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_float64);
	}
	else
	{
		stmt->location.val = jit_value_create(func, jit_type_long);
		val.val = ptrs_jit_reinterpretCast(func, val.val, jit_type_long);
	}
	jit_insn_store(func, stmt->location.val, val.val);

	if(stmt->type == PTRS_TYPE_UNDEFINED
		|| stmt->type == PTRS_TYPE_INT
		|| stmt->type == PTRS_TYPE_FLOAT)
	{
		stmt->location.meta = ptrs_jit_const_meta(func, stmt->type);
	}
	else
	{
		stmt->location.meta = jit_value_create(func, jit_type_ulong);
		jit_insn_store(func, stmt->location.meta, val.meta);
	}

	return stmt->location;
}

size_t ptrs_arraymax = UINT32_MAX;
ptrs_jit_var_t ptrs_handle_array(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_definearray *stmt = &node->arg.definearray;
	jit_value_t size;

	if(stmt->length != NULL)
	{
		ptrs_jit_var_t _size = stmt->length->vtable->get(stmt->length, func, scope);
		ptrs_jit_typeCheck(node, func, scope, _size, PTRS_TYPE_INT, "Array size needs to be of type int not %t");
		size = _size.val;
	}
	else
	{
		size = jit_const_long(func, ulong, stmt->meta.array.size);
	}

	//make sure array is not too big
	ptrs_jit_assert(node, func, scope, jit_insn_le(func, size, jit_const_int(func, nuint, ptrs_arraymax)),
		1, "Cannot create array of size %d", size);

	ptrs_nativetype_info_t *type = ptrs_getNativeTypeForArray(node, stmt->meta);
	jit_value_t byteSize = jit_insn_mul(func, size, jit_const_long(func, ulong, type->size));

	//allocate memory
	ptrs_jit_var_t val = {0};
	val.val = ptrs_jit_allocate(func, byteSize, stmt->onStack, true);
	val.meta = ptrs_jit_arrayMetaKnownType(func, size, stmt->meta.array.typeIndex);
	val.constType = PTRS_TYPE_POINTER;

	if(stmt->onStack)
	{
		//store the array
		if(stmt->location.addressable)
		{
			stmt->location.val = ptrs_jit_varToVal(func, val);
			stmt->location.meta = NULL;
			stmt->location.constType = PTRS_TYPE_POINTER;
		}
		else
		{
			stmt->location.val = jit_value_create(func, jit_type_long);
			stmt->location.meta = jit_value_create(func, jit_type_ulong);
			stmt->location.constType = PTRS_TYPE_POINTER;
			jit_insn_store(func, stmt->location.val, val.val);
			jit_insn_store(func, stmt->location.meta, val.meta);
		}
	}

	ptrs_astlist_handle(node, stmt->initVal, func, scope, val.val, size, type);
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
	ptrs_ast_t **expressions, const char *from)
{
	from = resolveRelPath(node, from);

	ptrs_cache_t *cache = ptrs_cache;
	while(cache != NULL)
	{
		if(strcmp(cache->path, from) == 0)
			break;
		cache = cache->next;
	}

	if(cache == NULL)
	{
		char *src = ptrs_readFile(from);

		cache = malloc(sizeof(ptrs_cache_t));
		cache->path = from;
		cache->symbols = NULL;
		cache->ast = ptrs_parse(src, from, &cache->symbols, false);

		cache->ast->vtable->get(cache->ast, func, scope);

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
static void importNative(ptrs_ast_t *node, void **values, const char *from)
{
	const char *error;

	dlerror();

	void *handle = NULL;
	if(from != NULL)
	{
		if(from[0] == '.' || from[0] == '/')
			from = resolveRelPath(node, from);

		handle = dlopen(from, RTLD_NOW);

		error = dlerror();
		if(error != NULL)
			ptrs_error(node, error);
	}

	struct ptrs_importlist *curr = node->arg.import.imports;
	for(int i = 0; curr != NULL; i++)
	{
		values[i] = dlsym(handle, curr->name);

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

	if(stmt->isScriptImport)
	{
		stmt->expressions = calloc(len, sizeof(ptrs_ast_t *));
		importScript(node, func, scope, stmt->expressions, stmt->from);
	}
	else
	{
		stmt->symbols = calloc(len, sizeof(void *));
		importNative(node, stmt->symbols, stmt->from);
	}

	ptrs_jit_var_t ret;
	ret.val = jit_const_long(func, long, 0);
	ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
	ret.constType = PTRS_TYPE_UNDEFINED;
	return ret;
}

ptrs_jit_var_t ptrs_handle_return(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *value = node->arg.astval;

	ptrs_jit_var_t ret;
	if(value == NULL)
	{
		ret.val = jit_const_int(func, long, 0);
		ret.meta = ptrs_jit_const_meta(func, PTRS_TYPE_UNDEFINED);
		ret.constType = PTRS_TYPE_UNDEFINED;
	}
	else
	{
		ret = value->vtable->get(value, func, scope);
	}

	const char *funcName;
	ptrs_function_t *funcAst = jit_function_get_meta(func, PTRS_JIT_FUNCTIONMETA_FUNCAST);
	if(funcAst != NULL)
		funcName = funcAst->name;
	else if(scope->rootFunc == func)
		funcName = "(root)";
	else
		funcName = "(unknown)";

	if(scope->returnType.type == PTRS_TYPE_UNDEFINED)
	{
		if(value != NULL)
		{
			ptrs_error(node, "Function %s defined to not return a value, but a value was returned",
				funcName);
		}

		jit_insn_default_return(func);
	}
	else if(scope->returnType.type == (uint8_t)-1)
	{
		jit_insn_return_struct_from_values(func, ret.val, ret.meta);
	}
	else
	{
		if(value == NULL)
		{
			ptrs_error(node, "Function %s defines a return type %m, but no value was returned",
				funcName, scope->returnType);
		}

		jit_value_t retMetaJit = jit_const_long(func, ulong, *(uint64_t *)&scope->returnType);
		jit_value_t fakeCondition = jit_const_int(func, ubyte, 1);
		struct ptrs_assertion *assertion = ptrs_jit_assert(value, func, scope, fakeCondition,
			3, "Function %s defines a return type %m, but a value of type %m was returned",
			jit_const_int(func, void_ptr, (uintptr_t)funcName), retMetaJit, ret.meta);

		ptrs_jit_assertMetaCompatibility(func, assertion, scope->returnType, ret.meta, NULL);

		if(scope->returnType.type == PTRS_TYPE_INT
			|| scope->returnType.type == PTRS_TYPE_FLOAT
			|| scope->returnType.type == PTRS_TYPE_STRUCT)
		{
			jit_insn_return(func, ret.val);
		}
		else
		{
			jit_insn_return_struct_from_values(func, ret.val, ret.meta);
		}
	}

	return ret;
}

ptrs_jit_var_t ptrs_handle_break(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	if(!scope->loopControlAllowed)
		ptrs_error(node, "break; statement is not allowed here");

	if(scope->returnForLoopControl)
		jit_insn_return(func, jit_const_int(func, ubyte, 2));
	else
		jit_insn_branch(func, &scope->breakLabel);

	ptrs_jit_var_t ret = {NULL, NULL, -1};
	return ret;
}

ptrs_jit_var_t ptrs_handle_continue(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	if(!scope->loopControlAllowed)
		ptrs_error(node, "continue; statement is not allowed here");

	if(scope->returnForLoopControl)
		jit_insn_return(func, jit_const_int(func, ubyte, 1));
	else
		jit_insn_branch(func, &scope->continueLabel);

	ptrs_jit_var_t ret = {NULL, NULL, -1};
	return ret;
}

ptrs_jit_var_t ptrs_handle_continue_label(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	if(!scope->loopControlAllowed)
		ptrs_error(node, "continue_label statement found but not allowed, "
			"this is probably a bug in PointerScript");

	if(scope->returnForLoopControl)
		ptrs_error(node, "continue_label statement found inside nested function, "
			"this is probably a bug in PointerScript");

	jit_insn_label(func, &scope->continueLabel);
	scope->hasCustomContinueLabel = true;

	ptrs_jit_var_t ret = {NULL, NULL, -1};
	return ret;
}

void ptrs_delete(ptrs_ast_t *node, ptrs_val_t val, ptrs_meta_t meta)
{
	if(meta.type == PTRS_TYPE_STRUCT)
	{
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		if(val.structval == NULL)
			ptrs_error(node, "Cannot delete constructor of struct %s", struc->name);

		jit_function_t dtor = ptrs_struct_getOverload(struc, ptrs_handle_delete, true);
		if(dtor != NULL)
		{
			ptrs_var_t result;
			ptrs_jit_applyNested(dtor, &result, struc->parentFrame, val.structval, ());
		}
	}
	else if(meta.type != PTRS_TYPE_POINTER)
	{
		ptrs_error(node, "Cannot delete value of type %m", meta);
	}

	free(val.ptrval);
}
ptrs_jit_var_t ptrs_handle_delete(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *ast = node->arg.astval;
	ptrs_jit_var_t val = ast->vtable->get(ast, func, scope);

	if(val.constType == PTRS_TYPE_POINTER)
	{
		ptrs_jit_reusableCallVoid(func, free,
			(jit_type_void_ptr),
			(val.val)
		);
	}
	else if(val.constType == PTRS_TYPE_STRUCT && jit_value_is_constant(val.meta))
	{
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(val.meta);
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		ptrs_jit_assert(node, func, scope,
			jit_insn_ne(func, val.val, jit_const_long(func, long, 0)),
			1, "Cannot delete constructor of struct %s", jit_const_int(func, void_ptr, (uintptr_t)struc->name)
		);

		jit_function_t dtor = ptrs_struct_getOverload(struc, ptrs_handle_delete, true);
		if(dtor != NULL)
			ptrs_jit_callnested(node, func, scope, val.val, dtor, NULL);

		ptrs_jit_reusableCallVoid(func, free,
			(jit_type_void_ptr),
			(val.val)
		);
	}
	else if(val.constType == PTRS_TYPE_STRUCT || val.constType == -1)
	{
		jit_value_t astval = jit_const_int(func, void_ptr, (uintptr_t)node);
		ptrs_jit_reusableCallVoid(func, ptrs_delete,
			(jit_type_void_ptr, jit_type_void_ptr, jit_type_void_ptr),
			(astval, val.val, val.meta)
		);
	}
	else
	{
		ptrs_error(node, "Cannot free value of type %t", val.constType);
	}
}

ptrs_jit_var_t ptrs_handle_throw(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *ast = node->arg.astval;
	ptrs_jit_var_t val = ast->vtable->get(ast, func, scope);
	val = ptrs_jit_vartoa(func, val);

	jit_value_t nodeVal = jit_const_int(func, void_ptr, (uintptr_t)node);
	jit_value_t errorVal;
	ptrs_jit_reusableCall(func, ptrs_createError, errorVal, jit_type_void_ptr,
		(jit_type_void_ptr, jit_type_int, jit_type_void_ptr, jit_type_sys_bool),
		(nodeVal, jit_const_int(func, int, 2), val.val, jit_const_int(func, sys_bool, 1))
	);

	jit_insn_throw(func, errorVal);
}

ptrs_jit_var_t ptrs_handle_trycatch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_trycatch *ast = &node->arg.trycatch;

	ptrs_catcher_labels_t *catcher = ptrs_jit_addCatcher(scope);
	jit_label_t beforeFinally = jit_label_undefined;
	jit_value_t hadException = jit_value_create(func, jit_type_int);

	jit_insn_label(func, &catcher->beforeTry);
	ptrs_jit_var_t val = ast->tryBody->vtable->get(ast->tryBody, func, scope);
	if(ast->finallyBody != NULL && ast->catchBody == NULL)
		jit_insn_store(func, hadException, jit_const_int(func, int, 0));
	jit_insn_branch(func, &beforeFinally);
	jit_insn_label(func, &catcher->afterTry);

	jit_insn_label(func, &catcher->catcher);
	if(ast->catchBody)
	{
		ptrs_funcparameter_t *curr = ast->args;
		for(int i = 0; curr != NULL; i++)
		{
			if(curr->name == NULL) // argument ignored with _
			{
				curr = curr->next;
				continue;
			}

			if(scope->tryCatchException == NULL)
				scope->tryCatchException = jit_value_create(func, jit_type_void_ptr);

			jit_value_t tmp;
			switch(i)
			{
				case 0: // message
					curr->arg.val = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, message), jit_type_void_ptr);
					tmp = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, messageLen), jit_type_int);
					break;

				case 1: // traceback
					curr->arg.val = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, backtrace), jit_type_void_ptr);
					tmp = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, backtraceLen), jit_type_int);
					break;

				case 2: // file
					curr->arg.val = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, file), jit_type_void_ptr);
					tmp = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, fileLen), jit_type_int);
					break;

				case 3: // line
					tmp = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, pos) + offsetof(ptrs_codepos_t, line), jit_type_int);
					curr->arg.val = jit_insn_convert(func, tmp, jit_type_long, 0);
					break;

				case 4: // column
					tmp = jit_insn_load_relative(func, scope->tryCatchException,
						offsetof(ptrs_error_t, pos) + offsetof(ptrs_codepos_t, column), jit_type_int);
					curr->arg.val = jit_insn_convert(func, tmp, jit_type_long, 0);
					break;

				default:
					ptrs_error(node, "Catch statements can receive a maximum of 5 parameters");
			}

			if(i < 3)
			{
				curr->arg.meta = ptrs_jit_arrayMetaKnownType(func, tmp, PTRS_NATIVETYPE_INDEX_CHAR);
				curr->arg.constType = PTRS_TYPE_POINTER;
			}
			else
			{
				curr->arg.meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
				curr->arg.constType = PTRS_TYPE_INT;
			}

			curr = curr->next;
		}

		ast->catchBody->vtable->get(ast->catchBody, func, scope);
	}
	if(ast->finallyBody != NULL && ast->catchBody == NULL)
		jit_insn_store(func, hadException, jit_const_int(func, int, 1));

	jit_insn_label(func, &beforeFinally);
	if(ast->finallyBody)
	{
		ast->finallyBody->vtable->get(ast->finallyBody, func, scope);

		if(ast->catchBody == NULL)
		{
			//hadException = jit_insn_to_bool(func, hadException);
			jit_insn_branch_if(func, hadException, &scope->rethrowLabel);
		}
	}

	return val;
}

ptrs_jit_var_t ptrs_handle_function(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_function *ast = &node->arg.function;

	ast->symbol = ptrs_jit_createFunctionFromAst(node, func, &ast->func);

	ptrs_jit_buildFunction(node, ast->symbol, scope, &ast->func, NULL);

	ptrs_jit_var_t ret;
	if(ast->isExpression)
	{
		ret.val = jit_const_long(func, long, (uintptr_t)ptrs_jit_function_to_closure(node, ast->symbol));
		ret.meta = ptrs_jit_pointerMeta(func,
			jit_const_long(func, ulong, PTRS_TYPE_FUNCTION),
			jit_insn_get_frame_pointer(func)
		);
		ret.constType = PTRS_TYPE_FUNCTION;
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
	ptrs_struct_t *struc = &node->arg.structval;

	jit_value_t staticData = NULL;
	if(struc->staticData != NULL)
		staticData = jit_const_int(func, void_ptr, (uintptr_t)struc->staticData);

	jit_insn_store_relative(func, jit_const_int(func, void_ptr, (uintptr_t)struc),
		offsetof(ptrs_struct_t, parentFrame), jit_insn_get_frame_pointer(func));

	ptrs_jit_var_t vresult;
	ptrs_jit_var_t *result;
	if(struc->location == NULL)
		result = &vresult;
	else
		result = struc->location;

	result->val = jit_const_long(func, long, 0);
	result->meta = ptrs_jit_const_pointerMeta(func, PTRS_TYPE_STRUCT, struc);
	result->constType = PTRS_TYPE_STRUCT;

	bool hasCtorOverload = false;
	jit_function_t ctor = NULL;
	jit_value_t ctorData;
	ptrs_scope_t ctorScope;

	for(struct ptrs_opoverload *curr = struc->overloads; curr != NULL; curr = curr->next)
	{
		curr->handlerFunc = ptrs_jit_createFunctionFromAst(node, func, curr->handler);

		if(curr->op == (void *)ptrs_handle_new)
		{
			ctor = curr->handlerFunc;
			ctorData = jit_value_get_param(ctor, 0);
			ptrs_initScope(&ctorScope, scope);
			ctorScope.returnType.type = -1;

			hasCtorOverload = true;
		}
	}

	for(int i = 0; i < struc->memberCount; i++)
	{
		struct ptrs_structmember *curr = &struc->member[i];
		if(curr->name == NULL) //hashmap filler entry
			continue;
		else if(curr->type == PTRS_STRUCTMEMBER_VAR && curr->value.startval == NULL)
			continue;
		else if(curr->type == PTRS_STRUCTMEMBER_ARRAY)
			continue; // TODO allow initializers of arrays

		jit_function_t currFunc;
		jit_value_t currData;
		ptrs_scope_t *currScope;
		if(curr->isStatic)
		{
			currFunc = func;
			currData = staticData;
			currScope = scope;
		}
		else if(curr->type == PTRS_STRUCTMEMBER_VAR
			|| curr->type == PTRS_STRUCTMEMBER_ARRAY)
		{
			if(ctor == NULL)
			{
				char *ctorName = malloc(strlen(struc->name) + strlen(".(data initializer)") + 1);
				sprintf(ctorName, "%s.(data initializer)", struc->name);

				ptrs_jit_reusableSignature(func, ctorSignature, ptrs_jit_getVarType(), (jit_type_void_ptr));
				ctor = ptrs_jit_createFunction(node, func, ctorSignature, ctorName);

				ptrs_function_t *func = calloc(1, sizeof(ptrs_function_t));
				func->name = ctorName;
				func->retType.meta.type = PTRS_TYPE_UNDEFINED;
				jit_function_set_meta(ctor, PTRS_JIT_FUNCTIONMETA_FUNCAST, func, NULL, 0);
				jit_function_set_meta(ctor, PTRS_JIT_FUNCTIONMETA_CLOSURE, ctor, NULL, 0);

				ctorData = jit_value_get_param(ctor, 0);
				ptrs_initScope(&ctorScope, scope);
				ctorScope.returnType.type = -1;
			}

			currFunc = ctor;
			currData = ctorData;
			currScope = &ctorScope;
		}

		if(curr->type == PTRS_STRUCTMEMBER_FUNCTION
			|| curr->type == PTRS_STRUCTMEMBER_GETTER
			|| curr->type == PTRS_STRUCTMEMBER_SETTER)
		{
			curr->value.function.func = ptrs_jit_createFunctionFromAst(node, func,
				curr->value.function.ast);
		}
		else if(curr->type == PTRS_STRUCTMEMBER_VAR)
		{
			ptrs_ast_t *ast = curr->value.startval;
			ptrs_jit_var_t startVal = ast->vtable->get(ast, currFunc, currScope);
			jit_value_t addr = jit_insn_add_relative(currFunc, currData, curr->offset);
			jit_insn_store_relative(currFunc, addr, 0, startVal.val);
			jit_insn_store_relative(currFunc, addr, sizeof(ptrs_val_t), startVal.meta);
		}
		// TODO allow initializers of arrays
	}

	//build all overloads
	for(struct ptrs_opoverload *curr = struc->overloads; curr != NULL; curr = curr->next)
	{
		ptrs_jit_buildFunction(node, curr->handlerFunc, scope, curr->handler, struc);
	}

	//build the data initializer if the function has no real constructor
	if(ctor != NULL && !hasCtorOverload)
	{
		jit_insn_default_return(ctor);
		ptrs_jit_placeAssertions(ctor, &ctorScope);

		if(ptrs_compileAot && jit_function_compile(ctor) == 0)
			ptrs_error(node, "Failed compiling the constructor of function %s", struc->name);

		struct ptrs_opoverload *ctorOverload = malloc(sizeof(struct ptrs_opoverload));
		ctorOverload->op = ptrs_handle_new;
		ctorOverload->isStatic = true;
		ctorOverload->handler = NULL;
		ctorOverload->handlerFunc = ctor;

		ctorOverload->next = struc->overloads;
		struc->overloads = ctorOverload;
	}

	//build all functions
	for(int i = 0; i < struc->memberCount; i++)
	{
		struct ptrs_structmember *curr = &struc->member[i];
		if(curr->name == NULL)
			continue;
		if(curr->type != PTRS_STRUCTMEMBER_FUNCTION
			&& curr->type != PTRS_STRUCTMEMBER_GETTER
			&& curr->type != PTRS_STRUCTMEMBER_SETTER)
			continue;

		ptrs_jit_buildFunction(node, curr->value.function.func, scope,
			curr->value.function.ast, struc);
	}

	return *result;
}

ptrs_jit_var_t ptrs_handle_if(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_ifelse *stmt = &node->arg.ifelse;

	ptrs_jit_var_t condition = stmt->condition->vtable->get(stmt->condition, func, scope);

	jit_label_t end = jit_label_undefined;
	jit_label_t isFalse = jit_label_undefined;

	if(stmt->ifBody != NULL)
	{
		ptrs_jit_branch_if_not(func, &isFalse, condition);
		stmt->ifBody->vtable->get(stmt->ifBody, func, scope);

		if(stmt->elseBody != NULL)
			jit_insn_branch(func, &end);
	}
	else
	{
		ptrs_jit_branch_if(func, &end, condition);
	}

	if(stmt->elseBody != NULL)
	{
		jit_insn_label(func, &isFalse);
		stmt->elseBody->vtable->get(stmt->elseBody, func, scope);

		jit_insn_label(func, &end);
	}
	else
	{
		jit_insn_label(func, &isFalse);
		jit_insn_label(func, &end);
	}

	return condition;
}

ptrs_jit_var_t ptrs_handle_switch(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_switch *stmt = &node->arg.switchcase;
	struct ptrs_ast_case *curr = stmt->cases;
	jit_label_t done = jit_label_undefined;
	int64_t interval = stmt->max - stmt->min + 1;

	ptrs_jit_var_t condition = stmt->condition->vtable->get(stmt->condition, func, scope);
	jit_value_t val = ptrs_jit_vartoi(func, condition);

	if(jit_value_is_constant(condition.val))
	{
		int64_t _val = jit_value_get_long_constant(condition.val);
		bool hadCase = false;

		while(curr != NULL)
		{
			if(_val >= curr->min && _val <= curr->max)
			{
				curr->body->vtable->get(curr->body, func, scope);
				hadCase = true;
				break;
			}
			curr = curr->next;
		}

		if(!hadCase)
			stmt->defaultCase->vtable->get(stmt->defaultCase, func, scope);
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

		jit_label_t defaultCase = jit_label_undefined;
		jit_insn_jump_table(func, val, table, interval);
		jit_insn_branch(func, &defaultCase);

		while(curr != NULL)
		{
			ptrs_ast_t *body = curr->body;
			while(curr != NULL && curr->body == body)
			{
				for(int i = curr->min - stmt->min; i <= curr->max - stmt->min; i++)
				{
					jit_insn_label(func, table + i);
					hasCase[i] = true;
				}
				curr = curr->next;
			}

			body->vtable->get(body, func, scope);

			jit_insn_branch(func, &done);
		}

		for(int i = 0; i < interval; i++)
		{
			if(!hasCase[i])
				jit_insn_label(func, table + i);
		}
		jit_insn_label(func, &defaultCase);

		if(stmt->defaultCase != NULL)
			stmt->defaultCase->vtable->get(stmt->defaultCase, func, scope);
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
				jit_insn_branch_if(func, caseCondition, cases + i);
			}
			else
			{
				jit_label_t noMatch = jit_label_undefined;

				caseCondition = jit_insn_ge(func, val, jit_const_int(func, long, curr->min));
				jit_insn_branch_if_not(func, caseCondition, &noMatch);
				caseCondition = jit_insn_le(func, val, jit_const_int(func, long, curr->max));
				jit_insn_branch_if(func, caseCondition, cases + i);

				jit_insn_label(func, &noMatch);
			}

			curr = curr->next;
		}

		if(stmt->defaultCase != NULL)
		{
			stmt->defaultCase->vtable->get(stmt->defaultCase, func, scope);
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

			body->vtable->get(body, func, scope);

			jit_insn_branch(func, &done);
		}
	}

	jit_insn_label(func, &done);
}

ptrs_jit_var_t ptrs_handle_loop(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *body = node->arg.astval;

	bool oldAllowed = scope->loopControlAllowed;
	bool oldReturn = scope->returnForLoopControl;
	bool oldContinueLabel = scope->hasCustomContinueLabel;
	jit_label_t oldContinue = scope->continueLabel;
	jit_label_t oldBreak = scope->breakLabel;

	scope->loopControlAllowed = true;
	scope->returnForLoopControl = false;
	scope->hasCustomContinueLabel = false;
	scope->continueLabel = jit_label_undefined;
	scope->breakLabel = jit_label_undefined;

	jit_label_t start = jit_label_undefined;
	jit_insn_label(func, &start);

	//run the loop body
	ptrs_jit_var_t val = body->vtable->get(body, func, scope);

	//patch all continues to the end of the loop body
	if(!scope->hasCustomContinueLabel)
		jit_insn_label(func, &scope->continueLabel);

	// branch back to the start of the body
	jit_insn_branch(func, &start);

	//after the loop - patch the breaks
	jit_insn_label(func, &scope->breakLabel);

	scope->loopControlAllowed = oldAllowed;
	scope->returnForLoopControl = oldReturn;
	scope->hasCustomContinueLabel = oldContinueLabel;
	scope->continueLabel = oldContinue;
	scope->breakLabel = oldBreak;

	return val;
}

struct array_iterator_save
{
	ptrs_meta_t meta;
	size_t pos;
};
struct struct_iterator_save
{
	ptrs_struct_t *struc;
	size_t pos;
};
static bool pointerIterator(void *parentFrame, uint8_t *array, ptrs_var_t *varlist, ptrs_meta_t varlistMeta,
	struct array_iterator_save *saveArea, ptrs_meta_t saveAreaMeta)
{
	if(saveArea->pos >= saveArea->meta.array.size)
		return false;

	if(varlistMeta.array.size > 0)
	{
		varlist[0].value.intval = saveArea->pos;
		varlist[0].meta.type = PTRS_TYPE_INT;
	}
	if(varlistMeta.array.size > 1)
	{
		ptrs_nativetype_info_t *type = &ptrs_nativeTypes[saveArea->meta.array.typeIndex];
		type->getHandler(array + type->size * saveArea->pos, type->size, varlist + 1);
	}

	for(int i = 2; i < varlistMeta.array.size; i++)
	{
		varlist[i].value.intval = 0;
		varlist[i].meta.type = PTRS_TYPE_UNDEFINED;
	}

	saveArea->pos++;
	return true;
}
static bool structIterator(void *parentFrame, void *data, ptrs_var_t *varlist, ptrs_meta_t varlistMeta,
	struct struct_iterator_save *saveArea, ptrs_meta_t saveAreaMeta)
{
	ptrs_struct_t *struc = saveArea->struc;
	bool isInstance = data != NULL;
	struct ptrs_structmember *curr;
	while(true)
	{
		if(saveArea->pos >= struc->memberCount)
			return false;

		curr = &struc->member[saveArea->pos];
		saveArea->pos++;

		if(curr->name != NULL && curr->type != PTRS_STRUCTMEMBER_SETTER
			&& (curr->isStatic || isInstance))
			break;
	}

	if(varlistMeta.array.size > 0)
	{
		varlist[0].value.ptrval = curr->name;
		varlist[0].meta.type = PTRS_TYPE_POINTER;
		varlist[0].meta.array.typeIndex = PTRS_NATIVETYPE_INDEX_CHAR;
		varlist[0].meta.array.size = curr->namelen + 1;
	}
	if(varlistMeta.array.size > 1)
	{
		varlist[1] = ptrs_struct_getMember(NULL, data, struc, curr);
	}

	for(int i = 2; i < varlistMeta.array.size; i++)
	{
		varlist[i].value.intval = 0;
		varlist[i].meta.type = PTRS_TYPE_UNDEFINED;
	}

	return true;
}
static void *getForeachIterator(ptrs_ast_t *node, void **parentFrame, void *saveArea,
	ptrs_val_t val, ptrs_meta_t meta)
{
	if(meta.type == PTRS_TYPE_POINTER)
	{
		struct array_iterator_save *arraySave = saveArea;
		arraySave->pos = 0;
		arraySave->meta = meta;
		return pointerIterator;
	}
	else if(meta.type == PTRS_TYPE_STRUCT)
	{
		ptrs_struct_t *struc = ptrs_meta_getPointer(meta);
		*parentFrame = struc->parentFrame;

		void *handler = ptrs_struct_getOverloadClosure(struc, ptrs_handle_forin_step, val.ptrval != NULL);
		if(handler != NULL)
		{
			ptrs_var_t *varSave = saveArea;
			varSave->value.intval = 0;
			varSave->meta.type = PTRS_TYPE_UNDEFINED;
			return handler;
		}
		else
		{
			struct struct_iterator_save *strucSave = saveArea;
			strucSave->struc = struc;
			strucSave->pos = 0;
			return structIterator;
		}
	}
	else
	{
		ptrs_error(node, "Cannot iterate over value of type %t", meta.type);
	}
}
ptrs_jit_var_t ptrs_handle_forin_setup(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_forin *stmt = &node->arg.forin;
	stmt->value = stmt->valueAst->vtable->get(stmt->valueAst, func, scope);

	if(stmt->value.constType == PTRS_TYPE_POINTER && jit_value_is_constant(stmt->value.meta))
	{
		// set up variables for `iterateArray`
		stmt->saveArea = stmt->value.meta;
		stmt->iterator = jit_value_create(func, jit_type_ulong);
		jit_insn_store(func, stmt->iterator, jit_const_long(func, ulong, 0));

		// calling ptrs_getNativeTypeForArray makes sure the type and the typeIndex are correct
		ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(stmt->value.meta);
		ptrs_getNativeTypeForArray(node, meta);

		return stmt->value;
	}

	if(stmt->value.constType != -1 && stmt->value.constType != PTRS_TYPE_STRUCT)
		ptrs_error(node, "Cannot iterate over value of type %t", stmt->value.constType);

	stmt->saveArea = jit_insn_array(func, sizeof(ptrs_var_t));
	stmt->varlist = jit_insn_array(func, stmt->varcount * sizeof(ptrs_var_t));
	stmt->parentFrame = jit_value_create(func, jit_type_void_ptr);

	jit_value_t jitNode = jit_const_int(func, void_ptr, (uintptr_t)node);
	jit_value_t parentFramePtr = jit_insn_address_of(func, stmt->parentFrame);
	ptrs_jit_reusableCall(func, getForeachIterator, stmt->iterator, jit_type_void_ptr,
		(jit_type_void_ptr, jit_type_void_ptr, jit_type_void_ptr, jit_type_long, jit_type_ulong),
		(jitNode, parentFramePtr, stmt->saveArea, stmt->value.val, stmt->value.meta)
	);

	return stmt->value;
}

void iterateArray(struct ptrs_ast_forin *stmt, jit_function_t func, ptrs_scope_t *scope)
{
	// the value is an array, we can iterate over it without an iterator function
	// stmt->iterator holds the current index
	// stmt->saveArea holds the constant meta

	ptrs_meta_t meta = ptrs_jit_value_getMetaConstant(stmt->saveArea);

	jit_value_t condition = jit_insn_lt(func, stmt->iterator, jit_const_long(func, ulong, meta.array.size));
	jit_insn_branch_if_not(func, condition, &scope->breakLabel);

	stmt->varsymbols[0].val = jit_insn_dup(func, stmt->iterator);
	stmt->varsymbols[0].meta = ptrs_jit_const_meta(func, PTRS_TYPE_INT);
	stmt->varsymbols[0].constType = PTRS_TYPE_INT;
	stmt->varsymbols[0].addressable = false;

	ptrs_nativetype_info_t *type = &ptrs_nativeTypes[meta.array.typeIndex];
	jit_value_t loadedValue = jit_insn_load_elem(func, stmt->value.val, stmt->iterator, type->jitType);

	ptrs_jit_var_t result;
	if(type->varType == PTRS_TYPE_FLOAT)
	{
		loadedValue = jit_insn_convert(func, loadedValue, jit_type_float64, 0);
		stmt->varsymbols[1].val = ptrs_jit_reinterpretCast(func, loadedValue, jit_type_long);
		stmt->varsymbols[1].meta = ptrs_jit_const_meta(func, PTRS_TYPE_FLOAT);
		stmt->varsymbols[1].constType = PTRS_TYPE_FLOAT;
		stmt->varsymbols[1].addressable = false;
	}
	else
	{
		stmt->varsymbols[1].val = jit_insn_convert(func, loadedValue, jit_type_long, 0);
		stmt->varsymbols[1].meta = ptrs_jit_const_meta(func, type->varType);
		stmt->varsymbols[1].constType = type->varType;
		stmt->varsymbols[1].addressable = false;
	}

	jit_insn_store(func, stmt->iterator, jit_insn_add(func, stmt->iterator, jit_const_long(func, ulong, 1)));
}
void iterateWithIteratorFunction(struct ptrs_ast_forin *stmt, jit_function_t func, ptrs_scope_t *scope)
{
	static jit_type_t iteratorSig = NULL;
	if(iteratorSig == NULL)
	{
		jit_type_t args[6] = {
			jit_type_long, //parent_frame (for structs) or val.val (for arrays)
			jit_type_ulong, //val.val (for structs) or val.meta (for arrays) 
			jit_type_long, //varlist.val
			jit_type_ulong, //varlist.meta
			jit_type_long, //saveArea.val
			jit_type_ulong, //saveArea.meta
		};
		iteratorSig = jit_type_create_signature(jit_abi_cdecl, jit_type_ubyte, args, 6, 0);
	}

	jit_value_t args[6] = {
		stmt->parentFrame,
		stmt->value.val,
		stmt->varlist,
		ptrs_jit_const_arrayMeta(func, stmt->varcount, PTRS_NATIVETYPE_INDEX_VAR),
		stmt->saveArea,
		ptrs_jit_const_arrayMeta(func, 1, PTRS_NATIVETYPE_INDEX_VAR),
	};
	jit_value_t done = jit_insn_call_indirect(func, stmt->iterator, iteratorSig, args, 6, 0);

	jit_insn_branch_if_not(func, done, &scope->breakLabel);

	for(int i = 0; i < stmt->varcount; i++)
	{
		stmt->varsymbols[i].val = jit_insn_load_relative(func, stmt->varlist,
			i * sizeof(ptrs_var_t), jit_type_long);
		stmt->varsymbols[i].meta = jit_insn_load_relative(func, stmt->varlist,
			i * sizeof(ptrs_var_t) + sizeof(ptrs_val_t), jit_type_ulong);

		stmt->varsymbols[i].constType = -1;
		stmt->varsymbols[i].addressable = false;
	}
}
ptrs_jit_var_t ptrs_handle_forin_step(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	struct ptrs_ast_forin *stmt = node->arg.forinptr;

	if(stmt->value.constType == PTRS_TYPE_POINTER)
		iterateArray(stmt, func, scope);
	else
		iterateWithIteratorFunction(stmt, func, scope);
}

ptrs_jit_var_t ptrs_handle_exprstatement(ptrs_ast_t *node, jit_function_t func, ptrs_scope_t *scope)
{
	ptrs_ast_t *expr = node->arg.astval;

	if(expr != NULL)
		return expr->vtable->get(expr, func, scope);
}
