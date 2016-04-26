#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../parser/ast.h"
#include "../parser/common.h"
#include "include/error.h"
#include "include/conversion.h"
#include "include/scope.h"

void binary_typeerror(ptrs_ast_t *node, const char *op, ptrs_vartype_t tleft, ptrs_vartype_t tright)
{
	ptrs_error(node, "Cannot use operator %s on variables of type %s and %s",
		op, ptrs_typetoa(tleft), ptrs_typetoa(tright));
}

#define binary_floatop(operator) \
	else if(tleft == PTRS_TYPE_FLOAT || tright == PTRS_TYPE_FLOAT) \
	{ \
		float fleft = ptrs_vartof(left); \
		float fright = ptrs_vartof(right); \
		\
		result->type = PTRS_TYPE_FLOAT; \
		result->value.floatval = fleft operator fright; \
	}

#define binary_pointer_compare(operator) \
	else if((tleft == PTRS_TYPE_INT || tleft == PTRS_TYPE_POINTER || tleft == PTRS_TYPE_NATIVE) \
		&& (tright == PTRS_TYPE_INT || tright == PTRS_TYPE_POINTER || tright == PTRS_TYPE_NATIVE)) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.intval operator right->value.intval; \
	} \

#define binary_pointer_add() \
	else if(((tleft == PTRS_TYPE_NATIVE) ^ (tright == PTRS_TYPE_NATIVE)) \
	 	&& (tleft == PTRS_TYPE_INT || tright == PTRS_TYPE_INT)) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.intval = left->value.intval + right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval + right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.intval + right->value.ptrval; \
	}

#define binary_pointer_sub() \
	else if(((tleft == PTRS_TYPE_NATIVE) ^ (tright == PTRS_TYPE_NATIVE)) \
		&& (tleft == PTRS_TYPE_INT || tright == PTRS_TYPE_INT)) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.intval = left->value.intval - right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_NATIVE && tright == PTRS_TYPE_NATIVE) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.strval - right->value.strval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval - right->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.ptrval - right->value.ptrval; \
	}

#define binary_string_equal() \
	else if(tleft == PTRS_TYPE_STRING || tright == PTRS_TYPE_STRING) \
	{ \
		char buff[32]; \
		const char *strleft = ptrs_vartoa(left, buff, 32);; \
		const char *strright = ptrs_vartoa(right, buff, 32); \
		\
		result->type = PTRS_TYPE_INT; \
		result->value.intval = strcmp(strleft, strright) == 0; \
	}


#define handle_binary(name, operator, oplabel, isAssign, ...) \
	ptrs_var_t *ptrs_handle_op_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_var_t leftv; \
		ptrs_var_t rightv; \
		struct ptrs_ast_binary expr = node->arg.binary; \
		\
		ptrs_var_t *left = expr.left->handler(expr.left, &leftv, scope); \
		ptrs_var_t *right = expr.right->handler(expr.right, &rightv, scope); \
		ptrs_vartype_t tleft = left->type; \
		ptrs_vartype_t tright = right->type; \
		\
		if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_INT) \
		{ \
			result->type = PTRS_TYPE_INT; \
			result->value.intval = left->value.intval operator right->value.intval; \
		} \
		__VA_ARGS__ \
		else if(tleft == PTRS_TYPE_INT || tright == PTRS_TYPE_INT) \
		{ \
			int64_t ileft = ptrs_vartoi(left); \
			int64_t iright = ptrs_vartoi(right); \
			\
			result->type = PTRS_TYPE_INT; \
			result->value.intval = ileft operator iright; \
		} \
		else \
		{ \
			binary_typeerror(node, oplabel, tleft, tright); \
		} \
		\
		if(isAssign) \
		{ \
			memcpy(left, result, sizeof(ptrs_var_t)); \
			return left; \
		} \
		\
		return result; \
	} \

handle_binary(equal, ==, "==", false, binary_floatop(==) binary_pointer_compare(==) binary_string_equal())
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
handle_binary(add, +, "+", false, binary_floatop(+) binary_pointer_add())
handle_binary(sub, -, "-", false, binary_floatop(-) binary_pointer_sub())
handle_binary(mul, *, "*", false, binary_floatop(*))
handle_binary(div, /, "/", false, binary_floatop(/))
handle_binary(mod, %, "%", false)
handle_binary(addassign, +=, "+=", true, binary_floatop(+=))
handle_binary(subassign, -=, "-=", true, binary_floatop(-=))
handle_binary(mulassign, *=, "*=", true, binary_floatop(*=))
handle_binary(divassign, /=, "/=", true, binary_floatop(/=))
handle_binary(modassign, %=, "%=", true)
handle_binary(shrassign, >>=, ">>=", true)
handle_binary(shlassign, <<=, "<<=", true)
handle_binary(andassign, &=, "&=", true)
handle_binary(xorassign, ^=, "^=", true)
handle_binary(orassign, |=, "|=", true)

ptrs_var_t *ptrs_handle_op_assign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t rightv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *left = expr.left->handler(expr.left, result, scope);
	ptrs_var_t *right = expr.right->handler(expr.right, &rightv, scope);

	memcpy(left, right, sizeof(ptrs_var_t));
	return left;
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

#define handle_prefix(name, operator, opLabel, handlefloat, handleptr) \
	ptrs_var_t *ptrs_handle_prefix_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_var_t *value = node->arg.astval->handler(node->arg.astval, result, scope); \
		ptrs_vartype_t type = value->type; \
		\
		result->type = type; \
		if(type == PTRS_TYPE_INT) \
			result->value.intval = operator value->value.intval; \
		handlefloat \
		handleptr \
		else \
			ptrs_error(node, "Cannot use prefixed operator %s on variable of type %s", opLabel, ptrs_typetoa(type)); \
		\
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

handle_prefix(inc, ++, "++", handle_prefix_float(++), handle_prefix_ptr(++))
handle_prefix(dec, --, "--", handle_prefix_float(--), handle_prefix_ptr(--))
handle_prefix(not, ~, "~", /*nothing*/, /*nothing*/)
handle_prefix(plus, +, "+", handle_prefix_float(+), /*nothing*/)
handle_prefix(minus, -, "-", handle_prefix_float(-), /*nothing*/)

#define handle_suffix(name, operator, opLabel) \
	ptrs_var_t *ptrs_handle_suffix_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_var_t *value = node->arg.astval->handler(node->arg.astval, result, scope); \
		ptrs_vartype_t type = value->type; \
		\
		result->type = type; \
		if(type == PTRS_TYPE_INT) \
			result->value.intval = value->value.intval operator; \
		else if(type == PTRS_TYPE_FLOAT) \
			result->value.floatval = value->value.floatval operator; \
		else if(type == PTRS_TYPE_NATIVE) \
			result->value.strval = value->value.strval operator; \
		else if(type == PTRS_TYPE_POINTER) \
			result->value.ptrval = value->value.ptrval operator; \
		else \
			ptrs_error(node, "Cannot use suffixed operator %s on variable of type %s", opLabel, ptrs_typetoa(type)); \
		\
		return result; \
	}

handle_suffix(inc, ++, "++")
handle_suffix(dec, --, "--")
