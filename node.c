/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "operators.h"
#include "slice.h"
#include "type.h"
#include "value.h"

char const *node_type_name(nodetype_t type)
{
    switch (type) {
#undef S
#define S(T)     \
    case NT_##T: \
        return #T;
        NODETYPES(S)
#undef S
    default:
        UNREACHABLE();
    }
}

void default_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) n;
    (void) indent;
    fprintf(f, "\n");
}

void BinaryExpression_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "%s\n", operator_name(n->binary_expression.op));
    node_print(f, "LHS", tree, n->binary_expression.lhs, indent + 4);
    node_print(f, "RHS", tree, n->binary_expression.rhs, indent + 4);
}

void BoolConstant_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, "%s\n", (n->bool_constant) ? "true" : "false");
}

void Break_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    if (n->label.ok) {
        fprintf(f, SL, SLARG(n->label.value));
    }
}

void Continue_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    if (n->label.ok) {
        fprintf(f, SL, SLARG(n->label.value));
    }
    fprintf(f, "\n");
}

void Call_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "\n");
    node_print(f, "Callable", tree, n->function_call.callable, indent + 4);
    node_print(f, "Arguments", tree, n->function_call.arguments, indent + 4);
}

void Constant_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    if (n->constant_value.ok) {
        value_print(f, n->constant_value.value);
    }
    fprintf(f, "\n");
}

void Defer_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "\n");
    node_print(f, NULL, tree, n->statement, indent + 4);
}

void Embed_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->identifier));
}

void ExpressionList_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "%zu\n", n->expression_list.len);
    char buf[32];
    for (size_t ix = 0; ix < n->expression_list.len; ++ix) {
        snprintf(buf, 31, "Param %zu", ix);
	node_print(f, buf, tree, n->expression_list.items[ix], indent + 4);
    }        
}

void Enum_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, SL, SLARG(n->enumeration.name));
    /* if (n->enumeration.underlying.ok) { */
    /* 	slice_t type_name = type_string(tree, n->enumeration.underlying.value);
     */
    /* 	fprintf(f, ": " SL, SLARG(type_name)); */
    /* } */
    fprintf(f, "\n");
    for (size_t ix = 0; ix < n->enumeration.values.len; ++ix) {
        node_print(f, NULL, tree, n->enumeration.values.items[ix], indent + 4);
    }
}

void EnumValue_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->variable_declaration.name));
}

void ForeignFunction_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->identifier));
}

void Function_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, SL "\n", SLARG(n->function.name));
    node_print(f, "Sig", tree, n->function.signature, indent + 4);
    node_print(f, "Impl", tree, n->function.implementation, indent + 4);
}

void Identifier_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->identifier));
}

void Module_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "\n");
    for (size_t ix = 0; ix < n->statement_block.statements.len; ++ix) {
        node_print(f, NULL, tree, n->statement_block.statements.items[ix],
            indent + 4);
    }
}

void Number_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->number.number));
}

void Parameter_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) indent;
    fprintf(f, SL ": " SL, SLARG(n->variable_declaration.name), SLARG(typespec_to_string(tree, n->variable_declaration.type)));
    fprintf(f, "\n");
}

void Return_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "\n");
    node_print(f, NULL, tree, n->statement, indent + 4);
}

void Signature_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "func() ");
    fprintf(f, SL, SLARG(typespec_to_string(tree, n->signature.return_type)));
    fprintf(f, "\n");
    for (size_t ix = 0; ix < n->signature.parameters.len; ++ix) {
        node_print(f, "Param", tree, n->signature.parameters.items[ix], indent + 4);
    }
}

void StatementBlock_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    fprintf(f, "\n");
    for (size_t ix = 0; ix < n->statement_block.statements.len; ++ix) {
        node_print(f, NULL, tree, n->statement_block.statements.items[ix],
            indent + 4);
    }
}

void String_print(FILE *f, nodes_t tree, node_t *n, int indent)
{
    (void) tree;
    (void) indent;
    fprintf(f, SL "\n", SLARG(n->string.string));
}

#define PRINTOVERRIDES(S) \
    S(BinaryExpression)   \
    S(BoolConstant)       \
    S(Break)              \
    S(Call)               \
    S(Constant)           \
    S(Continue)           \
    S(Defer)              \
    S(Embed)              \
    S(Enum)               \
    S(EnumValue)          \
    S(ExpressionList)     \
    S(ForeignFunction)    \
    S(Module)             \
    S(Number)             \
    S(Parameter)          \
    S(Return)             \
    S(Function)           \
    S(Identifier)         \
    S(Signature)          \
    S(StatementBlock)     \
    S(String)

typedef void (*print_fnc)(FILE *, nodes_t, node_t *, int);

static bool      print_initialized = false;
static print_fnc print_fncs[] = {
#undef S
#define S(T) [NT_##T] = default_print,
    NODETYPES(S)
#undef S
};

void initialize_print()
{
#undef S
#define S(T) print_fncs[NT_##T] = T##_print;
    PRINTOVERRIDES(S)
#undef S
    print_initialized = true;
}

void node_print(FILE *f, char const *prefix, nodes_t tree, nodeptr ix,
    int indent)
{
    if (!print_initialized) {
        initialize_print();
    }

    fprintf(f, "%4zu. ", ix.value);
    if (indent > 0) {
        fprintf(f, "%*s", indent, "");
    }
    if (prefix != NULL) {
        fprintf(f, "%s: ", prefix);
    }
    node_t *n = tree.items + ix.value;
    fprintf(f, "%4zu:%3zu %.*s | ",
        n->location.line + 1,
        n->location.column + 1,
        SLARG(C(node_type_name(n->node_type))));
    if (n->bound_type.ok) {
        fprintf(f, SL " | ", SLARG(type_to_string(n->bound_type)));
    }
    print_fncs[n->node_type](f, tree, n, indent);
}
