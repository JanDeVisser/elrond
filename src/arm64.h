/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __ARM64_H__
#define __ARM64_H__

#include <stdint.h>
#include <stdio.h>

#include "ir.h"
#include "node.h"
#include "parser.h"
#include "slice.h"

typedef struct _register_allocation {
    int      reg;
    intptr_t num_regs;
} arm64_register_allocation_t;

typedef DA(arm64_register_allocation_t) arm64_register_allocations_t;

typedef uint64_t arm64_var_pointer_t;

typedef struct _value_stack_entry {
    enum {
        VSE_RegisterAllocation,
        VSE_VarPointer,
    } type;
    union {
        arm64_register_allocation_t register_allocation;
        arm64_var_pointer_t         var_pointer;
    };
} arm64_value_stack_entry_t;

typedef DA(arm64_value_stack_entry_t) arm64_value_stack_entries_t;

typedef struct _arm64_variable {
    slice_t  name;
    uint64_t depth;
} arm64_variable_t;

typedef DA(arm64_variable_t) arm64_variables_t;

typedef enum {
    CS_Code,
    CS_Prolog,
    CS_Epilog,
    CS_Data,
    CS_Text,
    CS_Max,
} arm64_code_section_t;

typedef struct _arm64_function {
    slice_t                     name;
    nodeptr                     function;
    struct _arm64_object       *object;
    uint64_t                    stack_depth;
    arm64_variables_t           variables;
    sb_t                        sections[CS_Max];
    int                         active;
    uint32_t                    regs;
    uint32_t                    save_regs;
    arm64_value_stack_entries_t stack;
} arm64_function_t;

typedef DA(arm64_function_t) arm64_functions_t;

void arm64_add_instruction(arm64_function_t *f, slice_t mnemonic, char const *param_fmt, ...)
    __attribute__((__format__(printf, 3, 4)));
void                        arm64_add_instruction_param(arm64_function_t *f, slice_t mnemonic, slice_t param);
void                        arm64_add_simple_instruction(arm64_function_t *f, slice_t mnemonic);
void                        arm64_add_vtext(arm64_function_t *f, char const *fmt, va_list args);
void                        arm64_add_text(arm64_function_t *f, char const *fmt, ...);
void                        arm64_add_label(arm64_function_t *f, slice_t label);
void                        arm64_add_directive_f(arm64_function_t *f, slice_t directive, slice_t args);
void                        arm64_add_comment(arm64_function_t *f, slice_t comment);
bool                        arm64_empty(arm64_function_t *f);
bool                        arm64_has_text(arm64_function_t *f);
void                        arm64_analyze(arm64_function_t *f, ir_generator_t *gen, operations_t *operations);
void                        arm64_emit_return(arm64_function_t *f);
void                        arm64_skeleton(arm64_function_t *f, ir_generator_t *gen);
arm64_register_allocation_t arm64_push_reg_by_type(arm64_function_t *f, nodeptr type);
arm64_register_allocation_t arm64_push_reg(arm64_function_t *f, size_t size);
arm64_register_allocation_t arm64_pop_reg(arm64_function_t *f);
void                        arm64_push_by_type(arm64_function_t *f, nodeptr type);
void                        arm64_push(arm64_function_t *f, size_t size);
int                         arm64_pop_by_type(arm64_function_t *f, nodeptr type, int target);
int                         arm64_pop(arm64_function_t *f, size_t size, int target);
arm64_var_pointer_t         arm64_deref_by_type(arm64_function_t *f, nodeptr type, int target);
arm64_var_pointer_t         arm64_deref(arm64_function_t *f, size_t, int target);
arm64_var_pointer_t         arm64_assign_by_type(arm64_function_t *f, nodeptr type);
arm64_var_pointer_t         arm64_assign(arm64_function_t *f, size_t);
void                        arm64_function_generate(arm64_function_t *f, ir_generator_t *gen, operations_t *operations);
void                        arm64_function_write(arm64_function_t *func, FILE *file);

#define pop_value(T, f, target) (arm64_pop((f), sizeof(T), (target)))
#define push_value(T, f) (arm64_push((f), sizeof(T)))

typedef struct _arm64_object {
    slice_t           file_name;
    nodeptr           module;
    arm64_functions_t functions;
    sb_t              sections[CS_Max];
    int               active;
    slices_t          strings;
    bool              has_exports;
    bool              has_main;
} arm64_object_t;

typedef DA(arm64_object_t) arm64_objects_t;

void arm64_add_directive_o(arm64_object_t *o, slice_t directive, slice_t args);
int  arm64_add_string(arm64_object_t *o, slice_t str);
void arm64_object_generate(arm64_object_t *o, ir_generator_t *gen);
bool arm64_save_and_assemble(arm64_object_t *o, ir_generator_t *gen);
void arm64_add_data(arm64_object_t *o, slice_t label, bool global, slice_t type, bool is_static, slice_t data);
void arm64_object_write(arm64_object_t *o, FILE *f);
void arm64_binop(arm64_function_t *f, nodeptr lhs, operator_t op, nodeptr rhs);

typedef struct _arm64_executable {
    nodeptr         program;
    arm64_objects_t objects;
} arm64_executable_t;

OPTDEF(arm64_executable_t);

bool                   arm64_executable_generate(arm64_executable_t *exe, ir_generator_t *gen);
opt_arm64_executable_t arm64_generate(ir_generator_t *gen, nodeptr program);

// void                            generate_unary(arm64_function_t *f, Function &function, nodeptr operand, Operator op);
// void                            generate_binop(arm64_function_t *f, Function &function, nodeptr lhs_type, Operator op, nodeptr rhs_type);

#endif /* __ARM64_H__ */
