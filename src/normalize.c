/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreter.h"
#include "ir.h"
#include "node.h"
#include "operators.h"
#include "parser.h"
#include "slice.h"
#include "type.h"
#include "value.h"

#define normalize(parser, n)                                    \
    (                                                           \
        {                                                       \
            nodeptr normalized = node_normalize((parser), (n)); \
            if (!normalized.ok) {                               \
                return nullptr;                                 \
            }                                                   \
            normalized;                                         \
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
    operator_t      op = N(n)->binary_expression.op;
    tokenlocation_t location = N(n)->location;
    nodeptr         lhs = normalize(parser, N(n)->binary_expression.lhs);
    nodeptr         rhs = normalize(parser, N(n)->binary_expression.rhs);
    operator_def_t  op_def = operators[op];

    if (op_def.assignment_op_for.ok) {
        nodeptr bin_expr = parser_add_node(
            parser,
            NT_BinaryExpression,
            location,
            .binary_expression = {
                .lhs = lhs,
                .op = op_def.assignment_op_for.value,
                .rhs = rhs,
            });
        nodeptr lhs_copy = parser_append_node(parser, *N(lhs));
        return parser_add_node(
            parser,
            NT_BinaryExpression,
            location,
            .binary_expression = {
                .lhs = lhs_copy,
                .op = OP_Assign,
                .rhs = bin_expr,
            });
    }
    if (N(lhs)->node_type == NT_Constant
        && op == OP_Cast
        && N(rhs)->node_type == NT_TypeSpecification) {
        nodeptr type = typespec_resolve(N(rhs)->type_specification);
        if (type.ok) {
            opt_value_t result = value_coerce(N(lhs)->constant_value.value, type);
            if (result.ok) {
                return parser_add_node(
                    parser,
                    NT_Constant, location,
                    .constant_value = result);
            }
        }
    }
    if (N(lhs)->node_type == NT_Constant && N(rhs)->node_type == NT_Constant) {
        opt_value_t result = evaluate(
            N(lhs)->constant_value.value,
            op,
            N(rhs)->constant_value.value);
        if (result.ok) {
            return parser_add_node(
                parser,
                NT_Constant,
                location,
                .constant_value = result);
        }
    }
    if (op == OP_Call) {
        switch (N(rhs)->node_type) {
        case NT_Void: {
            rhs = parser_add_node(
                parser,
                NT_ExpressionList,
                N(rhs)->location,
                .expression_list = { 0 });
        } break;
        case NT_ExpressionList:
            break;
        default: {
            nodeptrs arg_list = { 0 };
            dynarr_append(&arg_list, rhs);
            rhs = parser_add_node(
                parser,
                NT_ExpressionList,
                N(rhs)->location,
                .expression_list = arg_list);
            break;
        }
        }
        return parser_add_node(
            parser,
            NT_Call,
            location,
            .function_call = { .callable = lhs, .arguments = rhs });
    }
    if (op == OP_Sequence) {
        nodeptrs expressions = { 0 };
        flatten_binex(parser, n, &expressions);
        return parser_add_node(
            parser,
            NT_ExpressionList,
            N(rhs)->location,
            .expression_list = expressions);
    }

    if (lhs.value != N(n)->binary_expression.lhs.value
        || rhs.value != N(n)->binary_expression.rhs.value) {
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

nodeptr Comptime_normalize(parser_t *parser, nodeptr n)
{
    nodeptr stmts = parse_snippet(parser, N(n)->comptime.raw_text);
    stmts = normalize(parser, stmts);
    N(n)->comptime.statements = stmts;
    return n;
}

nodeptr Function_normalize(parser_t *parser, nodeptr n)
{
    node_t *node = N(n);
    nodeptr impl = normalize(parser, node->function.implementation);
    node = N(n);
    nodeptr sig = normalize(parser, node->function.signature);
    if (node->function.implementation.value != impl.value
        || node->function.signature.value != sig.value) {
        n = parser_add_node(
            parser,
            NT_Function,
            node->location,
            .function = {
                .name = node->function.name,
                .implementation = impl,
                .signature = sig,
            });
    }
    return n;
}

opt_nodeptrs normalize_block(parser_t *parser, nodeptr n, off_t offset)
{
    nodeptrs new_block = { 0 };
    size_t   len = ((nodeptrs *) ((void *) N(n) + offset))->len;
    for (size_t ix = 0; ix < len; ++ix) {
        nodeptr normalized = node_normalize(parser, ((nodeptrs *) ((void *) N(n) + offset))->items[ix]);
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
    trace("--> StatementBlock len: %zu", N(n)->statement_block.statements.len);
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
    sb_t    unescaped = { 0 };
    node_t *node = N(n);
    sb_unescape(&unescaped, slice_sub_by_length(node->string.string, 1, node->string.string.len - 2));
    value_t val = { .type = String, .slice = sb_as_slice(unescaped) };
    return parser_add_node(parser, NT_Constant, N(n)->location, .constant_value = OPTVAL(value_t, val));
}

nodeptr VariableDeclaration_normalize(parser_t *parser, nodeptr n)
{
    nodeptr initializer = nullptr;
    if (N(n)->variable_declaration.initializer.ok) {
        initializer = normalize(parser, N(n)->variable_declaration.initializer);
    }
    if (initializer.ok != N(n)->variable_declaration.initializer.ok || initializer.value != N(n)->variable_declaration.initializer.value) {
        variable_declaration_t decl = N(n)->variable_declaration;
        decl.initializer = initializer;
        return parser_add_node(
            parser,
            NT_VariableDeclaration,
            N(n)->location,
            .variable_declaration = decl);
    }
    return n;
}

#define NORMALIZEOVERRIDES(S) \
    S(BinaryExpression)       \
    S(BoolConstant)           \
    S(Comptime)               \
    S(Function)               \
    S(Module)                 \
    S(Number)                 \
    S(Program)                \
    S(Return)                 \
    S(StatementBlock)         \
    S(String)                 \
    S(VariableDeclaration)

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
    trace("normalize %zu = %s", ix.value, node_type_name(N(ix)->node_type));
    nodeptr ret = normalize_fncs[N(ix)->node_type](parser, ix);
    if (ret.ok) {
        trace("result %zu = %s => %zu = %s",
            ix.value, node_type_name(N(ix)->node_type),
            ret.value, node_type_name(N(ret)->node_type));
    } else {
        trace("result %zu = %s => NULL",
            ix.value, node_type_name(N(ix)->node_type));
    }
    if (has_ns) {
        dynarr_pop(&parser->namespaces);
    }
    return ret;
}
