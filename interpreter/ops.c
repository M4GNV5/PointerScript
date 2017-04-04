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

#define typcomp(left, right) ((uint8_t)PTRS_TYPE_##left << 4) | (uint8_t)PTRS_TYPE_##right

#define binary_typecheck(val) \
	if(tleft != tright) \
	{ \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = val; \
		return result; \
	}

#define binary_floatop(operator, isAssign) \
	case typcomp(FLOAT, INT): \
	case typcomp(INT, FLOAT): \
	case typcomp(FLOAT, FLOAT): \
		; \
		double fleft = ptrs_vartof(left); \
		double fright = ptrs_vartof(right); \
		\
		result->type = PTRS_TYPE_FLOAT; \
		result->value.floatval = fleft operator fright; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_FLOAT; \
			left->value.floatval = result->value.floatval; \
		} \
		break;

#define binary_pointer_compare(operator) \
	case typcomp(NATIVE, NATIVE): \
	case typcomp(NATIVE, POINTER): \
	case typcomp(NATIVE, FUNCTION): \
	case typcomp(NATIVE, STRUCT): \
	case typcomp(POINTER, NATIVE): \
	case typcomp(POINTER, POINTER): \
	case typcomp(POINTER, FUNCTION): \
	case typcomp(POINTER, STRUCT): \
	case typcomp(STRUCT, NATIVE): \
	case typcomp(STRUCT, POINTER): \
	case typcomp(STRUCT, FUNCTION): \
	case typcomp(STRUCT, STRUCT): \
	case typcomp(FUNCTION, NATIVE): \
	case typcomp(FUNCTION, POINTER): \
	case typcomp(FUNCTION, FUNCTION): \
	case typcomp(FUNCTION, STRUCT): \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.nativeval operator right->value.nativeval; \
		break;

#define binary_pointer_add(operator, isAssign) \
	case typcomp(NATIVE, INT): \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.nativeval = left->value.nativeval operator right->value.intval; \
		result->meta.array.size = left->meta.array.size - right->value.intval; \
		result->meta.array.readOnly = left->meta.array.readOnly; \
		break; \
	case typcomp(INT, NATIVE): \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.nativeval = left->value.intval + right->value.nativeval; \
		result->meta.array.size = right->meta.array.size - left->value.intval; \
		result->meta.array.readOnly = right->meta.array.readOnly; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_NATIVE; \
			left->value.nativeval = result->value.nativeval; \
		} \
		break; \
	case typcomp(POINTER, INT): \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval operator right->value.intval; \
		result->meta.array.size = left->meta.array.size - right->value.intval; \
		break; \
	case typcomp(INT, POINTER): \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.intval + right->value.ptrval; \
		result->meta.array.size = right->meta.array.size - left->value.intval; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_POINTER; \
			left->value.ptrval = result->value.ptrval; \
		} \
		break;

#define binary_pointer_sub(operator, isAssign) \
	case typcomp(NATIVE, INT): \
		result->type = PTRS_TYPE_NATIVE; \
		result->value.intval = left->value.intval operator right->value.intval; \
		result->meta.array.readOnly = tleft == PTRS_TYPE_NATIVE ? left->meta.array.readOnly : right->meta.array.readOnly; \
		break; \
	case typcomp(NATIVE, NATIVE): \
		result->type = PTRS_TYPE_INT; \
		if(isAssign) \
			left->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.intval operator right->value.intval; \
		break; \
	case typcomp(POINTER, INT): \
		result->type = PTRS_TYPE_POINTER; \
		result->value.ptrval = left->value.ptrval operator right->value.intval; \
		break; \
	case typcomp(POINTER, POINTER): \
		result->type = PTRS_TYPE_INT; \
		result->value.intval = left->value.ptrval - right->value.ptrval; \
		if(isAssign) \
		{ \
			left->type = PTRS_TYPE_INT; \
			left->value.intval = result->value.intval; \
		} \
		break;



