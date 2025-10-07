/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __IR_H__
#define __IR_H__

#include <stdint.h>

#include "node.h"
#include "parser.h"
#include "slice.h"
#include "value.h"

#define IROPERATIONTYPES(S)        \
    S(AssignFromRef, nodeptr)      \
    S(AssignValue, nodeptr)        \
    S(BinaryOperator, binary_op_t) \
    S(Break, break_op_t)           \
    S(Call, call_op_t)             \
    S(DeclVar, name_t)             \
    S(Dereference, nodeptr)        \
    S(Discard, nodeptr)            \
    S(Jump, uint64_t)              \
    S(JumpF, uint64_t)             \
    S(JumpT, uint64_t)             \
    S(Label, uint64_t)             \
    S(NativeCall, call_op_t)       \
    S(Pop, nodeptr)                \
    S(PushConstant, value_t)       \
    S(PushValue, var_path_t)       \
    S(PushVarAddress, var_path_t)  \
    S(ScopeBegin, namespace_t)     \
    S(ScopeEnd, scope_end_op_t)    \
    S(UnaryOperator, unary_op_t)

typedef enum _ir_operation_type {
#undef S
#define S(I, T) IRO_##I,
    IROPERATIONTYPES(S)
#undef S
} ir_operation_type_t;

typedef struct _var_path {
    slice_t  name;
    nodeptr  type;
    intptr_t offset;
} var_path_t;

typedef struct _break_op {
    uint64_t scope_end;
    uint64_t depth;
    uint64_t label;
    nodeptr  exit_type;
} break_op_t;

typedef struct _call_op {
    slice_t     name;
    namespace_t parameters;
    nodeptr     return_type;
} call_op_t;

typedef struct _binary_op {
    nodeptr    lhs;
    operator_t op;
    nodeptr    rhs;
} binary_op_t;

typedef struct _scope_end_op {
    uint64_t enclosing_end;
    bool     has_defers;
    nodeptr  exit_type;
} scope_end_op_t;

typedef struct _unary_op {
    nodeptr    operand;
    operator_t op;
} unary_op_t;

typedef struct _operation {
    ir_operation_type_t type;
    union {
#undef S
#define S(I, T) T I;
        IROPERATIONTYPES(S)
#undef S
    };
} operation_t;

OPTDEF(operation_t);
typedef DA(operation_t) operations_t;

typedef struct _ir_function {
    slice_t      name;
    nodeptr      syntax_node;
    nodeptr      module;
    namespace_t  parameters;
    nodeptr      return_type;
    operations_t operations;
} ir_function_t;

typedef struct _ir_module {
    slice_t      name;
    nodeptr      syntax_node;
    nodeptr      program;
    namespace_t  variables;
    nodeptrs     functions;
    operations_t operations;
} ir_module_t;

typedef struct _ir_program {
    slice_t      name;
    nodeptr      syntax_node;
    namespace_t  variables;
    nodeptrs     functions;
    nodeptrs     modules;
    operations_t operations;
} ir_program_t;

typedef enum _ir_node_type {
    IRN_Function,
    IRN_Module,
    IRN_Program,
} ir_node_type_t;

typedef struct _ir_node {
    ir_node_type_t type;
    size_t         ix;
    nodeptr        bound_type;
    union {
        ir_function_t function;
        ir_module_t   module;
        ir_program_t  program;
    };
} ir_node_t;

OPTDEF(ir_node_t);
typedef DA(ir_node_t) ir_nodes_t;

typedef struct _loop_descriptor {
    slice_t  name;
    uint64_t loop_begin;
    uint64_t loop_end;
} loop_descriptor_t;

typedef struct _ir_defer_statement {
    nodeptr  statement;
    uint64_t label;
} ir_defer_statement_t;

typedef DA(ir_defer_statement_t) ir_defer_statements_t;

typedef struct _block_descriptor {
    uint64_t              scope_end_label;
    ir_defer_statements_t defer_stmts;
} block_descriptor_t;

typedef struct _function_descriptor {
    uint64_t end_label;
    nodeptr  return_type;
} function_descriptor_t;

typedef struct _ir_context {
    nodeptr ir_node;
    enum {
        USET_None = 0,
        USET_Function,
        USET_Loop,
        USET_Block,
    } unwind_type;
    union {
        function_descriptor_t function;
        loop_descriptor_t     loop;
        block_descriptor_t    block;
    };
} ir_context_t;

typedef DA(ir_context_t) ir_contexts_t;

typedef struct _ir_generator {
    parser_t     *parser;
    ir_nodes_t    ir_nodes;
    ir_contexts_t ctxs;
} ir_generator_t;

slice_t        operation_type_name(ir_operation_type_t type);
void           operation_list(sb_t *sb, operation_t const *op);
void           generate(ir_generator_t *gen, nodeptr node);
ir_generator_t generate_ir(parser_t *parser, nodeptr n);
void           list(FILE *f, ir_generator_t *gen, nodeptr ir);

#endif /* __IR_H__ */
