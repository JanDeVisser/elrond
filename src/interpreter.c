/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "interpreter.h"
#include "ir.h"
#include "node.h"
#include "type.h"
#include "value.h"

scope_t scope_initialize(interpreter_t *interpreter, nodeptr ir, namespace_t variables, nodeptr parent)
{
    scope_t ret = {
        .interpreter = interpreter,
        .ir = ir,
        .parent = parent,
    };
    for (size_t ix = 0; ix < variables.len; ++ix) {
        name_t *var = variables.items + ix;
        dynarr_append_s(
            scope_variable_t,
            &ret.variables,
            .name = var->name,
            .address = 0,
            .type = var->type);
    }
    return ret;
}

void scope_allocate(scope_t *scope)
{
    scope->bp = scope->interpreter->stack.len;
    for (size_t ix = 0; ix < scope->variables.len; ++ix) {
        scope_variable_t *var = scope->variables.items + ix;
        var->address = stack_reserve(&scope->interpreter->stack, type_size_of(var->type));
    }
}

void scope_setup(scope_t *scope)
{
    intptr_t depth = 0;
    for (size_t ix = 0; ix < scope->variables.len; ++ix) {
        scope_variable_t *var = scope->variables.items + ix;
        depth += align_at(8, type_size_of(var->type));
    }
    scope->bp = scope->interpreter->stack.len - depth;
    uint64_t offset = scope->bp;
    for (size_t ix = 0; ix < scope->variables.len; ++ix) {
        scope_variable_t *var = scope->variables.items + ix;
        var->address = offset;
        offset += align_at(8, type_size_of(var->type));
    }
}

void scope_release(scope_t *scope)
{
    ir_node_t *node = scope->interpreter->gen->ir_nodes.items + scope->ir.value;
    if (node->type == IRN_Program || node->type == IRN_Module) {
        return;
    }
    stack_discard(&scope->interpreter->stack, scope->bp);
}

scope_t *interpreter_current_scope(interpreter_t *interpreter)
{
    return dynarr_back(&interpreter->scopes);
}

static void create_scope(interpreter_t *interpreter, nodeptr ir, namespace_t variables)
{
    nodeptr parent = { 0 };
    if (interpreter->scopes.len != 0) {
        ir_node_t *ir_node = interpreter->gen->ir_nodes.items + ir.value;
        switch (ir_node->type) {
        case IRN_Program:
            UNREACHABLE();
        case IRN_Module: {
            scope_t *program_scope = interpreter->scopes.items;
            if (interpreter->gen->ir_nodes.items[program_scope->ir.value].type == IRN_Program) {
                parent = nodeptr_ptr(0);
            }
        } break;
        case IRN_Function: {
            for (size_t ix = 0; ix < interpreter->scopes.len; ++ix) {
                scope_t   *s = interpreter->scopes.items + ix;
                ir_node_t *s_node = interpreter->gen->ir_nodes.items + s->ir.value;
                if (s_node->type == IRN_Module && s->ir.value == ir_node->function.module.value) {
                    parent = nodeptr_ptr(ix);
                    break;
                }
            }
            assert(parent.ok);
        } break;
        default:
            parent = nodeptr_ptr(interpreter->scopes.len - 1);
        }
    }
    dynarr_append(&interpreter->scopes, scope_initialize(interpreter, ir, variables, parent));
}

scope_t *interpreter_new_scope(interpreter_t *interpreter, nodeptr ir, namespace_t variables)
{
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_OnScopeStart, interpreter, (interpreter_callback_payload_t) { 0 });
    }
    create_scope(interpreter, ir, variables);
    scope_t *back = dynarr_back(&interpreter->scopes);
    scope_allocate(back);
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_AfterScopeStart, interpreter, (interpreter_callback_payload_t) { 0 });
    }
    return back;
}

