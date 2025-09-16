/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "operators.h"
#include "parser.h"
#include "slice.h"
#include "type.h"
#include "value.h"

#define normalize(tree, n)                                      \
    (                                                           \
        {                                                       \
            nodeptr __normalized = node_normalize((tree), (n)); \
            if (!__normalized.ok) {                             \
                return nullptr;                                 \
            }                                                   \
            (__normalized);                                     \
        })

typedef nodeptr (*normalize_fnc)(parser_t *, node_t *n);

nodeptr default_normalize(parser_t *parser, node_t *n)
{
    (void) parser;
    return nodeptr_ptr(n->ix);
}

nodeptr BinaryExpression_normalize(parser_t *parser, node_t *n)
{
    nodeptr lhs = normalize(parser, n->binary_expression.lhs);
    nodeptr rhs = normalize(parser, n->binary_expression.rhs);
    if (lhs.value != n->binary_expression.lhs.value || rhs.value != n->binary_expression.rhs.value) {
        node_t new_node = *n;
        new_node.binary_expression.lhs = lhs;
        new_node.binary_expression.rhs = rhs;
        return parser_append_node(parser, new_node);
    }
    return nodeptr_ptr(n->ix);
}

nodeptr BoolConstant_normalize(parser_t *parser, node_t *n)
{
    value_t val = { .type = Boolean, .boolean = n->bool_constant };
    return parser_copy_node(parser, *n, NT_Constant, .constant_value = OPTVAL(value_t, val));
}

nodeptr Function_normalize(parser_t *parser, node_t *n)
{
    nodeptr impl = normalize(parser, n->function.implementation);
    nodeptr sig = normalize(parser, n->function.signature);
    if (n->function.implementation.value != impl.value || n->function.signature.value != sig.value) {
        node_t new_node = *n;
        new_node.function.implementation = impl;
        new_node.function.signature = sig;
        return parser_copy_node(parser, *n, NT_Function, .function = { .implementation = impl, .signature = sig });
    }
    return nodeptr_ptr(n->ix);
}

nodeptr normalize_block(parser_t *parser, node_t *n, off_t offset)
{
    nodeptrs  new_block = { 0 };
    nodeptrs *current_block = (nodeptrs *) (((void *) n) + offset);
    for (size_t ix = 0; ix < current_block->len; ++ix) {
        nodeptr normalized = node_normalize(parser, current_block->items[ix]);
        if (normalized.ok) {
            dynarr_append(&new_block, normalized);
        }
    }
    if (!dynarr_eq(new_block, *current_block)) {
        node_t new_node = *n;
        *(nodeptrs *) (((void *) &new_node) + offset) = new_block;
        return parser_append_node(parser, new_node);
    }
    return nodeptr_ptr(n->ix);
}

nodeptr Module_normalize(parser_t *parser, node_t *n)
{
    return normalize_block(parser, n, offsetof(node_t, statement_block.statements));
}

nodeptr Number_normalize(parser_t *parser, node_t *n)
{
    opt_long v = slice_to_long(n->number.number, 0);
    if (!v.ok) {
        return nullptr;
    }
    value_t val = { .type = I64, .i64 = v.value };
    return parser_copy_node(parser, *n, NT_Constant, .constant_value = OPTVAL(value_t, val));
}

nodeptr StatementBlock_normalize(parser_t *parser, node_t *n)
{
    return normalize_block(parser, n, offsetof(node_t, statement_block.statements));
}

nodeptr String_normalize(parser_t *parser, node_t *n)
{
    sb_t unescaped = { 0 };
    sb_unescape(&unescaped, n->string.string);
    value_t val = { .type = String, .slice = sb_as_slice(unescaped) };
    return parser_copy_node(parser, *n, NT_Constant, .constant_value = OPTVAL(value_t, val));
}

#define NORMALIZEOVERRIDES(S) \
    S(BinaryExpression)       \
    S(BoolConstant)           \
    S(Function)               \
    S(Module)                 \
    S(Number)                 \
    S(StatementBlock)         \
    S(String)

static bool          normalize_initialized = false;
static normalize_fnc normalize_fncs[] = {
#undef S
#define S(T) [NT_##T] = default_normalize,
    NODETYPES(S)
#undef S
};

void initialize_normalize()
{
#undef S
#define S(T) normalize_fncs[NT_##T] = T##_normalize;
    NORMALIZEOVERRIDES(S)
#undef S
    normalize_initialized = true;
}

nodeptr node_normalize(parser_t *parser, nodeptr ix)
{
    if (!normalize_initialized) {
        initialize_normalize();
    }
    node_t *n = parser->nodes.items + ix.value;
    return normalize_fncs[n->node_type](parser, n);
}
