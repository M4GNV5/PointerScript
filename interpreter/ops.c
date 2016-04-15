#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

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
		float fleft = tleft == PTRS_TYPE_FLOAT ? left->value.floatval : left->value.intval; \
		float fright = tright == PTRS_TYPE_FLOAT ? right->value.floatval : right->value.intval; \
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
		result->value.intval = left->value.intval + left->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval + left->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_INT && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.intval + left->value.ptrval; \
	}

#define binary_pointer_sub() \
	else if(((tleft == PTRS_TYPE_NATIVE) ^ (tright == PTRS_TYPE_NATIVE)) \
		&& (tleft == PTRS_TYPE_INT || tright == PTRS_TYPE_INT)) \
	{ \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.intval = left->value.intval - left->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_NATIVE && tright == PTRS_TYPE_NATIVE) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.strval - left->value.strval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_INT) \
	{ \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval - left->value.intval; \
	} \
	else if(tleft == PTRS_TYPE_POINTER && tright == PTRS_TYPE_POINTER) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.ptrval - left->value.ptrval; \
	}


#define handle_binary(name, operator, oplabel, ...) \
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
		else \
		{ \
			binary_typeerror(node, oplabel, tleft, tright); \
		} \
		\
		return result; \
	} \

handle_binary(equal, ==, "==", binary_floatop(==) binary_pointer_compare(==))
handle_binary(inequal, !=, "!=", binary_floatop(!=) binary_pointer_compare(!=))
handle_binary(lessequal, <=, "<=", binary_floatop(<=) binary_pointer_compare(<=))
handle_binary(greaterequal, >=, ">=", binary_floatop(>=) binary_pointer_compare(>=))
handle_binary(less, <, "<", binary_floatop(<) binary_pointer_compare(<))
handle_binary(greater, >, ">", binary_floatop(>) binary_pointer_compare(>))
handle_binary(logicor, ||, "||", binary_floatop(||) binary_pointer_compare(||))
handle_binary(logicand, &&, "&&", binary_floatop(&&) binary_pointer_compare(&&))
handle_binary(or, |, "|")
handle_binary(xor, ^, "^")
handle_binary(and, &, "&")
handle_binary(shr, >>, ">>")
handle_binary(shl, <<, "<<")
handle_binary(add, +, "+", binary_floatop(+) binary_pointer_add())
handle_binary(sub, -, "-", binary_floatop(-) binary_pointer_sub())
handle_binary(mul, *, "*", binary_floatop(*))
handle_binary(div, /, "/", binary_floatop(/))
handle_binary(mod, %, "%")

ptrs_var_t *ptrs_handle_op_assign(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope)
{
	ptrs_var_t rightv;
	struct ptrs_ast_binary expr = node->arg.binary;

	ptrs_var_t *left = expr.left->handler(expr.left, result, scope);
	ptrs_var_t *right = expr.right->handler(expr.right, &rightv, scope);

	left->type = right->type;
	left->value = right->value;
	return left;
}

#define handle_assign(name, opfunc) \
	ptrs_var_t *ptrs_handle_op_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		/*TODO*/ \
		ptrs_error(node, "Operator not yet supported"); \
		return result; \
	}
handle_assign(addassign, ptrs_handle_op_add)
handle_assign(subassign, ptrs_handle_op_sub)
handle_assign(mulassign, ptrs_handle_op_mul)
handle_assign(divassign, ptrs_handle_op_div)
handle_assign(modassign, ptrs_handle_op_mod)
handle_assign(shrassign, ptrs_handle_op_shr)
handle_assign(shlassign, ptrs_handle_op_shl)
handle_assign(andassign, ptrs_handle_op_and)
handle_assign(xorassign, ptrs_handle_op_or)
handle_assign(orassign, ptrs_handle_op_or)

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
		result->value.ptrval = value->value.ptrval operator;

#define handle_prefix_float(operator) \
	else if(type == PTRS_TYPE_FLOAT) \
		result->value.floatval = operator value->value.floatval;

handle_prefix(inc, ++, "++", handle_prefix_float(++), handle_prefix_ptr(++))
handle_prefix(dec, --, "--", handle_prefix_float(--), handle_prefix_ptr(--))
handle_prefix(logicnot, ~, "~", /*nothing*/, /*nothing*/)
handle_prefix(not, !, "!", handle_prefix_float(!), /*nothing*/)
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
