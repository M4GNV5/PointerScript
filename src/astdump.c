#include <stdio.h>
#include "ast.h"

int indentCount = 0;
void indent()
{
	for(int i = 0; i < indentCount; i++)
	{
		printf("  ");
	}
}

//struct ptrs_var_t *(*ptrs_asthandler_t)(struct ptrs_ast *node);

void dump_astlist(struct ptrs_astlist *list)
{
	while(list)
	{
		list->entry->handler(list->entry);
		list = list->next;
	}
}

ptrs_var_t *ptrs_dump_body(ptrs_ast_t *node)
{
	dump_astlist(node->arg.body.nodes);
}

ptrs_var_t *ptrs_dump_define(ptrs_ast_t *node)
{
	indent();
	printf("VariableDeclaration\n");
	indentCount++;

	indent();
	printf("name: '%s'\n", node->arg.define.name);

	ptrs_ast_t *val = node->arg.define.value;
	if(val)
	{
		indent();
		printf("value:\n");
		indentCount++;
		val->handler(val);
		indentCount--;
	}
	indentCount--;
}

ptrs_var_t *ptrs_dump_if(ptrs_ast_t *node)
{
	indent();
	printf("IfStatement\n");
	indentCount++;

	struct ptrs_ast_controlstatement control = node->arg.control;

	indent();
	printf("condition:\n");
	indentCount++;
	control.condition->handler(control.condition);
	indentCount--;

	indent();
	printf("body:\n");
	control.body->handler(control.body);
	indentCount--;

	indentCount--;
}

ptrs_var_t *ptrs_dump_exprstatement(ptrs_ast_t *node)
{
	indent();
	printf("ExpressionStatement\n");
	indentCount++;
	node->arg.astval->handler(node->arg.astval);
	indentCount--;
}

ptrs_var_t *ptrs_dump_call(ptrs_ast_t *node)
{
	indent();
	printf("CallExpression\n");
	indentCount++;

	struct ptrs_ast_call call = node->arg.call;

	indent();
	printf("value:\n");
	indentCount++;
	call.value->handler(call.value);
	indentCount--;

	indent();
	printf("arguments:\n");
	indentCount++;
	dump_astlist(call.arguments);
	indentCount--;

	indentCount--;
}

ptrs_var_t *ptrs_dump_identifier(ptrs_ast_t *node)
{
	indent();
	printf("Identifier: '%s'\n", node->arg.strval);
}

ptrs_var_t *ptrs_dump_string(ptrs_ast_t *node)
{
	indent();
	printf("String: '%s'\n", node->arg.strval);
}

ptrs_var_t *ptrs_dump_int(ptrs_ast_t *node)
{
	indent();
	printf("Integer: %d\n", node->arg.intval);
}

ptrs_var_t *ptrs_dump_float(ptrs_ast_t *node)
{
	indent();
	printf("Float: %f\n", node->arg.floatval);
}

ptrs_var_t *ptrs_dump_cast(ptrs_ast_t *node)
{
	indent();
	printf("CastExpression\n");
	indentCount++;

	struct ptrs_ast_cast cast = node->arg.cast;

	indent();
	printf("type: %d\n", cast.type);

	indent();
	printf("value:\n");
	indentCount++;
	cast.value->handler(cast.value);
	indentCount--;

	indentCount--;
}

void dump_binary(ptrs_ast_t *node, const char *op);
void dump_unary(ptrs_ast_t *node, const char *type, const char *op);

