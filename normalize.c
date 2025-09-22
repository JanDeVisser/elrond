/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "operators.h"
#include "parser.h"
#include "slice.h"
#include "type.h"
#include "value.h"

#define normalize(parser, n)                                      \
    (                                                             \
        {                                                         \
            nodeptr __normalized = node_normalize((parser), (n)); \
            if (!__normalized.ok) {                               \
                return nullptr;                                   \
            }                                                     \
            (__normalized);                                       \
        })

typedef nodeptr (*normalize_fnc)(parser_t *, nodeptr n);

nodeptr default_normalize(parser_t *parser, nodeptr n)
{
    (void) parser;
    return n;
}

void flatten_binex(parser_t *parser, nodeptr root, nodeptrs *list)
{
    node_t *node = N(root);
    if (node->node_type == NT_BinaryExpression) {
        if (node->binary_expression.op == OP_Sequence) {
            flatten_binex(parser, node->binary_expression.lhs, list);
            dynarr_append(list, node->binary_expression.rhs);
        } else {
            dynarr_append(list, root);
        }
    } else {
        dynarr_append(list, root);
    }
}

nodeptr BinaryExpression_normalize(parser_t *parser, nodeptr n)
{
    nodeptr        lhs = normalize(parser, N(n)->binary_expression.lhs);
    nodeptr        rhs = normalize(parser, N(n)->binary_expression.rhs);
    operator_t     op = N(n)->binary_expression.op;
    operator_def_t op_def = operators[op];

    node_t *expr = N(n);
    node_t *lhs_node = N(lhs);
    node_t *rhs_node = N(rhs);

    if (op_def.assignment_op_for.ok) {
        node_t  lhs_node_copy = *lhs_node;
        nodeptr bin_expr = parser_add_node(parser, NT_BinaryExpression, expr->location, .binary_expression = { .lhs = lhs, .op = op_def.assignment_op_for.value, .rhs = rhs });
        nodeptr lhs_copy = parser_append_node(parser, lhs_node_copy);
        return parser_add_node(parser, NT_BinaryExpression, expr->location, .binary_expression = { .lhs = lhs_copy, .op = OP_AddressOf, .rhs = bin_expr });
    }
    if (lhs_node->node_type == NT_Constant && rhs_node->node_type == NT_Constant) {
        opt_value_t result = evaluate(lhs_node->constant_value.value, op, rhs_node->constant_value.value);
        if (result.ok) {
            return parser_add_node(parser, NT_Constant, expr->location, .constant_value = result);
        }
    }
    if (lhs_node->node_type == NT_Constant && op == OP_Cast && rhs_node->node_type == NT_TypeSpecification) {
        nodeptr type = typespec_resolve(rhs_node->type_specification);
        if (type.ok) {
            opt_value_t result = value_coerce(lhs_node->constant_value.value, type);
            if (result.ok) {
                return parser_add_node(parser, NT_Constant, expr->location, .constant_value = result);
            }
        }
    }
    if (op == OP_Call) {
        switch (rhs_node->node_type) {
        case NT_Void: {
            rhs = parser_add_node(parser, NT_ExpressionList, rhs_node->location, .expression_list = { 0 });
        } break;
        case NT_ExpressionList:
            break;
        default: {
            nodeptrs arg_list = { 0 };
            dynarr_append(&arg_list, rhs);
            rhs = parser_add_node(parser, NT_ExpressionList, rhs_node->location, .expression_list = arg_list);
            break;
        }
        }
        return parser_add_node(parser, NT_Call, expr->location, .function_call = { .callable = lhs, .arguments = rhs });
    }
    if (op == OP_Sequence) {
        nodeptrs expressions = { 0 };
        flatten_binex(parser, n, &expressions);
        return parser_add_node(parser, NT_ExpressionList, rhs_node->location, .expression_list = expressions);
    }

    if (lhs.value != N(n)->binary_expression.lhs.value || rhs.value != N(n)->binary_expression.rhs.value) {
        return parser_add_node(
            parser,
            NT_BinaryExpression,
            N(n)->location,
            .binary_expression = { .lhs = lhs, .op = op, .rhs = rhs });
    }
    return n;
}

nodeptr BoolConstant_normalize(parser_t *parser, nodeptr n)
{
    value_t val = { .type = Boolean, .boolean = N(n)->bool_constant };
    return parser_add_node(parser, NT_Constant, N(n)->location, .constant_value = OPTVAL(value_t, val));
}

nodeptr Function_normalize(parser_t *parser, nodeptr n)
{
    nodeptr impl = normalize(parser, N(n)->function.implementation);
    nodeptr sig = normalize(parser, N(n)->function.signature);
    if (N(n)->function.implementation.value != impl.value || N(n)->function.signature.value != sig.value) {
        n = parser_add_node(
            parser,
            NT_Function,
            N(n)->location,
            .function = {
                .name = N(n)->function.name,
                .implementation = impl,
                .signature = sig,
            });
    }
    return n;
}

