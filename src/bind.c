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

#define node_type(node)                          \
    (                                            \
        {                                        \
            nodeptr __v = N((node))->bound_type; \
            __v;                                 \
        });

typedef enum _operand_kind {
    OPK_Type,
    OPK_Pseudo,
} operand_kind_t;

typedef enum _pseudo_type {
    PST_Any,
    PST_Int,
    PST_Lhs,
    PST_Number,
    PST_Rhs,
    PST_Type,
} pseudo_type_t;

typedef struct _operand {
    operand_kind_t kind;
    union {
        nodeptr       type;
        pseudo_type_t pseudo_type;
    };
} operand_t;

#define Pseudo_Int { .kind = OPK_Pseudo, .pseudo_type = PST_Int }
#define Pseudo_Number { .kind = OPK_Pseudo, .pseudo_type = PST_Number }
#define Pseudo_Lhs { .kind = OPK_Pseudo, .pseudo_type = PST_Lhs }
#define Pseudo_Type { .kind = OPK_Pseudo, .pseudo_type = PST_Type }
#define Operand_Type(T) { .kind = OPK_Type, .type = OPTVAL(size_t, (IX_##T)) }

typedef struct _operator_bind_map {
    operator_t op;
    operand_t  lhs;
    operand_t  rhs;
    operand_t  result;
} operator_bind_map_t;

OPTDEF(operator_bind_map_t);

operator_bind_map_t operator_bind_map[] = {
    { .op = OP_Add, .lhs = Pseudo_Number, .rhs = Pseudo_Lhs, .result = Pseudo_Lhs },
    { .op = OP_Add, .lhs = { .kind = OPK_Type, .type = OPTVAL(size_t, IX_StringBuilder) }, .rhs = Pseudo_Lhs, .result = Pseudo_Lhs },
    { .op = OP_Add, .lhs = Operand_Type(StringBuilder), .rhs = Operand_Type(String), .result = Pseudo_Lhs },
    { .op = OP_Divide, .lhs = Pseudo_Number, .rhs = Pseudo_Lhs, .result = Pseudo_Lhs },
    { .op = OP_Multiply, .lhs = Pseudo_Number, .rhs = Pseudo_Lhs, .result = Pseudo_Lhs },
    { .op = OP_Multiply, .lhs = Operand_Type(StringBuilder), .rhs = Pseudo_Int, .result = Pseudo_Lhs },
    { .op = OP_Subtract, .lhs = Pseudo_Number, .rhs = Pseudo_Lhs, .result = Pseudo_Lhs },
};
#define BIND_MAP_SIZE (sizeof(operator_bind_map) / sizeof(operator_bind_map_t))

#define bind(parser, n)                             \
    ({                                              \
        nodeptr __bound = node_bind((parser), (n)); \
        if (!__bound.ok) {                          \
            return nullptr;                         \
        }                                           \
        (__bound);                                  \
    })

