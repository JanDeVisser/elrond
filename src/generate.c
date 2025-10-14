/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "node.h"
#include "operators.h"
#include "parser.h"
#include "slice.h"
#include "type.h"
#include "value.h"

#define GN(n) (parser_node((gen)->parser, (n)))
#define GENERATEOVERRIDES(S) \
    S(BinaryExpression)      \
    S(Call)                  \
    S(Constant)              \
    S(Function)              \
    S(Module)                \
    S(Identifier)            \
    S(IfStatement)           \
    S(Module)                \
    S(Program)               \
    S(Return)                \
    S(StatementBlock)        \
    S(VariableDeclaration)   \
    S(WhileStatement)

typedef void (*generate_fnc)(ir_generator_t *, nodeptr n);

static void function_list(sb_t *sb, ir_generator_t *gen, nodeptr ir);
static void module_list(sb_t *sb, ir_generator_t *gen, nodeptr ir);
static void program_list(sb_t *sb, ir_generator_t *gen, nodeptr ir);

static void generate_default(ir_generator_t *gen, nodeptr n);
#undef S
#define S(T) static void generate_##T(ir_generator_t *gen, nodeptr n);
GENERATEOVERRIDES(S)
#undef S

static bool         generate_initialized = false;
static generate_fnc generate_fncs[] = {
#undef S
#define S(T) [NT_##T] = generate_default,
    NODETYPES(S)
#undef S
};

uint64_t next_label()
{
    static uint64_t label = 0;
    return label++;
}