opt_nodeptrs normalize_block(parser_t *parser, nodeptr n, off_t offset)
{
    nodeptrs new_block = { 0 };
    size_t   sz = ((nodeptrs *) ((void *) N(n) + offset))->len;
    for (size_t ix = 0; ix < sz; ++ix) {
        nodeptr s = ((nodeptrs *) ((void *) N(n) + offset))->items[ix];
        nodeptr normalized = node_normalize(parser, s);
        if (normalized.ok) {
            dynarr_append(&new_block, normalized);
        }
    }
    if (!dynarr_eq(new_block, *((nodeptrs *) ((void *) N(n) + offset)))) {
        return OPTVAL(nodeptrs, new_block);
    }
    return OPTNULL(nodeptrs);
}

nodeptr Module_normalize(parser_t *parser, nodeptr n)
{
    opt_nodeptrs new_block = normalize_block(parser, n, offsetof(node_t, module.statements));
    if (new_block.ok) {
        node_t new_mod = *N(n);
        new_mod.module.statements = new_block.value;
        n = parser_append_node(parser, new_mod);
    }
    N(n)->namespace.ok = true;
    return n;
}

nodeptr Number_normalize(parser_t *parser, nodeptr n)
{
    opt_long v = slice_to_long(N(n)->number.number, 0);
    if (!v.ok) {
        return nullptr;
    }
    value_t val = { .type = I64, .i64 = v.value };
    return parser_add_node(parser, NT_Constant, N(n)->location, .constant_value = OPTVAL(value_t, val));
}

nodeptr Program_normalize(parser_t *parser, nodeptr n)
{
    node_t      *program = N(n);
    nodeptrs     mods = program->program.modules;
    opt_nodeptrs new_block = normalize_block(parser, n, offsetof(node_t, program.statements));

    nodeptrs new_mods_arr = { 0 };
    for (size_t ix = 0; ix < mods.len; ++ix) {
        nodeptr normalized = node_normalize(parser, mods.items[ix]);
        if (normalized.ok) {
            dynarr_append(&new_mods_arr, normalized);
        }
    }
    opt_nodeptrs new_mods = { 0 };
    if (!dynarr_eq(new_mods_arr, mods)) {
        new_mods = OPTVAL(nodeptrs, new_mods_arr);
    }

    if (new_block.ok || new_mods.ok) {
        node_t new_prog = *N(n);
        if (new_block.ok) {
            new_prog.program.statements = new_block.value;
        }
        if (new_mods.ok) {
            new_prog.program.modules = new_mods.value;
        }
        n = parser_append_node(parser, new_prog);
    }
    N(n)->namespace.ok = true;
    return n;
}

nodeptr Return_normalize(parser_t *parser, nodeptr n)
{
    nodeptr normalized = normalize(parser, N(n)->statement);
    if (normalized.value != N(n)->statement.value) {
        return parser_add_node(parser, NT_Return, N(n)->location, .statement = normalized);
    }
    return n;
}

nodeptr StatementBlock_normalize(parser_t *parser, nodeptr n)
{
    opt_nodeptrs new_block = normalize_block(parser, n, offsetof(node_t, statement_block.statements));
    if (new_block.ok) {
        node_t new_node = *N(n);
        new_node.statement_block.statements = new_block.value;
        n = parser_append_node(parser, new_node);
    }
    N(n)->namespace.ok = true;
    return n;
}

nodeptr String_normalize(parser_t *parser, nodeptr n)
{
    sb_t unescaped = { 0 };
    sb_unescape(&unescaped, N(n)->string.string);
    value_t val = { .type = String, .slice = sb_as_slice(unescaped) };
    return parser_add_node(parser, NT_Constant, N(n)->location, .constant_value = OPTVAL(value_t, val));
}

#define NORMALIZEOVERRIDES(S) \
    S(BinaryExpression)       \
    S(BoolConstant)           \
    S(Function)               \
    S(Module)                 \
    S(Number)                 \
    S(Program)                \
    S(Return)                 \
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
    node_t *node = N(ix);
    assert(node != NULL);
    bool has_ns = node->namespace.ok;
    if (has_ns) {
        dynarr_append(&parser->namespaces, ix);
    }
    printf("normalize %zu = %s\n", ix.value, node_type_name(N(ix)->node_type));
    nodeptr ret = normalize_fncs[N(ix)->node_type](parser, ix);
    if (ret.ok) {
        printf("result %zu = %s => %zu = %s\n",
            ix.value, node_type_name(N(ix)->node_type),
            ret.value, node_type_name(N(ret)->node_type));
    } else {
        printf("result %zu = %s => NULL\n",
            ix.value, node_type_name(N(ix)->node_type));
    }
    if (has_ns) {
        dynarr_pop(&parser->namespaces);
    }
    return ret;
}
