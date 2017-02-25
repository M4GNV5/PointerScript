#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"
#include "include/call.h"
#include "include/struct.h"

static void binary_typeerror(ptrs_ast_t *node, ptrs_scope_t *scope, const char *op, ptrs_vartype_t tleft, ptrs_vartype_t tright)
{
	ptrs_error(node, scope, "Cannot use operator %s on variables of type %s and %s",
		op, ptrs_typetoa(tleft), ptrs_typetoa(tright));
}

#define binary_typecheck(val) \
	else if(tleft != tright) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = val; \
	}

#define binary_floatop(operator) \
	else if((tleft == PTRS_TYPE_FLOAT || tleft == PTRS_TYPE_INT) \
	 	&& (tright == PTRS_TYPE_FLOAT || tright == PTRS_TYPE_INT)) \
	{ \
		double fleft = ptrs_vartof(left); \
		double fright = ptrs_vartof(right); \
		\
		result->type = PTRS_TYPE_FLOAT; \
		result->value.floatval = fleft operator fright; \
	}

#define binary_assignfloatop(operator) \
	else if((tleft == PTRS_TYPE_FLOAT || tleft == PTRS_TYPE_INT) \
	 	&& (tright == PTRS_TYPE_FLOAT || tright == PTRS_TYPE_INT)) \
	{ \
		double fleft = ptrs_vartof(left); \
		double fright = ptrs_vartof(right); \
		\
		result->type = left->type = PTRS_TYPE_FLOAT; \
		result->value.floatval = left->value.floatval = fleft operator fright; \
	}

#define binary_pointer_compare(operator) \
	else if((tleft == PTRS_TYPE_INT || tleft > PTRS_TYPE_FLOAT) \
		&& (tright == PTRS_TYPE_INT || tright > PTRS_TYPE_FLOAT)) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.intval operator right->value.intval; \
	} \

#define binary_pointer_add(operator, isAssign) \
	else if(tleft == PTRS_TYPE_NATIVE && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.nativeval = left->value.nativeval operator right->value.intval; \
		result->meta.array.size = left->meta.array.size - right->value.intval; \
		result->meta.array.readOnly = left->meta.array.readOnly; \
	} \
	else if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_NATIVE) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.nativeval = left->value.intval + right->value.nativeval; \
		result->meta.array.size = right->meta.array.size - left->value.intval; \
		result->meta.array.readOnly = right->meta.array.readOnly; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_NATIVE; \
			left->value.nativeval = result->value.nativeval; \
		} \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval operator right->value.intval; \
		result->meta.array.size = left->meta.array.size - right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.intval + right->value.ptrval; \
		result->meta.array.size = right->meta.array.size - left->value.intval; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_POINTER; \
			left->value.ptrval = result->value.ptrval; \
		} \
	}

#define binary_pointer_sub(operator, isAssign) \
	else if((tleft == PTRS_TYPE_NATIVE) ^ (tright == PTRS_TYPE_NATIVE) \
		&& (tleft == PTRS_TYPE_INT || tright == PTRS_TYPE_INT)) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.intval = left->value.intval operator right->value.intval; \
		result->meta.array.readOnly = tleft == PTRS_TYPE_NATIVE ? left->meta.array.readOnly : right->meta.array.readOnly; \
	} \
	else if(tleft == PTRS_TYPE_NATIVE && tright == PTRS_TYPE_NATIVE) \
	{ \
		result->type = PTRS_TYPE_INT; \
		if(isAssign) \
			left->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.intval operator right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval operator right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.ptrval - right->value.ptrval; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_INT; \
			left->value.intval = result->value.intval; \
		} \
	}