scope_t *interpreter_emplace_scope(interpreter_t *interpreter, nodeptr ir, namespace_t variables)
{
    create_scope(interpreter, ir, variables);
    scope_t *back = dynarr_back(&interpreter->scopes);
    scope_setup(back);
    return back;
}

void interpreter_drop_scope(interpreter_t *interpreter)
{
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_OnScopeDrop, interpreter, (interpreter_callback_payload_t) { 0 });
    }
    scope_t *back = dynarr_back(&interpreter->scopes);
    scope_release(back);
    dynarr_pop(&interpreter->scopes);
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_AfterScopeDrop, interpreter, (interpreter_callback_payload_t) { 0 });
    }
}

value_t interpreter_pop(interpreter_t *interpreter, nodeptr type)
{
    size_t  aligned = align_at(8, type_size_of(type));
    size_t  offset = interpreter->stack.len - aligned / sizeof(intptr_t);
    value_t ret = make_value_from_buffer(type, (void *) (interpreter->stack.items + offset));
    stack_discard(&interpreter->stack, aligned);
    return ret;
}

void interpreter_move_in(interpreter_t *interpreter, void *ptr, size_t size, uint8_t reg)
{
    assert(reg < INTERPRETER_NUM_REGS);
    size_t num_regs = align_at(8, size) / 8;
    assert(reg + num_regs - 1 < INTERPRETER_NUM_REGS);
    memcpy(interpreter->registers + reg, ptr, size);
}