#define parser_bind_error(parser, loc, fmt, ...)                 \
    (                                                            \
        {                                                        \
            parser_error((parser), (loc), (fmt), ##__VA_ARGS__); \
            nullptr;                                             \
        })

typedef nodeptr (*bind_fnc)(parser_t *, nodeptr n);

nodeptr default_bind(parser_t *parser, nodeptr n)
{
    (void) parser;
    (void) n;
    return nullptr;
}

bool match_operand(operand_t operand, nodeptr type, nodeptr lhs)
{
    switch (operand.kind) {
    case OPK_Type:
        return operand.type.value == type.value;
    case OPK_Pseudo:
        switch (operand.pseudo_type) {
        case PST_Any:
            return true;
        case PST_Int:
            return type_is_int(type);
        case PST_Number:
            return type_is_number(type);
        case PST_Lhs:
            assert(lhs.ok);
            return lhs.value == type.value;
        default:
            UNREACHABLE();
        }
    }
}

nodeptr match_operator(nodeptr lhs, operator_t op, nodeptr rhs)
{
    for (size_t ix = 0; ix < BIND_MAP_SIZE; ++ix) {
        operator_bind_map_t entry = operator_bind_map[ix];
        if (entry.op != op) {
            continue;
        }
        if (match_operand(entry.lhs, lhs, nullptr) && match_operand(entry.rhs, rhs, lhs)) {
            switch (entry.result.kind) {
            case OPK_Type:
                return entry.result.type;
            case OPK_Pseudo:
                switch (entry.result.pseudo_type) {
                case PST_Lhs:
                    return lhs;
                case PST_Rhs:
                    return lhs;
                default:
                    UNREACHABLE();
                }
            }
        }
    }
    return nullptr;
}

nodeptr bind_block(parser_t *parser, nodeptr n, off_t offset)
{
    size_t  sz = ((nodeptrs *) ((void *) parser_node(parser, n) + offset))->len;
    bool    all_bound = true;
    nodeptr ret = Void;
    for (size_t ix = 0; ix < sz; ++ix) {
        nodeptr s = ((nodeptrs *) ((void *) parser_node(parser, n) + offset))->items[ix];
        ret = node_bind(parser, s);
        all_bound = all_bound && ret.ok;
    }
    return (all_bound) ? ret : nullptr;
}

nodeptr BinaryExpression_bind(parser_t *parser, nodeptr n)
{
    tokenlocation_t location = N(n)->location;
    operator_t      op = N(n)->binary_expression.op;
    nodeptr         lhs = N(n)->binary_expression.lhs;
    nodeptr         rhs = N(n)->binary_expression.rhs;
    nodeptr         lhs_type = bind(parser, N(n)->binary_expression.lhs);
    nodeptr         lhs_value_type = node_value_type(N(n)->binary_expression.lhs);

    if (op == OP_MemberAccess) {
        if (type_kind(lhs_type) != TYPK_ReferenceType) {
            return parser_bind_error(
                parser,
                location,
                "Left hand side of member access operator must be value reference");
        }
        type_t *ref = get_type(lhs_type);
        if (type_kind(ref->referencing) != TYPK_StructType) {
            return parser_bind_error(
                parser,
                location,
                "Left hand side of member access operator must have struct type");
        }
        if (NT(rhs) != NT_Identifier) {
            return parser_bind_error(
                parser,
                location,
                "Right hand side of member access operator must be identifier");
        } else {
            slice_t id = N(rhs)->identifier.id;
            nodeptr s = ref->referencing;
            assert(type_kind(s) == TYPK_StructType);
            type_t *strukt = get_type(s);
            for (size_t ix = 0; ix < strukt->struct_fields.len; ++ix) {
                if (slice_eq(id, strukt->struct_fields.items[ix].name)) {
                    N(n)->bound_type = strukt->struct_fields.items[ix].type;
                    return n;
                }
            }
            return parser_bind_error(
                parser,
                location,
                "Unknown field `" SL "`", SLARG(id));
        }
    }
    bind(parser, rhs);
    nodeptr rhs_value_type = node_value_type(rhs);

    if (op == OP_Assign) {
        if (type_kind(lhs_type) != TYPK_ReferenceType) {
            return parser_bind_error(
                parser, location, "Cannot assign to non-references");
        }
        if (lhs_value_type.value != rhs_value_type.value) {
            return parser_bind_error(
                parser,
                location,
                "Cannot assign a value of type `" SL "` to a variable of type `" SL "`",
                SLARG(type_to_string(rhs_value_type)),
                SLARG(type_to_string(lhs_value_type)));
        }
        N(n)->bound_type = lhs_type;
        return n;
    }

    if (op == OP_Cast) {
        if (NT(lhs) == NT_Constant && NT(rhs) == NT_TypeSpecification) {
            if (rhs_value_type.ok) {
                if (value_coerce(N(lhs)->constant_value.value, rhs_value_type).ok) {
                    return rhs_value_type;
                }
            }
        }
        type_t *lhs_type_ptr = get_type(lhs_value_type);
        type_t *rhs_type_ptr = get_type(rhs_value_type);
        if (lhs_type_ptr->kind == TYPK_IntType && rhs_type_ptr->kind == TYPK_IntType) {
            if (lhs_type_ptr->int_type.width_bits > rhs_type_ptr->int_type.width_bits) {
                return parser_bind_error(
                    parser,
                    location,
                    "Invalid argument type. Cannot narrow integers");
            }
            return rhs_value_type;
        }
        if (lhs_type_ptr->kind == TYPK_SliceType && rhs_type_ptr->kind == TYPK_ZeroTerminatedArray) {
            if (lhs_type_ptr->slice_of.value != U8.value || rhs_type_ptr->array_of.value != U8.value) {
                return parser_bind_error(
                    parser,
                    location,
                    "Invalid argument type. Cannot cast slices to zero-terminated arrays except for strings");
            }
            return rhs_value_type;
        }
        return parser_bind_error(
            parser,
            location,
            "Invalid argument type. Can only cast integers");
    }

    nodeptr res_type = match_operator(lhs_value_type, op, rhs_value_type);
    if (res_type.ok) {
        return res_type;
    }
    return parser_bind_error(
        parser,
        location,
        "Operator `%s` cannot be applied to left hand type `" SL "` and right hand type `" SL "`",
        operator_name(op),
        SLARG(type_to_string(lhs_value_type)),
        SLARG(type_to_string(rhs_value_type)));
}

nodeptr Comptime_bind(parser_t *parser, nodeptr n)
{
    node_t *node = N(n);
    size_t  bound = parser->bound;
    nodeptr stmts = node->comptime.statements;
    do {
        parser->bound = 0;
        node_bind(parser, stmts);
        bound += parser->bound;
    } while (!parser_bound_type(parser, stmts).ok && parser->bound != 0);
    parser->bound = bound;
    if (!parser_bound_type(parser, stmts).ok) {
        return nullptr;
    }
    if (!node->comptime.output.ok) {
        ir_generator_t gen = generate_ir(parser, stmts);
        value_t        output = execute_ir(&gen, nodeptr_ptr(0));
        if (output.type.value == String.value) {
            node->comptime.output = OPTVAL(slice_t, output.slice);
        } else {
            node->comptime.output = OPTVAL(slice_t, (slice_t) { 0 });
        }
    }
    if (node->comptime.output.value.len > 0) {
        nodeptr inc = parse_snippet(parser, node->comptime.output.value);
        if (inc.ok) {
            if (inc.value != n.value) {
                parser->nodes.items[n.value] = *N(inc);
                if (inc.value == parser->nodes.len - 1) {
                    dynarr_pop(&parser->nodes);
                }
            }
            nodeptr normalized = node_normalize(parser, n);
            if (!normalized.ok) {
                return nullptr;
            }
            if (normalized.value != n.value) {
                parser->nodes.items[n.value] = *N(normalized);
                if (normalized.value == parser->nodes.len - 1) {
                    dynarr_pop(&parser->nodes);
                }
            }
            return node_bind(parser, n);
        }
    }
    return Void;
}

nodeptr Constant_bind(parser_t *parser, nodeptr n)
{
    return N(n)->constant_value.value.type;
}

nodeptr Call_bind(parser_t *parser, nodeptr n)
{
    node_t *node = N(n);
    type_t *sig = get_type(bind(parser, node->function_call.callable));
    if (sig->kind != TYPK_Signature) {
        return parser_bind_error(
            parser,
            node->location,
            "`" SL "` not callable",
            SLARG(N(node->function_call.callable)->identifier.id)); // FIXME error message
    }
    type_t *args = get_type(bind(parser, node->function_call.arguments));
    assert(args->kind == TYPK_TypeList);
    size_t ix;
    for (ix = 0; ix < MIN(args->type_list_types.len, sig->signature_type.parameters.len); ++ix) {
        if (args->type_list_types.items[ix].value != sig->signature_type.parameters.items[ix].value) {
            return parser_bind_error(
                parser,
                node->location,
                "Type mismatch for parameter `" SL "` of function `" SL "`",
                SLARG(N(node->function_call.arguments)->variable_declaration.name),
                SLARG(N(node->function_call.callable)->identifier.id)); // FIXME error message
        }
    }
    if (ix > sig->signature_type.parameters.len) {
        return parser_bind_error(
            parser,
            node->location,
            "Too many parameters for function `" SL "`",
            SLARG(N(node->function_call.callable)->identifier.id)); // FIXME error message
    } else if (ix > args->type_list_types.len) {
        node_t *param = N(sig->signature_type.parameters.items[ix]);
        return parser_bind_error(
            parser,
            node->location,
            "Missing parameter `" SL "` for function `" SL "`",
            SLARG(param->variable_declaration.name),
            SLARG(N(node->function_call.callable)->identifier.id)); // FIXME error message
    }
    node_t *callable = N(node->function_call.callable);
    if (callable->node_type == NT_Identifier) {
        node->function_call.declaration = callable->identifier.declaration;
    } else {
        fatal("TODO: callable with node type `%s` in NT_Call node", node_type_name(callable->node_type));
    }
    return sig->signature_type.result;
}

nodeptr ExpressionList_bind(parser_t *parser, nodeptr n)
{
    nodeptrs types = { 0 };
    node_t  *node = N(n);
    for (size_t ix = 0; ix < node->expression_list.len; ++ix) {
        bind(parser, node->expression_list.items[ix]);
        dynarr_append(&types, N(node->expression_list.items[ix])->bound_type);
    }
    return typelist_of(types);
}

nodeptr ForeignFunction_bind(parser_t *parser, nodeptr n)
{
    (void) parser;
    (void) n;
    return Void;
}

nodeptr Function_bind(parser_t *parser, nodeptr n)
{
    nodeptr sig = bind(parser, N(n)->function.signature);
    node_t *node = N(n);
    if (!node->namespace.ok) {
        parser_add_name(parser, node->function.name, sig, n);
        node->namespace.ok = true;
        dynarr_append(&parser->namespaces, n);
    }
    bind(parser, node->function.implementation);
    node = N(n);
    node_t *impl = N(node->function.implementation);
    type_t *sig_type = get_type(sig);
    assert(sig_type->kind == TYPK_Signature);
    nodeptr result_type = sig_type->signature_type.result;
    if (impl->node_type != NT_ForeignFunction) {
        if (impl->bound_type.value != result_type.value) {
            return parser_bind_error(
                parser,
                node->location,
                "Contradicting result types");
        }
    }
    return sig;
}

nodeptr Identifier_bind(parser_t *parser, nodeptr n)
{
    opt_name_t name = parser_resolve(parser, N(n)->identifier.id);
    if (name.ok) {
        N(n)->identifier.declaration = name.value.declaration;
        return name.value.type;
    }
    return nullptr;
}

nodeptr Module_bind(parser_t *parser, nodeptr n)
{
    // TODO Module type should be type of last non-function stmt
    return bind_block(parser, n, offsetof(node_t, module.statements));
}

nodeptr Parameter_bind(parser_t *parser, nodeptr n)
{
    bind(parser, N(n)->variable_declaration.type);
    return N(N(n)->variable_declaration.type)->bound_type;
}

nodeptr Program_bind(parser_t *parser, nodeptr n)
{
    for (size_t ix = 0; ix < N(n)->program.modules.len; ++ix) {
        bind(parser, N(n)->program.modules.items[ix]);
    }
    return bind_block(parser, n, offsetof(node_t, program.statements));
}

nodeptr Return_bind(parser_t *parser, nodeptr n)
{
    return bind(parser, parser_node(parser, n)->statement);
}

nodeptr Signature_bind(parser_t *parser, nodeptr n)
{
    nodeptrs parameters = { 0 };
    for (size_t ix = 0; ix < N(n)->signature.parameters.len; ++ix) {
        bind(parser, N(n)->signature.parameters.items[ix]);
        dynarr_append(&parameters, N(N(n)->signature.parameters.items[ix])->bound_type);
    }
    bind(parser, N(n)->signature.return_type);
    return signature(parameters, N(N(n)->signature.return_type)->bound_type);
}

nodeptr StatementBlock_bind(parser_t *parser, nodeptr n)
{
    return bind_block(parser, n, offsetof(node_t, statement_block.statements));
}

nodeptr TypeSpecification_bind(parser_t *parser, nodeptr n)
{
    return typespec_resolve(N(n)->type_specification);
}

#define BINDOVERRIDES(S) \
    S(BinaryExpression)  \
    S(Call)              \
    S(Comptime)          \
    S(Constant)          \
    S(ExpressionList)    \
    S(ForeignFunction)   \
    S(Function)          \
    S(Identifier)        \
    S(Module)            \
    S(Parameter)         \
    S(Program)           \
    S(Return)            \
    S(Signature)         \
    S(StatementBlock)    \
    S(TypeSpecification)

static bool     bind_initialized = false;
static bind_fnc bind_fncs[] = {
#undef S
#define S(T) [NT_##T] = default_bind,
    NODETYPES(S)
#undef S
};

void initialize_bind()
{
#undef S
#define S(T) bind_fncs[NT_##T] = T##_bind;
    BINDOVERRIDES(S)
#undef S
    bind_initialized = true;
}

nodeptr node_bind(parser_t *parser, nodeptr ix)
{
    if (!bind_initialized) {
        initialize_bind();
    }
    node_t *node = N(ix);
    if (node->bound_type.ok) {
        return ix;
    }
    trace("bind %zu = %s", ix.value, node_type_name(node->node_type));
    if (node->namespace.ok) {
        dynarr_append(&parser->namespaces, ix);
    }
    nodeptr ret = bind_fncs[parser_node(parser, ix)->node_type](parser, ix);
    if (node->namespace.ok) {
        dynarr_pop(&parser->namespaces);
    }
    if (ret.ok) {
        trace("result %zu = %s bound_type %zu " SL " " SL,
            ix.value, node_type_name(parser_node(parser, ix)->node_type),
            ret.value, SLARG(type_kind_name(ret)),
            SLARG(type_to_string(ret)));
        N(ix)->bound_type = ret;
        ++parser->bound;
    } else {
        trace("result %zu = %s => NULL",
            ix.value, node_type_name(parser_node(parser, ix)->node_type));
        N(ix)->bound_type = nullptr;
    }
    return ret;
}