#define handle_binary(name, operator, oplabel, isAssign, cases, preCheck) \
	ptrs_var_t *ptrs_handle_op_##name(ptrs_ast_t *node, ptrs_var_t *result, ptrs_scope_t *scope) \
	{ \
		ptrs_var_t leftv; \
		ptrs_var_t rightv; \
		struct ptrs_ast_binary *expr = &node->arg.binary; \
		\
		ptrs_var_t *left = expr->left->handler(expr->left, &leftv, scope); \
		ptrs_var_t *right = expr->right->handler(expr->right, &rightv, scope); \
		ptrs_vartype_t tleft = left->type; \
		ptrs_vartype_t tright = right->type; \
		\
		preCheck \
		\
		switch(((uint8_t)tleft << 4) | (uint8_t)tright) \
		{ \
			case typcomp(INT, INT): \
				result->type = PTRS_TYPE_INT; \
				result->value.intval = left->value.intval operator right->value.intval; \
				break; \
			cases \
			default: \
				ptrs_error(node, scope, "Cannot use operator %s on variables of type %s and %s", \
					oplabel, ptrs_typetoa(tleft), ptrs_typetoa(tright)); \
		} \
		\
		if(isAssign && left == &leftv) \
			expr->left->setHandler(expr->left, result, scope); \
		\
		return result; \
	} \

handle_binary(typeequal, ==, "===", false, binary_floatop(==, false) binary_pointer_compare(==), binary_typecheck(false))
handle_binary(typeinequal, ==, "!==", false, binary_floatop(!=, false) binary_pointer_compare(!=), binary_typecheck(true))
handle_binary(equal, ==, "==", false, binary_floatop(==, false) binary_pointer_compare(==), /*none*/)
handle_binary(inequal, !=, "!=", false, binary_floatop(!=, false) binary_pointer_compare(!=), /*none*/)
handle_binary(lessequal, <=, "<=", false, binary_floatop(<=, false) binary_pointer_compare(<=), /*none*/)
handle_binary(greaterequal, >=, ">=", false, binary_floatop(>=, false) binary_pointer_compare(>=), /*none*/)
handle_binary(less, <, "<", false, binary_floatop(<, false) binary_pointer_compare(<), /*none*/)
handle_binary(greater, >, ">", false, binary_floatop(>, false) binary_pointer_compare(>), /*none*/)
handle_binary(or, |, "|", false, /*none*/, /*none*/)
handle_binary(xor, ^, "^", false, /*none*/, /*none*/)
handle_binary(and, &, "&", false, /*none*/, /*none*/)
handle_binary(shr, >>, ">>", false, /*none*/, /*none*/)
handle_binary(shl, <<, "<<", false, /*none*/, /*none*/)
handle_binary(add, +, "+", false, binary_floatop(+, false) binary_pointer_add(+, false), /*none*/)
handle_binary(sub, -, "-", false, binary_floatop(-, false) binary_pointer_sub(-, false), /*none*/)
handle_binary(mul, *, "*", false, binary_floatop(*, false), /*none*/)
handle_binary(div, /, "/", false, binary_floatop(/, false), /*none*/)
handle_binary(mod, %, "%", false, /*none*/, /*none*/)
handle_binary(addassign, +=, "+=", true, binary_floatop(+=, true) binary_pointer_add(+=, true), /*none*/)
handle_binary(subassign, -=, "-=", true, binary_floatop(-=, true) binary_pointer_sub(-=, true), /*none*/)
handle_binary(mulassign, *=, "*=", true, binary_floatop(*=, true), /*none*/)
handle_binary(divassign, /=, "/=", true, binary_floatop(/=, true), /*none*/)
handle_binary(modassign, %=, "%=", true, /*none*/, /*none*/)
handle_binary(shrassign, >>=, ">>=", true, /*none*/, /*none*/)
handle_binary(shlassign, <<=, "<<=", true, /*none*/, /*none*/)
handle_binary(andassign, &=, "&=", true, /*none*/, /*none*/)
handle_binary(xorassign, ^=, "^=", true, /*none*/, /*none*/)
handle_binary(orassign, |=, "|=", true, /*none*/, /*none*/)

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
		result->type = type; \
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
		result->type = type; \
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