void interpreter_move_in_value(interpreter_t *interpreter, value_t val, uint8_t reg)
{
    type_t *t = get_type(val.type);
    switch (t->kind) {
    case TYPK_IntType:
        switch (t->int_type.code) {
#undef S
#define S(W, Signed, Code, Fld, Type)                           \
    case Code:                                                  \
        interpreter_move_in_T(Type, interpreter, val.Fld, reg); \
        break;
            VALUE_INTFIELDS(S)
#undef S
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            interpreter_move_in_T(float, interpreter, val.f32, reg);
            break;
        case FW_64:
            interpreter_move_in_T(double, interpreter, val.f64, reg);
            break;
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_BoolType:
        interpreter_move_in_T(bool, interpreter, val.boolean, reg);
        break;
    case TYPK_SliceType:
        interpreter_move_in_T(slice_t, interpreter, val.slice, reg);
        break;
    case TYPK_VoidType:
        interpreter_move_in_T(stack_void_t, interpreter, (stack_void_t) {}, reg);
        break;
    default:
        UNREACHABLE();
    }
}

uint64_t interpreter_move_out_reg(interpreter_t *interpreter, uint8_t reg)
{
    assert(reg < INTERPRETER_NUM_REGS);
    return interpreter->registers[reg];
}

value_t interpreter_move_out(interpreter_t *interpreter, nodeptr type, uint8_t reg)
{
    assert(reg < INTERPRETER_NUM_REGS);
    size_t num_regs = align_at(8, type_size_of(type)) / 8;
    assert(reg + num_regs < INTERPRETER_NUM_REGS);
    return make_value_from_buffer(type, interpreter->registers + reg);
}

void interpreter_execute_operations(interpreter_t *interpreter, nodeptr ir)
{
    ir_node_t    *ir_node = interpreter->gen->ir_nodes.items + ir.value;
    operations_t *ops = NULL;
    switch (ir_node->type) {
    case IRN_Function:
        ops = &ir_node->function.operations;
        break;
    case IRN_Module:
        ops = &ir_node->module.operations;
        break;
    default:
        UNREACHABLE();
    }
    list(stderr, interpreter->gen, ir);
    assert(ops != NULL);
    bool must_analyze = true;
    for (size_t ix = 0; ix < interpreter->labels.len; ++ix) {
        if (interpreter->labels.items[ix].ir_node.value == ir.value) {
            must_analyze = false;
            break;
        }
    }
    if (must_analyze) {
        interpreter_labels_t labels = { 0 };
        for (size_t ix = 0; ix < ops->len; ++ix) {
            operation_t *op = ops->items + ix;
            if (op->type == IRO_Label) {
                dynarr_append_s(
                    interpreter_label_t,
                    &labels,
                    .label = op->Label,
                    .operation_index = ix, );
            }
            dynarr_append_s(
                interpreter_node_labels_t,
                &interpreter->labels,
                .ir_node = ir,
                .labels = labels);
        }
    }
    interpreter_context_t *ctx = dynarr_back(&interpreter->call_stack);
    while (ctx->ip < ops->len) {
        execute_op(ops->items + ctx->ip, interpreter);
    }
}

value_t interpreter_execute(interpreter_t *interpreter, nodeptr ir)
{
    ir_node_t *node = interpreter->gen->ir_nodes.items + ir.value;
    switch (node->type) {
    case IRN_Function:
        return execute_function(interpreter, ir);
    case IRN_Module:
        return execute_module(interpreter, ir);
    case IRN_Program:
        return execute_module(interpreter, ir);
    default:
        UNREACHABLE();
    }
}

value_t execute_program(interpreter_t *interpreter, nodeptr program)
{
    interpreter_new_scope(interpreter, program, (namespace_t) { 0 });
    dynarr_append_s(
        interpreter_context_t,
        &interpreter->call_stack,
        .ir = program);
    nodeptr    main = { 0 };
    ir_node_t *prog = interpreter->gen->ir_nodes.items + program.value;
    for (size_t ix = 0; ix < prog->program.modules.len; ++ix) {
        nodeptr m = prog->program.modules.items[ix];
        interpreter_execute(interpreter, m);
        ir_node_t *mod = interpreter->gen->ir_nodes.items + m.value;
        for (size_t iix = 0; !main.ok && ix < mod->module.functions.len; ++ix) {
            nodeptr    f = mod->module.functions.items[iix];
            ir_node_t *func = interpreter->gen->ir_nodes.items + f.value;
            if (slice_eq(func->function.name, C("main"))) {
                main = f;
                break;
            }
        }
    }
    if (main.ok) {
        return execute_function(interpreter, main);
    }
    return make_value_void();
}

value_t execute_module(interpreter_t *interpreter, nodeptr module)
{
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_StartModule, interpreter, (interpreter_callback_payload_t) { .module = module });
    }
    ir_node_t *mod = interpreter->gen->ir_nodes.items + module.value;
    dynarr_append_s(
        interpreter_context_t,
        &interpreter->call_stack,
        .ir = module,
        .ip = 0);
    interpreter_new_scope(
        interpreter,
        module,
        mod->module.variables);
    interpreter_execute_operations(interpreter, module);
    dynarr_pop(&interpreter->call_stack);
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_EndModule, interpreter, (interpreter_callback_payload_t) { .module = module });
    }
    return interpreter_pop(interpreter, mod->bound_type);
}

value_t execute_function(interpreter_t *interpreter, nodeptr function)
{
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_StartFunction, interpreter, (interpreter_callback_payload_t) { .function = function });
    }
    ir_node_t *func = interpreter->gen->ir_nodes.items + function.value;
    scope_t   *param_scope = interpreter_new_scope(interpreter, function, func->function.parameters);
    (void) param_scope;
    dynarr_append_s(
        interpreter_context_t,
        &interpreter->call_stack,
        .ir = function,
        .ip = 0);
    interpreter_execute_operations(interpreter, function);
    dynarr_pop(&interpreter->call_stack);
    interpreter_drop_scope(interpreter);
    if (interpreter->callback != NULL) {
        interpreter->callback(ICT_EndFunction, interpreter, (interpreter_callback_payload_t) { .function = function });
    }
    return interpreter_move_out(interpreter, func->function.return_type, 0);
}

value_t execute_ir(ir_generator_t *gen, nodeptr ir)
{
    interpreter_t interpreter = { 0 };
    interpreter.gen = gen;
    return interpreter_execute(&interpreter, ir);
}