#define handle_binary(name, operator, oplabel, isAssign, ...) \
	ptrs_var_t *ptrs_handle_op_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_var_t leftv; \
		ptrs_var_t rightv; \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		ptrs_function_t *overload; \
		\
		ptrs_var_t *left = expr->left->handler(expr->left, &leftv, scope); \
		ptrs_var_t *right = expr->right->handler(expr->right, &rightv, scope); \
		ptrs_vartype_t tleft = left->type; \
		ptrs_vartype_t tright = right->type; \
		\
		if(tleft == PTRS_TYPE_STRUCT && (overload = ptrs_struct_getOverload(left, ptrs_handle_op_##name, true)) != NULL) \
		{ \
			ptrs_var_t func = {{.funcval = overload}, .type = PTRS_TYPE_FUNCTION}; \
			return ptrs_callfunc(node, result, scope, left->value.structval, &func, 1, right); \
		} \
		else if(tright == PTRS_TYPE_STRUCT && (overload = ptrs_struct_getOverload(right, ptrs_handle_op_##name, false)) != NULL) \
		{ \
			ptrs_var_t func = {{.funcval = overload}, .type = PTRS_TYPE_FUNCTION}; \
			return ptrs_callfunc(node, result, scope, right->value.structval, &func, 1, left); \
		} \
		else if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_INT) \
		{ \
			result->type = PTRS_TYPE_INT; \
			result->value.intval = left->value.intval operator right->value.intval; \
		} \
		__VA_ARGS__ \
		else \
		{ \
			binary_typeerror(node, scope, oplabel, tleft, tright); \
		} \
		\
		if(isAssign && left == &leftv) \
			expr->left->setHandler(expr->left, result, scope); \
		\
		return result; \
	} \

handle_binary(typeequal, ==, "===", false, binary_typecheck(false) binary_floatop(==) binary_pointer_compare(==))
handle_binary(typeinequal, ==, "!==", false, binary_typecheck(true) binary_floatop(!=) binary_pointer_compare(!=))
handle_binary(equal, ==, "==", false, binary_floatop(==) binary_pointer_compare(==))
handle_binary(inequal, !=, "!=", false, binary_floatop(!=) binary_pointer_compare(!=))
handle_binary(lessequal, <=, "<=", false, binary_floatop(<=) binary_pointer_compare(<=))
handle_binary(greaterequal, >=, ">=", false, binary_floatop(>=) binary_pointer_compare(>=))
handle_binary(less, <, "<", false, binary_floatop(<) binary_pointer_compare(<))
handle_binary(greater, >, ">", false, binary_floatop(>) binary_pointer_compare(>))
handle_binary(or, |, "|", false)
handle_binary(xor, ^, "^", false)
handle_binary(and, &, "&", false)
handle_binary(shr, >>, ">>", false)
handle_binary(shl, <<, "<<", false)
handle_binary(add, +, "+", false, binary_floatop(+) binary_pointer_add(+, false))
handle_binary(sub, -, "-", false, binary_floatop(-) binary_pointer_sub(-, false))
handle_binary(mul, *, "*", false, binary_floatop(*))
handle_binary(div, /, "/", false, binary_floatop(/))
handle_binary(mod, %, "%", false)
handle_binary(addassign, +=, "+=", true, binary_assignfloatop(+=) binary_pointer_add(+=, true))
handle_binary(subassign, -=, "-=", true, binary_assignfloatop(-=) binary_pointer_sub(-=, true))
handle_binary(mulassign, *=, "*=", true, binary_assignfloatop(*=))
handle_binary(divassign, /=, "/=", true, binary_assignfloatop(/=))
handle_binary(modassign, %=, "%=", true)
handle_binary(shrassign, >>=, ">>=", true)
handle_binary(shlassign, <<=, "<<=", true)
handle_binary(andassign, &=, "&=", true)
handle_binary(xorassign, ^=, "^=", true)
handle_binary(orassign, |=, "|=", true)

ptrs_var_t *ptrs_handle_op_assign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *right = expr.right->handler(expr.right, result, scope);
	expr.left->setHandler(expr.left, right, scope);

	return right;
}

ptrs_var_t *ptrs_handle_op_logicor(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary expr = node->arg.binary;
	ptrs_var_t *left = expr.left->handler(expr.left, result, scope);

	if(ptrs_vartob(left))
		return left;
	else
		return expr.right->handler(expr.right, result, scope);
}

ptrs_var_t *ptrs_handle_op_logicxor(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary expr = node->arg.binary;
	bool left = ptrs_vartob(expr.left->handler(expr.left, result, scope));
	bool right = ptrs_vartob(expr.right->handler(expr.right, result, scope));

	result->type = PTRS_TYPE_INT;
	if((left && !right) || (!left && right))
		result->value.intval = true;
	else
		result->value.intval = false;

	return result;
}

ptrs_var_t *ptrs_handle_op_logicand(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	struct ptrs_ast_binary expr = node->arg.binary;
	ptrs_var_t *left = expr.left->handler(expr.left, result, scope);

	if(!ptrs_vartob(left))
		return left;
	else
		return expr.right->handler(expr.right, result, scope);
}

ptrs_var_t *ptrs_handle_prefix_logicnot(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t *value = node->arg.astval->handler(node->arg.astval, result, scope);
	result->type = PTRS_TYPE_INT;
	result->value.intval = !ptrs_vartob(value);
	return result;
}

#define handle_prefix(name, operator, opLabel, isAssign, handlefloat, handleptr) \
	ptrs_var_t *ptrs_handle_prefix_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		ptrs_var_t *value = expr->handler(expr, result, scope); \
		ptrs_vartype_t type = value->type; \
		ptrs_var_t overload; \
		result->type = type; \
		\
		if(type == PTRS_TYPE_STRUCT && (overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_prefix_##name, true)) != NULL) \
		{ \
			overload.type = PTRS_TYPE_FUNCTION; \
			return ptrs_callfunc(node, result, scope, value->value.structval, &overload, 0, NULL); \
		} \
		\
		if(type == PTRS_TYPE_INT) \
			result->value.intval = operator value->value.intval; \
		handlefloat \
		handleptr \
		else \
			ptrs_error(node, scope, "Cannot use prefixed operator %s on variable of type %s", opLabel, ptrs_typetoa(type)); \
		\
		if(isAssign && result == value) \
			expr->setHandler(expr, result, scope); \
		\
		result->meta = value->meta; \
		return result; \
	}

#define handle_prefix_ptr(operator) \
	else if(type == PTRS_TYPE_NATIVE) \
		result->value.strval = operator value->value.strval; \
	else if(type == PTRS_TYPE_POINTER) \
		result->value.ptrval = operator value->value.ptrval;

#define handle_prefix_float(operator) \
	else if(type == PTRS_TYPE_FLOAT) \
		result->value.floatval = operator value->value.floatval;

handle_prefix(inc, ++, "++", true, handle_prefix_float(++), handle_prefix_ptr(++))
handle_prefix(dec, --, "--", true, handle_prefix_float(--), handle_prefix_ptr(--))
handle_prefix(not, ~, "~", false, /*nothing*/, /*nothing*/)
handle_prefix(plus, +, "+", false, handle_prefix_float(+), /*nothing*/)
handle_prefix(minus, -, "-", false, handle_prefix_float(-), /*nothing*/)

#define handle_suffix(name, operator, opLabel) \
	ptrs_var_t *ptrs_handle_suffix_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_ast_t *expr = node->arg.astval; \
		ptrs_var_t valuev; \
		ptrs_var_t *value = expr->handler(expr, &valuev, scope); \
		ptrs_vartype_t type = value->type; \
		ptrs_var_t overload; \
		result->type = type; \
		\
		if(type == PTRS_TYPE_STRUCT && (overload.value.funcval = ptrs_struct_getOverload(value, ptrs_handle_suffix_##name, true)) != NULL) \
		{ \
			overload.type = PTRS_TYPE_FUNCTION; \
			return ptrs_callfunc(node, result, scope, value->value.structval, &overload, 0, NULL); \
		} \
		\
		if(type == PTRS_TYPE_INT) \
			result->value.intval = value->value.intval operator; \
		else if(type == PTRS_TYPE_FLOAT) \
			result->value.floatval = value->value.floatval operator; \
		else if(type == PTRS_TYPE_NATIVE) \
			result->value.strval = value->value.strval operator; \
		else if(type == PTRS_TYPE_POINTER) \
			result->value.ptrval = value->value.ptrval operator; \
		else \
			ptrs_error(node, scope, "Cannot use suffixed operator %s on variable of type %s", opLabel, ptrs_typetoa(type)); \
		\
		if(value == &valuev) \
			expr->setHandler(expr, value, scope); \
		\
		result->meta = value->meta; \
		return result; \
	}

handle_suffix(inc, ++, "++")
handle_suffix(dec, --, "--")
