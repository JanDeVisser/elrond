/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "operators.h"
#include "slice.h"

#define normalize(tree, n)                                      \
    (                                                           \
        {                                                       \
            nodeptr __normalized = node_normalize((tree), (n)); \
            if (!__normalized.ok) {                             \
                return nullptr;                                 \
            }                                                   \
            (__normalized);                                     \
        })

typedef nodeptr (*normalize_fnc)(nodes_t *, node_t *n);

nodeptr nodes_add_node(nodes_t *this, node_t n)
{
    n.ix = this->len;
    dynarr_append(this, n);
    return nodeptr_ptr(n.ix);
}

nodeptr default_normalize(nodes_t *tree, node_t *n)
{
    (void) tree;
    return nodeptr_ptr(n->ix);
}

nodeptr BinaryExpression_normalize(nodes_t *tree, node_t *n)
{
    nodeptr lhs = normalize(tree, n->binary_expression.lhs);
    nodeptr rhs = normalize(tree, n->binary_expression.rhs);
    if (lhs.value != n->binary_expression.lhs.value || rhs.value != n->binary_expression.rhs.value) {
        node_t new_node = *n;
        new_node.binary_expression.lhs = lhs;
        new_node.binary_expression.lhs = rhs;
        return nodes_add_node(tree, new_node);
    }
    return nodeptr_ptr(n->ix);
}

nodeptr Function_normalize(nodes_t *tree, node_t *n)
{
    nodeptr impl = normalize(tree, n->function.implementation);
    nodeptr sig = normalize(tree, n->function.signature);
    if (n->function.implementation.value != impl.value || n->function.signature.value != sig.value) {
        node_t new_node = *n;
        new_node.function.implementation = impl;
        new_node.function.signature = sig;
        return nodes_add_node(tree, new_node);
    }
    return nodeptr_ptr(n->ix);
}

nodeptr normalize_block(nodes_t *tree, node_t *n, off_t offset)
{
    nodeptrs  new_block = { 0 };
    nodeptrs *current_block = (nodeptrs *) (((void *) n) + offset);
    for (size_t ix = 0; ix < current_block->len; ++ix) {
        nodeptr normalized = node_normalize(tree, current_block->items[ix]);
        if (normalized.ok) {
            dynarr_append(&new_block, normalized);
        }
    }
    if (!dynarr_eq(new_block, *current_block)) {
        node_t new_node = *n;
        *(nodeptrs *) (((void *) &new_node) + offset) = new_block;
        return nodes_add_node(tree, new_node);
    }
    return nodeptr_ptr(n->ix);
}

nodeptr Module_normalize(nodes_t *tree, node_t *n)
{
    return normalize_block(tree, n, offsetof(node_t, statement_block.statements));
}

nodeptr StatementBlock_normalize(nodes_t *tree, node_t *n)
{
    return normalize_block(tree, n, offsetof(node_t, statement_block.statements));
}

#define NORMALIZEOVERRIDES(S) \
    S(BinaryExpression)       \
    S(Module)                 \
    S(Function)               \
    S(StatementBlock)

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

nodeptr node_normalize(nodes_t *tree, nodeptr ix)
{
    if (!normalize_initialized) {
        initialize_normalize();
    }
    node_t *n = tree->items + ix.value;
    return normalize_fncs[n->node_type](tree, n);
}