ptrs_var_t ptrs_dump_op_equal(ptrs_ast_t *node) { dump_binary(node, "=="); }
ptrs_var_t ptrs_dump_op_inequal(ptrs_ast_t *node) { dump_binary(node, "!="); }
ptrs_var_t ptrs_dump_op_lessequal(ptrs_ast_t *node) { dump_binary(node, "<="); }
ptrs_var_t ptrs_dump_op_greaterequal(ptrs_ast_t *node) { dump_binary(node, ">="); }
ptrs_var_t ptrs_dump_op_less(ptrs_ast_t *node) { dump_binary(node, "<"); }
ptrs_var_t ptrs_dump_op_greater(ptrs_ast_t *node) { dump_binary(node, ">"); }
ptrs_var_t ptrs_dump_op_assign(ptrs_ast_t *node) { dump_binary(node, "="); }
ptrs_var_t ptrs_dump_op_addassign(ptrs_ast_t *node) { dump_binary(node, "+="); }
ptrs_var_t ptrs_dump_op_subassign(ptrs_ast_t *node) { dump_binary(node, "-="); }
ptrs_var_t ptrs_dump_op_mulassign(ptrs_ast_t *node) { dump_binary(node, "*="); }
ptrs_var_t ptrs_dump_op_divassign(ptrs_ast_t *node) { dump_binary(node, "/="); }
ptrs_var_t ptrs_dump_op_modassign(ptrs_ast_t *node) { dump_binary(node, "%="); }
ptrs_var_t ptrs_dump_op_shrassign(ptrs_ast_t *node) { dump_binary(node, ">>="); }
ptrs_var_t ptrs_dump_op_shlassign(ptrs_ast_t *node) { dump_binary(node, "<<="); }
ptrs_var_t ptrs_dump_op_andassign(ptrs_ast_t *node) { dump_binary(node, "&="); }
ptrs_var_t ptrs_dump_op_xorassign(ptrs_ast_t *node) { dump_binary(node, "^="); }
ptrs_var_t ptrs_dump_op_orassign(ptrs_ast_t *node) { dump_binary(node, "|="); }
ptrs_var_t ptrs_dump_op_logicor(ptrs_ast_t *node) { dump_binary(node, "||"); }
ptrs_var_t ptrs_dump_op_logicand(ptrs_ast_t *node) { dump_binary(node, "&&"); }
ptrs_var_t ptrs_dump_op_or(ptrs_ast_t *node) { dump_binary(node, "|"); }
ptrs_var_t ptrs_dump_op_xor(ptrs_ast_t *node) { dump_binary(node, "^"); }
ptrs_var_t ptrs_dump_op_and(ptrs_ast_t *node) { dump_binary(node, "&"); }
ptrs_var_t ptrs_dump_op_shr(ptrs_ast_t *node) { dump_binary(node, ">>"); }
ptrs_var_t ptrs_dump_op_shl(ptrs_ast_t *node) { dump_binary(node, "<<"); }
ptrs_var_t ptrs_dump_op_add(ptrs_ast_t *node) { dump_binary(node, "+"); }
ptrs_var_t ptrs_dump_op_sub(ptrs_ast_t *node) { dump_binary(node, "-"); }
ptrs_var_t ptrs_dump_op_mul(ptrs_ast_t *node) { dump_binary(node, "*"); }
ptrs_var_t ptrs_dump_op_div(ptrs_ast_t *node) { dump_binary(node, "/"); }
ptrs_var_t ptrs_dump_op_mod(ptrs_ast_t *node) { dump_binary(node, "%"); }

const char *prefixExpression = "PrefixExpression";
ptrs_var_t *ptrs_dump_prefix_inc(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "++"); }
ptrs_var_t *ptrs_dump_prefix_dec(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "--"); }
ptrs_var_t *ptrs_dump_prefix_logicnot(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "!"); }
ptrs_var_t *ptrs_dump_prefix_not(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "~"); }
ptrs_var_t *ptrs_dump_prefix_address(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "&"); }
ptrs_var_t *ptrs_dump_prefix_dereference(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "*"); }
ptrs_var_t *ptrs_dump_prefix_plus(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "+"); }
ptrs_var_t *ptrs_dump_prefix_minus(ptrs_ast_t *node) { dump_unary(node, prefixExpression, "-"); }

const char *suffixExpression = "SuffixExpression";
ptrs_var_t *ptrs_dump_suffix_inc(ptrs_ast_t *node) { dump_unary(node, suffixExpression, "++"); }
ptrs_var_t *ptrs_dump_suffix_dec(ptrs_ast_t *node) { dump_unary(node, suffixExpression, "--"); }

void dump_binary(ptrs_ast_t *node, const char *op)
{
	indent();
	printf("BinaryExpression\n");
	indentCount++;

	indent();
	printf("operator: %s\n", op);

	indent();
	printf("left:\n");
	indentCount++;
	node->arg.binary.left->handler(node->arg.binary.left);
	indentCount--;

	indent();
	printf("right:\n");
	indentCount++;
	node->arg.binary.right->handler(node->arg.binary.right);
	indentCount--;

	indentCount--;
}

void dump_unary(ptrs_ast_t *node, const char *type, const char *op)
{
	indent();
	printf("%s\n", type);
	indentCount++;

	indent();
	printf("operator: %s\n", op);

	indent();
	printf("value:\n");
	indentCount++;
	node->arg.astval->handler(node->arg.astval);
	indentCount--;

	indentCount--;
}