slice_t operation_type_name(ir_operation_type_t type)
{
    switch (type) {
#undef S
#define S(I, T)   \
    case IRO_##I: \
        return C(#I);
        IROPERATIONTYPES(S)
#undef S
    default:
        UNREACHABLE();
    }
}

void operation_list(sb_t *sb, operation_t const *op)
{
    if (op->type == IRO_Label) {
        sb_printf(sb, "%lld:", op->Label);
        return;
    }
    slice_t type_name = operation_type_name(op->type);
    sb_printf(sb, "    " SL, SLARG(type_name));
    sb_printf(sb, "%*s", 15 - (int) type_name.len, "");
    switch (op->type) {
    case IRO_BinaryOperator:
        sb_printf(
            sb,
            SL " %s " SL,
            SLARG(type_to_string(op->BinaryOperator.lhs)),
            operator_name(op->BinaryOperator.op),
            SLARG(type_to_string(op->BinaryOperator.rhs)));
        break;
    case IRO_Break:
        sb_printf(sb, "scope_end %llu depth %llu label %llu exit_type %zu", op->Break.scope_end, op->Break.depth, op->Break.label, op->Break.exit_type.value);
        break;
    case IRO_PushConstant:
        value_print(sb, op->PushConstant);
        break;
    case IRO_Call:
    case IRO_NativeCall:
        sb_printf(sb, SL " " SL "(", SLARG(type_to_string(op->NativeCall.return_type)), SLARG(op->NativeCall.name));
        for (size_t ix = 0; ix < op->NativeCall.parameters.len; ++ix) {
            if (ix > 0) {
                sb_append_cstr(sb, ", ");
            }
            sb_append(sb, type_to_string(op->NativeCall.parameters.items[ix].type));
        }
        sb_append_char(sb, ')');
        break;
    case IRO_Pop:
        sb_printf(sb, SL, SLARG(type_to_string(op->Pop)));
        break;
    case IRO_PushVarAddress:
        trace("%p %zu", op->PushVarAddress.name.items, op->PushVarAddress.name.len);
        if (op->PushVarAddress.name.items == NULL) {
            sb_append(sb, C("name null"));
        } else {
            sb_printf(sb, SL " + %lu", SLARG(op->PushVarAddress.name), op->PushVarAddress.offset);
        }
        break;
    default:
        break;
    }
}

void function_list(sb_t *sb, ir_generator_t *gen, nodeptr ir)
{
    ir_node_t *func = gen->ir_nodes.items + ir.value;
    sb_printf(sb, "== [F] = " SL " ===================\n", SLARG(func->function.name));
    for (size_t ix = 0; ix < func->function.operations.len; ++ix) {
        operation_list(sb, func->function.operations.items + ix);
        sb_append_char(sb, '\n');
    }
}

void module_list(sb_t *sb, ir_generator_t *gen, nodeptr ir)
{
    ir_node_t *mod = gen->ir_nodes.items + ir.value;
    sb_printf(sb, "== [M] = " SL " ===================\n\n", SLARG(mod->module.name));
    for (size_t ix = 0; ix < mod->module.operations.len; ++ix) {
        operation_list(sb, mod->module.operations.items + ix);
        sb_append_char(sb, '\n');
    }
    sb_append_char(sb, '\n');
    for (size_t ix = 0; ix < mod->module.functions.len; ++ix) {
        function_list(sb, gen, mod->module.functions.items[ix]);
    }
}

void program_list(sb_t *sb, ir_generator_t *gen, nodeptr ir)
{
    ir_node_t *prog = gen->ir_nodes.items + ir.value;
    sb_printf(sb, "== [P] = " SL " ===================\n\n", SLARG(prog->program.name));
    for (size_t ix = 0; ix < prog->program.operations.len; ++ix) {
        operation_list(sb, prog->program.operations.items + ix);
        sb_append_char(sb, '\n');
    }
    sb_append_char(sb, '\n');
    for (size_t ix = 0; ix < prog->program.modules.len; ++ix) {
        module_list(sb, gen, prog->program.modules.items[ix]);
    }
}

void list(FILE *f, ir_generator_t *gen, nodeptr ir)
{
    assert(ir.ok);
    assert(ir.value < gen->ir_nodes.len);
    ir_node_t *n = gen->ir_nodes.items + ir.value;
    sb_t       list = { 0 };
    switch (n->type) {
    case IRN_Function:
        function_list(&list, gen, ir);
        break;
    case IRN_Module:
        module_list(&list, gen, ir);
        break;
    case IRN_Program:
        program_list(&list, gen, ir);
        break;
    }
    fprintf(f, SL "\n", SLARG(list));
}

void generator_add_operation(ir_generator_t *gen, operation_t op)
{
    for (int ix = gen->ctxs.len - 1; ix >= 0; --ix) {
        ir_context_t *ctx = gen->ctxs.items + ix;
        if (!ctx->ir_node.ok) {
            continue;
        }
        ir_node_t    *node = gen->ir_nodes.items + ctx->ir_node.value;
        operations_t *ops = NULL;
        switch (node->type) {
        case IRN_Function:
            ops = &node->function.operations;
            break;
        case IRN_Module:
            ops = &node->module.operations;
            break;
        case IRN_Program:
            ops = &node->program.operations;
            break;
        }
        assert(ops != NULL);
        if (ops->len > 0) {
            operation_t const *b = ops->items + (ops->len - 1);
            if (op.type == IRO_Discard && (b->type == IRO_PushConstant || b->type == IRO_PushValue)) {
                dynarr_pop(ops);
                return;
            }
        }
        sb_t op_string = { 0 };
        operation_list(&op_string, &op);
        trace("Appending op " SL, SLARG(op_string));
        dynarr_append(ops, op);
        return;
    }
    UNREACHABLE();
}

#define generator_add_op(gen, optype, ...) \
    generator_add_operation((gen), (operation_t) { .type = IRO_##optype, .optype = __VA_ARGS__ });

operation_t *last_op(ir_generator_t *gen)
{
    for (int ix = gen->ctxs.len - 1; ix >= 0; --ix) {
        ir_context_t *ctx = gen->ctxs.items + ix;
        if (!ctx->ir_node.ok) {
            continue;
        }
        ir_node_t    *node = gen->ir_nodes.items + ctx->ir_node.value;
        operations_t *ops = NULL;
        switch (node->type) {
        case IRN_Function:
            ops = &node->function.operations;
            break;
        case IRN_Module:
            ops = &node->module.operations;
            break;
        case IRN_Program:
            ops = &node->program.operations;
            break;
        }
        if (ops == NULL || ops->len == 0) {
            return NULL;
        }
        return ops->items + (ops->len - 1);
    }
    UNREACHABLE();
}

nodeptr find_ir_node(ir_generator_t const *gen, ir_node_type_t type)
{
    for (int ix = gen->ctxs.len - 1; ix >= 0; --ix) {
        ir_context_t *ctx = gen->ctxs.items + ix;
        if (!ctx->ir_node.ok) {
            continue;
        }
        if (gen->ir_nodes.items[ctx->ir_node.value].type == type) {
            return ctx->ir_node;
        }
    }
    return nullptr;
}

void generate_default(ir_generator_t *gen, nodeptr n)
{
    trace("generate_node(%s)", node_type_name(GN(n)->node_type));
}

void generate_BinaryExpression(ir_generator_t *gen, nodeptr n)
{
    node_t    *node = GN(n);
    nodeptr    lhs = node->binary_expression.lhs;
    nodeptr    rhs = node->binary_expression.rhs;
    operator_t op = node->binary_expression.op;
    nodeptr    lhs_type = GN(lhs)->bound_type;
    nodeptr    lhs_value_type = type_value_type(lhs_type);

    if (op == OP_MemberAccess) {
        generate(gen, lhs);
        type_t *lhs_type_ptr = get_type(lhs_type);
        type_t *s = get_type(lhs_type_ptr->referencing);
        size_t  offset = 0;
        dynarr_foreach(struct_field_t, fld, &s->struct_fields)
        {
            offset = align_at(type_align_of(fld->type), offset);
            if (slice_eq(fld->name, GN(rhs)->identifier.id)) {
                break;
            }
            offset += type_size_of(fld->type);
        }
        operation_t *operation = last_op(gen);
        operation->PushVarAddress.type = node->bound_type;
        operation->PushVarAddress.offset += offset;
        return;
    }

    nodeptr rhs_type = GN(rhs)->bound_type;
    nodeptr rhs_value_type = type_value_type(rhs_type);

    if (op == OP_Assign) {
        generate(gen, rhs);
        generate(gen, lhs);
        if (type_kind(rhs_type) == TYPK_ReferenceType) {
            generator_add_op(gen, AssignFromRef, lhs_value_type);
        } else {
            generator_add_op(gen, AssignValue, lhs_value_type);
        }
        generate(gen, lhs);
        generator_add_op(gen, Dereference, lhs_value_type);
        return;
    }
    generate(gen, lhs);
    if (lhs_type.value != lhs_value_type.value) {
        generator_add_op(gen, Dereference, lhs_value_type);
    }
    generate(gen, rhs);
    if (rhs_type.value != rhs_value_type.value) {
        generator_add_op(gen, Dereference, rhs_value_type);
    }
    generator_add_op(gen, BinaryOperator, (binary_op_t) { .lhs = lhs_value_type, .op = op, .rhs = rhs_value_type });
}

void generate_Call(ir_generator_t *gen, nodeptr n)
{
    node_t     *node = GN(n);
    namespace_t params = { 0 };
    node_t     *decl = GN(node->function_call.declaration);
    node_t     *decl_sig = GN(decl->function.signature);
    for (size_t ix = 0; ix < decl_sig->signature.parameters.len; ++ix) {
        node_t *param = GN(decl_sig->signature.parameters.items[ix]);
        dynarr_append_s(
            name_t,
            &params,
            .name = param->variable_declaration.name,
            .type = param->bound_type);
    }
    node_t *args = GN(node->function_call.arguments);
    for (size_t ix = 0; ix < args->expression_list.len; ++ix) {
        node_t *expr = GN(args->expression_list.items[ix]);
        generate(gen, args->expression_list.items[ix]);
        nodeptr value_type = type_value_type(expr->bound_type);
        if (value_type.value != expr->bound_type.value) {
            generator_add_op(gen, Dereference, value_type);
        }
    }
    node_t *impl = GN(decl->function.implementation);
    if (impl->node_type == NT_ForeignFunction) {
        generator_add_op(gen, NativeCall, { .name = impl->identifier.id, .parameters = params, .return_type = node->bound_type });
        return;
    }
    generator_add_op(gen, Call, { .name = decl->function.name, .parameters = params, .return_type = node->bound_type });
}

void generate_Constant(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    assert(node->constant_value.ok);
    generator_add_op(gen, PushConstant, node->constant_value.value);
}

void generate_Function(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    if (GN(node->function.implementation)->node_type != NT_ForeignFunction) {
        namespace_t params = { 0 };
        node_t     *sig = GN(node->function.signature);
        for (size_t ix = 0; ix < sig->signature.parameters.len; ++ix) {
            node_t *param = GN(sig->signature.parameters.items[ix]);
            dynarr_append_s(
                name_t,
                &params,
                .name = param->variable_declaration.name,
                .type = param->bound_type);
        }
        ir_node_t function = {
            .type = IRN_Function,
            .ix = gen->ir_nodes.len,
            .bound_type = node->bound_type,
            .function = {
                .name = node->function.name,
                .syntax_node = n,
                .module = gen->ctxs.items[gen->ctxs.len - 1].ir_node,
                .parameters = params,
                .return_type = GN(sig->signature.return_type)->bound_type,
                .operations = { 0 },
            }
        };
        dynarr_append(&gen->ir_nodes, function);
        uint64_t end_label = next_label();
        dynarr_append_s(
            ir_context_t,
            &gen->ctxs,
            .ir_node = OPTVAL(size_t, function.ix),
            .unwind_type = USET_Function,
            .function = {
                .end_label = end_label,
                .return_type = function.function.return_type,
            });
        nodeptr module_ptr = find_ir_node(gen, IRN_Module);
        assert(module_ptr.ok);
        ir_node_t *module = gen->ir_nodes.items + module_ptr.value;
        dynarr_append(&module->module.functions, OPTVAL(size_t, function.ix));

        generate(gen, node->function.implementation);
        generator_add_op(gen, Label, end_label);
        dynarr_pop(&gen->ctxs);
    }
    generator_add_op(gen, PushConstant, make_value_void());
}

void generate_Identifier(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    generator_add_op(gen, PushVarAddress, (var_path_t) { .name = node->identifier.id, .type = node->bound_type, .offset = 0 });
}

void generate_IfStatement(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    generate(gen, node->if_statement.condition);
    nodeptr cond_type = GN(node->while_statement.condition)->bound_type;
    nodeptr value_type = type_value_type(cond_type);
    if (value_type.value != cond_type.value) {
        generator_add_op(gen, Dereference, value_type);
    }
    int else_label = next_label();
    int done_label = next_label();
    generator_add_op(gen, JumpF, else_label);
    generate(gen, node->if_statement.if_branch);
    generator_add_op(gen, Jump, done_label);
    generator_add_op(gen, Label, else_label);
    if (node->if_statement.else_branch.ok) {
        generate(gen, node->if_statement.else_branch);
    }
    generator_add_op(gen, Label, done_label);
}

void generate_Module(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    nodeptr program_ptr = nullptr;
    if (gen->ctxs.len != 0) {
        program_ptr = gen->ctxs.items[0].ir_node;
    }
    ir_node_t *program = gen->ir_nodes.items + program_ptr.value;
    assert(program->type == IRN_Program);

    ir_node_t module = {
        .type = IRN_Module,
        .ix = gen->ir_nodes.len,
        .bound_type = node->bound_type,
        .module = {
            .name = node->module.name,
            .syntax_node = n,
            .program = gen->ctxs.items[gen->ctxs.len - 1].ir_node,
            .variables = dynarr_copy(namespace_t, name_t, node->namespace.value),
            .functions = { 0 },
            .operations = { 0 },
        }
    };
    dynarr_append(&gen->ir_nodes, module);
    dynarr_append_s(ir_context_t, &gen->ctxs, .ir_node = OPTVAL(size_t, module.ix));
    dynarr_append(&program->program.modules, OPTVAL(size_t, module.ix));

    nodeptr discard = nullptr;
    bool    empty = true;
    for (size_t ix = 0; ix < node->module.statements.len; ++ix) {
        nodeptr stmt = node->module.statements.items[ix];
        if (discard.ok) {
            generator_add_op(gen, Discard, discard);
        }
        discard = GN(stmt)->bound_type;
        empty &= (!discard.ok);
        generate(gen, stmt);
    }
    if (empty) {
        generator_add_op(gen, PushConstant, make_value_void());
    }
    dynarr_pop(&gen->ctxs);
}

void generate_Program(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    assert(gen->ctxs.len == 0);

    ir_node_t program = {
        .type = IRN_Program,
        .ix = gen->ir_nodes.len,
        .bound_type = node->bound_type,
        .program = {
            .name = node->program.name,
            .syntax_node = n,
            .variables = dynarr_copy(namespace_t, name_t, node->namespace.value),
            .functions = { 0 },
            .modules = { 0 },
            .operations = { 0 },
        }
    };
    dynarr_append(&gen->ir_nodes, program);
    dynarr_append_s(ir_context_t, &gen->ctxs, .ir_node = OPTVAL(size_t, program.ix));

    nodeptr discard = nullptr;
    bool    empty = true;
    for (size_t ix = 0; ix < node->program.statements.len; ++ix) {
        nodeptr stmt = node->program.statements.items[ix];
        if (discard.ok) {
            generator_add_op(gen, Discard, discard);
        }
        discard = GN(stmt)->bound_type;
        empty &= !discard.ok;
        generate(gen, stmt);
    }
    if (empty) {
        generator_add_op(gen, PushConstant, make_value_void());
    }
    for (size_t ix = 0; ix < node->program.modules.len; ++ix) {
        generate(gen, node->program.modules.items[ix]);
    }
    dynarr_pop(&gen->ctxs);
}

void generate_Return(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    generate(gen, node->statement);
    node_t *expr = GN(node->statement);
    nodeptr value_type = type_value_type(expr->bound_type);
    if (value_type.value != expr->bound_type.value) {
        generator_add_op(gen, Dereference, value_type);
    }
    generator_add_op(gen, Pop, value_type);

    uint64_t depth = 0;
    uint64_t scope_end = 0;
    for (int ix = gen->ctxs.len - 1; ix >= 0; --ix) {
        ir_context_t *ctx = gen->ctxs.items + ix;
        bool          done = false;
        switch (ctx->unwind_type) {
        case USET_Block: {
            if (scope_end == 0) {
                if (ctx->block.defer_stmts.len != 0) {
                    scope_end = ctx->block.defer_stmts.items[ctx->block.defer_stmts.len - 1].label;
                } else {
                    scope_end = ctx->block.scope_end_label;
                }
                ++depth;
            }
        } break;
        case USET_Function: {
            generator_add_op(gen, Break, { scope_end, depth, ULLONG_MAX, Void });
            done = true;
        } break;
        default:
            break;
        }
        if (done) {
            return;
        }
    }
    UNREACHABLE();
}

void generate_StatementBlock(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    bool    pop_mod_ctx = false;
    if (gen->ctxs.len == 0) {
        sb_t      mod_name = sb_format("comptime %zu", node->location.line);
        ir_node_t module = {
            .type = IRN_Module,
            .ix = gen->ir_nodes.len,
            .bound_type = node->bound_type,
            .module = {
                .name = sb_as_slice(mod_name),
                .syntax_node = n,
                .program = nullptr,
                .variables = { 0 },
                .functions = { 0 },
                .operations = { 0 },
            }
        };
        dynarr_append(&gen->ir_nodes, module);
        dynarr_append_s(ir_context_t, &gen->ctxs, .ir_node = OPTVAL(size_t, module.ix));
        pop_mod_ctx = true;
    }
    namespace_t variables = { 0 };
    for (size_t ix = 0; ix < node->namespace.value.len; ++ix) {
        dynarr_append(&variables, node->namespace.value.items[ix]);
    }
    generator_add_op(gen, ScopeBegin, variables);
    uint64_t scope_end = next_label();
    // uint64_t end_block = next_label();
    generator_add_op(gen, PushConstant, make_value_void());
    ir_context_t ctx = (ir_context_t) {
        .ir_node = gen->ctxs.items[gen->ctxs.len - 1].ir_node,
        .unwind_type = USET_Block,
        .block = (block_descriptor_t) { .scope_end_label = scope_end },
    };
    dynarr_append(&gen->ctxs, ctx);

    bool    has_defered = false;
    nodeptr discard = Null;
    bool    empty = true;
    for (size_t ix = 0; ix < node->statement_block.statements.len; ++ix) {
        nodeptr stmt = node->statement_block.statements.items[ix];
        node_t *stmt_ptr = GN(stmt);
        if (discard.ok) {
            generator_add_op(gen, Discard, discard);
        }
        discard = stmt_ptr->bound_type;
        empty &= !discard.ok;
        generate(gen, stmt);
    }
    if (empty) {
        generator_add_op(gen, PushConstant, make_value_void());
    }
    generator_add_op(gen, Label, scope_end);
    block_descriptor_t *bd = &gen->ctxs.items[gen->ctxs.len - 1].block;
    //    for (size_t ix = 0; ix < bd->defer_stmts.len; ++ix) {
    //        ir_defer_statement_t ds = bd->defer_stmts.items[ix];
    //        has_defered = true;
    //        generator_add_op(gen, Label, ds.label);
    //        generate(gen, ds.statement);
    //        generator_add_op(gen, Discard, GN(ds.statement)->bound_type);
    //    }
    dynarr_pop(&gen->ctxs);

    uint64_t enclosing_end = 0;
    for (size_t ix = gen->ctxs.len - 1; ix >= 0; --ix) {
        ir_context_t *ctx = gen->ctxs.items + ix;
        bool          quit = false;
        switch (ctx->unwind_type) {
        case USET_Block: {
            if (ctx->block.defer_stmts.len != 0) {
                enclosing_end = ctx->block.defer_stmts.items[bd->defer_stmts.len - 1].label;
                quit = true;
            }
        } break;
        case USET_Function: {
            enclosing_end = ctx->function.end_label;
            quit = true;
        } break;
        default:
            break;
        }
        if (quit) {
            break;
        }
    }
    generator_add_op(gen, ScopeEnd, (scope_end_op_t) {
                                        .enclosing_end = enclosing_end,
                                        .has_defers = has_defered,
                                        .exit_type = node->bound_type,
                                    });
    if (pop_mod_ctx) {
        dynarr_pop(&gen->ctxs);
    }
}

void generate_VariableDeclaration(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    generator_add_op(
        gen,
        DeclVar,
        (name_t) { .name = node->variable_declaration.name, .type = node->bound_type });
    if (node->variable_declaration.initializer.ok) {
        generate(gen, node->variable_declaration.initializer);
        nodeptr rhs_type = GN(node->variable_declaration.initializer)->bound_type;
        nodeptr lhs_type = node->bound_type;
        generator_add_op(
            gen,
            PushVarAddress,
            (var_path_t) { .name = node->variable_declaration.name, .type = node->bound_type, .offset = 0 });
        if (type_kind(rhs_type) == TYPK_ReferenceType) {
            generator_add_op(gen, AssignFromRef, lhs_type);
        } else {
            generator_add_op(gen, AssignValue, lhs_type);
        }
    }
    generator_add_op(
        gen,
        PushVarAddress,
        (var_path_t) { .name = node->variable_declaration.name, .type = node->bound_type, .offset = 0 });
    nodeptr value_type = type_value_type(node->bound_type);
    generator_add_op(gen, Dereference, value_type);
}

void generate_WhileStatement(ir_generator_t *gen, nodeptr n)
{
    node_t *node = GN(n);
    nodeptr stmt_type = GN(node->while_statement.statement)->bound_type;
    nodeptr stmt_value_type = type_value_type(stmt_type);
    generator_add_op(gen, PushConstant, (value_t) { .type = stmt_value_type });
    ir_context_t ld = (ir_context_t) {
        .ir_node = gen->ctxs.items[gen->ctxs.len - 1].ir_node,
        .unwind_type = USET_Loop,
        .loop = (loop_descriptor_t) {
            .name = ORELSE(slice_t, node->while_statement.label, C("")),
            .loop_begin = next_label(),
            .loop_end = next_label(),
        },
    };
    dynarr_append(&gen->ctxs, ld);
    generator_add_op(gen, Label, ld.loop.loop_begin);
    generate(gen, node->while_statement.condition);
    nodeptr cond_type = GN(node->while_statement.condition)->bound_type;
    nodeptr value_type = type_value_type(cond_type);
    if (value_type.value != cond_type.value) {
        generator_add_op(gen, Dereference, value_type);
    }
    generator_add_op(gen, JumpF, ld.loop.loop_end);
    generator_add_op(gen, Discard, GN(node->while_statement.statement)->bound_type);
    generate(gen, node->while_statement.statement);
    generator_add_op(gen, Jump, ld.loop.loop_begin);
    generator_add_op(gen, Label, ld.loop.loop_end);
    dynarr_pop(&gen->ctxs);
}

/*
template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<Break> const &node)
{
    uint64_t depth { 0 };
    uint64_t scope_end { 0 };
    for (auto const &ctx : generator.ctxs | std::views::reverse) {
        if (std::visit(
                overloads {
                    [&node, &gen, &depth, &scope_end](Context::LoopDescriptor const &ld) -> bool {
                        if (node->label->empty() || ld.name == node->label.value()) {
                            generator_add_op<Break>(gen, IR::Operation::BreakOp { scope_end, depth, ld.loop_end });
                            return true;
                        }
                        return false;
                    },
                    [&gen, &depth, &scope_end](Context::BlockDescriptor const &bd) -> bool {
                        if (!bd.defer_stmts.empty()) {
                            if (scope_end == 0) {
                                scope_end = bd.defer_stmts.back().second;
                            }
                            ++depth;
                        }
                        return false;
                    },
                    [](Context::FunctionDescriptor const &) -> bool {
                        UNREACHABLE();
                    },
                    [](auto const &) {
                        return false;
                    } },
                ctx.unwind)) {
            return;
        }
    }
    UNREACHABLE();
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<Continue> const &node)
{
    uint64_t depth { 0 };
    uint64_t scope_end { 0 };
    for (auto const &ctx : generator.ctxs | std::views::reverse) {
        if (std::visit(
                overloads {
                    [&node, &gen, &depth, &scope_end](Context::LoopDescriptor const &ld) -> bool {
                        if (node->label->empty() || ld.name == node->label.value()) {
                            generator_add_op<Break>(gen, IR::Operation::BreakOp { scope_end, depth, ld.loop_begin });
                            return true;
                        }
                        return false;
                    },
                    [&gen, &depth, &scope_end](Context::BlockDescriptor const &bd) -> bool {
                        if (!bd.defer_stmts.empty()) {
                            if (scope_end == 0) {
                                scope_end = bd.defer_stmts.back().second;
                            }
                            ++depth;
                        }
                        return false;
                    },
                    [](Context::FunctionDescriptor const &) -> bool {
                        UNREACHABLE();
                    },
                    [](auto const &) {
                        return false;
                    } },
                ctx.unwind)) {
            return;
        }
    }
    UNREACHABLE();
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<DeferStatement> const &node)
{
    auto const label = next_label();
    for (auto &ctx : std::views::reverse(generator.ctxs)) {
        if (std::visit(
                overloads {
                    [&node, &label](Context::BlockDescriptor &bd) -> bool {
                        bd.defer_stmts.emplace_back(node->stmt, label);
                        return true;
                    },
                    [](auto &) -> bool {
                        return false;
                    } },
                ctx.unwind)) {
            generator_add_op<PushConstant>(gen, make_void());
            return;
        }
    }
    UNREACHABLE();
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<Enum> const &node)
{
    generator_add_op<PushConstant>(gen, make_void());
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<LoopStatement> const &node)
{
    generator_add_op<PushConstant>(gen, make_value(node->statement->bound_type));
    Context::LoopDescriptor const ld { node->label.value_or(std::wstring {}), next_label(), next_label() };
    generator.ctxs.push_back(Context { {}, ld });
    generator_add_op<Label>(gen, ld.loop_begin);
    generator_add_op<Discard>(gen, node->statement->bound_type);
    generator.generate(node->statement);
    generator_add_op<Jump>(gen, ld.loop_begin);
    generator_add_op<Label>(gen, ld.loop_end);
    generator.ctxs.pop_back();
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<Struct> const &node)
{
    generator_add_op<PushConstant>(gen, make_void());
}

template<>
void generate_node(ir_generator_t * gen, std::shared_ptr<UnaryExpression> const &node)
{
    if (node->op == Operator::Sizeof && node->operand->type == SyntaxNodeType::TypeSpecification) {
        generator_add_op<PushConstant>(gen, Value { TypeRegistry::i64, node->operand->bound_type->size_of() });
        return;
    }
    generator.generate(node->operand);
    if (auto value_type = node->operand->bound_type->value_type(); node->operand->bound_type != value_type) {
        generator_add_op<Dereference>(gen, value_type);
    }
    generator_add_op<UnaryOperator>(gen, Operation::UnaryOperator { node->operand->bound_type, node->op });
}
*/

void initialize_generate()
{
#undef S
#define S(T) generate_fncs[NT_##T] = generate_##T;
    GENERATEOVERRIDES(S)
#undef S
    generate_initialized = true;
}

void generate(ir_generator_t *gen, nodeptr n)
{
    if (!generate_initialized) {
        initialize_generate();
    }
    node_t *node = GN(n);
    trace("generate %zu = %s", n.value, node_type_name(node->node_type));
    generate_fncs[node->node_type](gen, n);
}

ir_generator_t generate_ir(parser_t *parser, nodeptr node)
{
    ir_generator_t generator = { 0 };
    generator.parser = parser;
    generate(&generator, node);
    assert(generator.ctxs.len == 0);
    return generator;
}
