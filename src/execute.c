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
#include "native.h"
#include "node.h"
#include "type.h"
#include "value.h"

#undef S
#define S(O, T) static void execute_##O(interpreter_t *interpreter, operation_t *op);
IROPERATIONTYPES(S)
#undef S

uint64_t ip_for_label(interpreter_t *interpreter, uint64_t label)
{
    interpreter_context_t *ctx = dynarr_back(&interpreter->call_stack);
    for (size_t ix = 0; ix < interpreter->labels.len; ++ix) {
        if (interpreter->labels.items[ix].ir_node.value == ctx->ir.value) {
            interpreter_labels_t labels = interpreter->labels.items[ix].labels;
            for (size_t ix = 0; ix < labels.len; ++ix) {
                if (labels.items[ix].label == label) {
                    return labels.items[ix].operation_index;
                }
            }
        }
    }
    UNREACHABLE();
}

void execute_default(interpreter_t *interpreter, operation_t *op)
{
    printf("execute_op(" SL ")\n", SLARG(operation_type_name(op->type)));
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_AssignFromRef(interpreter_t *interpreter, operation_t *op)
{
    uint64_t var_ref = stack_pop_T(uint64_t, &interpreter->stack);
    uint64_t val_ref = stack_pop_T(uint64_t, &interpreter->stack);
    stack_copy(&interpreter->stack, var_ref, val_ref, type_size_of(op->AssignFromRef));
    stack_push_T(uint64_t, &interpreter->stack, var_ref);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_AssignValue(interpreter_t *interpreter, operation_t *op)
{
    uint64_t var_ref = stack_pop_T(uint64_t, &interpreter->stack);
    stack_copy_and_pop(&interpreter->stack, var_ref, type_size_of(op->AssignValue));
    stack_push_T(uint64_t, &interpreter->stack, var_ref);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_BinaryOperator(interpreter_t *interpreter, operation_t *op)
{
    stack_evaluate(&interpreter->stack, op->BinaryOperator.lhs, op->BinaryOperator.op, op->BinaryOperator.rhs);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_Break(interpreter_t *interpreter, operation_t *op)
{
    uint64_t depth = (op->Break.scope_end != 0) ? op->Break.depth : 0;
    uint64_t ip = ip_for_label(interpreter, op->Break.label);
    interpreter_move_in(interpreter, &depth, sizeof(uint64_t), 18);
    interpreter_move_in(interpreter, &ip, sizeof(uint64_t), 17);
    dynarr_back(&interpreter->call_stack)->ip = ip_for_label(interpreter, op->Break.scope_end);
}

ir_node_t *find_function(ir_generator_t *gen, nodeptr ir, slice_t name)
{
    ir_node_t *node = gen->ir_nodes.items + ir.value;
    switch (node->type) {
    case IRN_Module: {
        for (size_t ix = 0; ix < node->module.functions.len; ++ix) {
            nodeptr    func = node->module.functions.items[ix];
            ir_node_t *f = gen->ir_nodes.items + func.value;
            if (slice_eq(f->function.name, name)) {
                return f;
            }
        }
        return find_function(gen, node->module.program, name);
    }
    case IRN_Program: {
        for (size_t ix = 0; ix < node->program.functions.len; ++ix) {
            nodeptr    func = node->program.functions.items[ix];
            ir_node_t *f = gen->ir_nodes.items + func.value;
            if (slice_eq(f->function.name, name)) {
                return f;
            }
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

void execute_Call(interpreter_t *interpreter, operation_t *op)
{
    nodeptr scope_ix = nodeptr_ptr(interpreter->scopes.len - 1);
    do {
        scope_t   *s = interpreter->scopes.items + scope_ix.value;
        ir_node_t *func = find_function(interpreter->gen, s->ir, op->Call.name);
        if (func != NULL) {
            nodeptr f = nodeptr_ptr(func->ix);
            interpreter_emplace_scope(interpreter, f, op->Call.parameters);
            dynarr_append_s(
                interpreter_context_t,
                &interpreter->call_stack,
                .ir = f, .ip = 0);
            interpreter_execute_operations(interpreter, f);
            dynarr_pop(&interpreter->call_stack);
            interpreter_drop_scope(interpreter);
            value_t return_value = make_value_from_buffer(op->Call.return_type, interpreter->registers);
            stack_push_value(&interpreter->stack, return_value);
            dynarr_back(&interpreter->call_stack)->ip++;
            return;
        }
        scope_ix = s->parent;
    } while (scope_ix.ok);
    UNREACHABLE();
}

void execute_DeclVar(interpreter_t *interpreter, operation_t *op)
{
    (void) op;
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_Dereference(interpreter_t *interpreter, operation_t *op)
{
    uint64_t ref = stack_pop_T(uint64_t, &interpreter->stack);
    stack_push_copy(&interpreter->stack, ref, type_size_of(op->Dereference));
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_Discard(interpreter_t *interpreter, operation_t *op)
{
    stack_discard(&interpreter->stack, align_at(8, type_size_of(op->Discard)));
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_Jump(interpreter_t *interpreter, operation_t *op)
{
    dynarr_back(&interpreter->call_stack)->ip = ip_for_label(interpreter, op->Jump);
}

void execute_JumpF(interpreter_t *interpreter, operation_t *op)
{
    if (!stack_pop_T(bool, &interpreter->stack)) {
        dynarr_back(&interpreter->call_stack)->ip = ip_for_label(interpreter, op->JumpF);
        return;
    }
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_JumpT(interpreter_t *interpreter, operation_t *op)
{
    if (stack_pop_T(bool, &interpreter->stack)) {
        dynarr_back(&interpreter->call_stack)->ip = ip_for_label(interpreter, op->JumpT);
        return;
    }
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_Label(interpreter_t *interpreter, operation_t *op)
{
    (void) interpreter;
    (void) op;
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_NativeCall(interpreter_t *interpreter, operation_t *op)
{
    intptr_t depth = 0;
    nodeptrs types;
    for (size_t ix = 0; ix < op->NativeCall.parameters.len; ++ix) {
        name_t *param = op->NativeCall.parameters.items + ix;
        depth += align_at(8, type_size_of(param->type)) / sizeof(intptr_t);
        dynarr_append(&types, param->type);
    }
    void *ptr = interpreter->stack.items + (interpreter->stack.len - depth);
    if (native_call(op->NativeCall.name, ptr, types, interpreter->registers, op->NativeCall.return_type)) {
        value_t return_value = make_value_from_buffer(op->Call.return_type, interpreter->registers);
        stack_discard(&interpreter->stack, depth);
        stack_push_value(&interpreter->stack, return_value);
        dynarr_back(&interpreter->call_stack)->ip++;
        return;
    }
    fprintf(stderr, "Error executing native function `" SL "`", SLARG(op->NativeCall.name));
    abort();
}

void execute_Pop(interpreter_t *interpreter, operation_t *op)
{
    value_t return_value = interpreter_pop(interpreter, op->Pop);
    interpreter_move_in_value(interpreter, return_value, 0);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_PushConstant(interpreter_t *interpreter, operation_t *op)
{
    stack_push_value(&interpreter->stack, op->PushConstant);
    dynarr_back(&interpreter->call_stack)->ip++;
}

scope_variable_t *get_variable(interpreter_t *interpreter, slice_t name)
{
    nodeptr ix = nodeptr_ptr(interpreter->scopes.len - 1);
    do {
        scope_t *s = interpreter->scopes.items + ix.value;
        for (size_t ix = 0; ix < s->variables.len; ++ix) {
            scope_variable_t *v = s->variables.items + ix;
            if (slice_eq(v->name, name)) {
                return v;
            }
        }
        ix = s->parent;
    } while (ix.ok);
    UNREACHABLE();
}

void execute_PushValue(interpreter_t *interpreter, operation_t *op)
{
    scope_variable_t *var = get_variable(interpreter, op->PushValue.name);
    stack_push_copy(&interpreter->stack, var->address + op->PushValue.offset, type_size_of(op->PushValue.type));
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_PushVarAddress(interpreter_t *interpreter, operation_t *op)
{
    scope_variable_t *var = get_variable(interpreter, op->PushValue.name);
    stack_push_T(uint64_t, &interpreter->stack, var->address + op->PushVarAddress.offset);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_ScopeBegin(interpreter_t *interpreter, operation_t *op)
{
    interpreter_new_scope(interpreter, nullptr, op->ScopeBegin);
    uint64_t zero = 0;
    interpreter_move_in(interpreter, &zero, sizeof(uint64_t), 17); // TODO defines for magic reg numbers.
    interpreter_move_in(interpreter, &zero, sizeof(uint64_t), 18); // or even make magic regs interpreter fields
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_ScopeEnd(interpreter_t *interpreter, operation_t *op)
{
    interpreter_drop_scope(interpreter);
    uint64_t depth = interpreter_move_out_reg(interpreter, 18);
    if (depth > 0) {
        --depth;
        interpreter_move_in(interpreter, &depth, sizeof(uint64_t), 18);
        dynarr_back(&interpreter->call_stack)->ip = ip_for_label(interpreter, op->ScopeEnd.enclosing_end);
        return;
    }
    uint64_t jump = interpreter_move_out_reg(interpreter, 17);
    if (jump != 0) {
        dynarr_back(&interpreter->call_stack)->ip = jump;
        return;
    }
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_UnaryOperator(interpreter_t *interpreter, operation_t *op)
{
    stack_evaluate_unary(&interpreter->stack, op->UnaryOperator.operand, op->UnaryOperator.op);
    dynarr_back(&interpreter->call_stack)->ip++;
}

void execute_op(operation_t *op, interpreter_t *interpreter)
{
    switch (op->type) {
#undef S
#define S(O, T)                       \
    case IRO_##O:                     \
        execute_##O(interpreter, op); \
        break;
        IROPERATIONTYPES(S)
#undef S
    default:
        UNREACHABLE();
    }
}
