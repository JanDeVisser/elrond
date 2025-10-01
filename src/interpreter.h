/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "ir.h"
#include "node.h"
#include "value.h"

#ifndef __INTERPRETER_H__
#define __INTERPRETER_H__

typedef struct _stack_void {
} stack_void_t;

typedef struct __stack_reference {
    intptr_t address;
    nodeptr  type;
} stack_reference_t;

typedef DA(intptr_t) interp_stack_t;

#define stack_ensure(stack, size) dynarr_ensure(stack, size)
intptr_t stack_reserve(interp_stack_t *stack, size_t size);
intptr_t stack_push(interp_stack_t *stack, void *ptr, size_t size);
intptr_t stack_push_value(interp_stack_t *stack, value_t value);
void     stack_store(interp_stack_t *stack, void *src, intptr_t offset, size_t size);
void     stack_load(interp_stack_t *stack, void *dest, intptr_t offset, size_t size);
void     stack_discard(interp_stack_t *stack, size_t bp);
void     stack_pop(interp_stack_t *stack, void *dest, size_t size);
void     stack_copy(interp_stack_t *stack, intptr_t dest, intptr_t src, size_t size);
void     stack_copy_and_pop(interp_stack_t *stack, intptr_t dest, size_t size);
void     stack_push_copy(interp_stack_t *stack, intptr_t src, size_t size);
intptr_t stack_evaluate(interp_stack_t *stack, nodeptr lhs_type, operator_t op, nodeptr rhs_type);
intptr_t stack_evaluate_unary(interp_stack_t *stack, nodeptr operand, operator_t op);

#define stack_store_T(T, stack, val, offset)                            \
    (                                                                   \
        {                                                               \
            interp_stack_t *__stack = (stack);                          \
            T               __val = (val);                              \
            intptr_t        __offset = (offset);                        \
            stack_store(__stack, (void *) &__val, __offset, sizeof(T)); \
        })

#define stack_push_T(T, stack, val)                          \
    ((sizeof(T) == 0) ? (stack)->len * sizeof(intptr_t) : ({ \
        interp_stack_t *__stack = (stack);                   \
        T               __val = (val);                       \
        stack_push(__stack, (void *) &__val, sizeof(T));     \
    }))

#define stack_get(T, stack, offset)                                     \
    (                                                                   \
        (sizeof(T) == 0)                                                \
            ? (T) { 0 }                                                 \
            : (                                                         \
                  {                                                     \
                      T __val;                                          \
                      stack_load((stack), &__val, (offset), sizeof(T)); \
                      (__val);                                          \
                  }))

#define stack_pop_T(T, stack)                                \
    (                                                        \
        (sizeof(T) == 0)                                     \
            ? (T) { 0 }                                      \
            : (                                              \
                  {                                          \
                      T __val;                               \
                      stack_pop((stack), &__val, sizeof(T)); \
                      (__val);                               \
                  }))

#define stack_copy_T(T, stack, dest, src)                  \
    do {                                                   \
        if (sizeof(T) != 0) {                              \
            stack_copy((stack), (dest), (src), sizeof(T)); \
        }                                                  \
    } while (0)

#define stack_copy_and_pop_T(T, stack, dest)                \
    do {                                                    \
        if (sizeof(T) != 0) {                               \
            stack_copy_and_pop((stack), (dest), sizeof(T)); \
        }                                                   \
    } while (0)

#define stack_push_copy_T(T, stack, src)                \
    do {                                                \
        if (sizeof(T) != 0) {                           \
            stack_push_copy((stack), (src), sizeof(T)); \
        }                                               \
    } while (0)

typedef intptr_t            value_address_t;
typedef struct _interpreter interpreter_t;

typedef struct _scope_variable {
    slice_t         name;
    value_address_t address;
    nodeptr         type;
} scope_variable_t;

typedef DA(scope_variable_t) scope_variables_t;

typedef struct _scope {
    interpreter_t    *interpreter;
    nodeptr           ir;
    nodeptr           parent;
    scope_variables_t variables;
    size_t            bp;
} scope_t;

typedef DA(scope_t) scopes_t;

scope_t scope_initialize(interpreter_t *interpreter, nodeptr ir, namespace_t variables, nodeptr parent);
void    scope_allocate(scope_t *scope);
void    scope_setup(scope_t *scope);
void    scope_release(scope_t *scope);

typedef struct _interpreter_context {
    nodeptr  ir;
    uint64_t ip;
} interpreter_context_t;

typedef DA(interpreter_context_t) interpreter_contexts_t;

typedef enum _interpeter_callback_type {
    ICT_StartModule,
    ICT_EndModule,
    ICT_StartFunction,
    ICT_EndFunction,
    ICT_BeforeOperation,
    ICT_AfterOperation,
    ICT_OnScopeStart,
    ICT_AfterScopeStart,
    ICT_OnScopeDrop,
    ICT_AfterScopeDrop,
} interpreter_callback_type_t;

typedef union _interpreter_callback_payload {
    operation_t op;
    nodeptr     function;
    nodeptr     module;
    nodeptr     type;
} interpreter_callback_payload_t;

typedef bool (*interpreter_callback_t)(interpreter_callback_type_t, interpreter_t *, interpreter_callback_payload_t payload);

typedef struct _interpreter_label {
    uint64_t label;
    uint64_t operation_index;
} interpreter_label_t;

typedef DA(interpreter_label_t) interpreter_labels_t;

typedef struct _interpreter_node_labels {
    nodeptr              ir_node;
    interpreter_labels_t labels;
} interpreter_node_labels_t;

typedef DA(interpreter_node_labels_t) interpreter_nodes_labels_t;

#define INTERPRETER_NUM_REGS 20

typedef struct _interpreter {
    ir_generator_t            *gen;
    interpreter_nodes_labels_t labels;
    scopes_t                   scopes;
    interp_stack_t             stack;
    interpreter_contexts_t     call_stack;
    uint64_t                   registers[INTERPRETER_NUM_REGS];
    interpreter_callback_t     callback;
} interpreter_t;

scope_t *interpreter_current_scope(interpreter_t *interpreter);
scope_t *interpreter_new_scope(interpreter_t *interpreter, nodeptr ir, namespace_t variables);
scope_t *interpreter_emplace_scope(interpreter_t *interpreter, nodeptr ir, namespace_t variables);
void     interpreter_drop_scope(interpreter_t *interpreter);
void     interpreter_move_in(interpreter_t *interpreter, void *ptr, size_t size, uint8_t reg);
void     interpreter_move_in_value(interpreter_t *interpreter, value_t val, uint8_t reg);
uint64_t interpreter_move_out_reg(interpreter_t *interpreter, uint8_t reg);
value_t  interpreter_move_out(interpreter_t *interpreter, nodeptr type, uint8_t reg);
value_t  interpreter_pop(interpreter_t *interpreter, nodeptr type);
void     interpreter_execute_operations(interpreter_t *interpreter, nodeptr ir);
value_t  interpreter_execute(interpreter_t *interpreter, nodeptr ir);
void     execute_op(operation_t *op, interpreter_t *interpreter);
value_t  execute_function(interpreter_t *interpreter, nodeptr function);
value_t  execute_program(interpreter_t *interpreter, nodeptr program);
value_t  execute_module(interpreter_t *interpreter, nodeptr module);
value_t  execute_ir(ir_generator_t *gen, nodeptr ir);

#define interpreter_move_in_T(T, interpreter, val, reg)               \
    do {                                                              \
        T __val = (val);                                              \
        interpreter_move_in((interpreter), &__val, sizeof(T), (reg)); \
    } while (0)

#endif /* __INTERPRETER_H__ */
