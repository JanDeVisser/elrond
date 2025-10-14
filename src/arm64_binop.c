/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "arm64.h"
#include "da.h"
#include "node.h"
#include "type.h"

typedef void (*op_gen_t)(arm64_function_t *, nodeptr, nodeptr);

typedef op_gen_t op_gens_t[OP_MAX];

void gen_int_BinaryInvert(arm64_function_t *function, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(bool, function, 0);
    arm64_add_instruction_param(function, C("mvn"), C("x0,x0"));
    push_value(bool, function);
}

void gen_bool_LogicalInvert(arm64_function_t *function, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(bool, function, 0);
    arm64_add_instruction_param(function, C("eor"), C("w0,w0,#0x01")); // a is 0b00000001 (a was true) or 0b00000000 (a was false
    push_value(bool, function);
}

void gen_int_Negate(arm64_function_t *function, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(bool, function, 0);
    arm64_add_instruction_param(function, C("neg"), C("x0,x0"));
    push_value(bool, function);
}

#define INT_OP(opname, opcode)                                           \
    void gen_int_##opname(arm64_function_t *f, nodeptr lhs, nodeptr rhs) \
    {                                                                    \
        (void) lhs;                                                      \
        (void) rhs;                                                      \
        pop_value(int64_t, f, 1);                                        \
        pop_value(int64_t, f, 0);                                        \
        arm64_add_instruction_param(f, C(opcode), C("x0,x0,x1"));        \
        push_value(int64_t, f);                                          \
    }

#define NEQ ""
#define EQ "b.eq    1f"

#define INT_CMP_OP(opname, BSigned, BUnsigned, Eq)                       \
    void gen_int_##opname(arm64_function_t *f, nodeptr lhs, nodeptr rhs) \
    {                                                                    \
        (void) lhs;                                                      \
        (void) rhs;                                                      \
        pop_value(int64_t, f, 1);                                        \
        pop_value(int64_t, f, 0);                                        \
        arm64_add_text(f,                                                \
            "    cmp     x0,x1\n"                                        \
            "    %s      1f\n"                                           \
            "    " Eq "\n"                                               \
            "    mov     w0,wzr\n"                                       \
            "    b       2f\n"                                           \
            "1:\n"                                                       \
            "    mov     w0,#0x01\n"                                     \
            "2:\n",                                                      \
            (get_type(lhs)->int_type.is_signed) ? BSigned : BUnsigned);  \
        push_value(bool, f);                                             \
    }

#define BOOL_OP(opname, opcode)                                           \
    void gen_bool_##opname(arm64_function_t *f, nodeptr lhs, nodeptr rhs) \
    {                                                                     \
        (void) lhs;                                                       \
        (void) rhs;                                                       \
        pop_value(bool, f, 1);                                            \
        pop_value(bool, f, 0);                                            \
        arm64_add_instruction_param(f, C(opcode), C("x0,x0,x1"));         \
        push_value(bool, f);                                              \
    }

INT_OP(Add, "add")
INT_OP(BinaryAnd, "and")
INT_OP(BinaryOr, "orr")
INT_OP(BinaryXor, "eor")
INT_CMP_OP(Equals, "b.eq", "b.eq", NEQ)
INT_CMP_OP(Greater, "b.gt", "b.hi", NEQ)
INT_CMP_OP(GreaterEqual, "b.gt", "b.hi", EQ)
INT_CMP_OP(Less, "b.lt", "b.lo", NEQ)
INT_CMP_OP(LessEqual, "b.lt", "b.lo", EQ)
BOOL_OP(LogicalAnd, "and")
BOOL_OP(LogicalOr, "orr")
INT_CMP_OP(NotEqual, "b.neq", "b.neq", NEQ)
INT_OP(Subtract, "sub")

void gen_int_Divide(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(int64_t, f, 1);
    arm64_add_text(f,
        "    cmp     x1,xzr\n"
        "    b.eq    _$divide_by_zero\n");
    pop_value(int64_t, f, 0);
    arm64_add_instruction_param(f, C("sdiv"), C("x0,x0,x1"));
    push_value(int64_t, f);
}

void gen_uint_Multiply(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(int64_t, f, 1);
    arm64_add_instruction_param(f, C("umull"), C("x0,x0,x1"));
    push_value(int64_t, f);
}

void gen_int_Multiply(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(int64_t, f, 1);
    arm64_add_text(f,
        "    cmp     x1,xzr\n"
        "    b.eq    _$divide_by_zero\n");
    pop_value(int64_t, f, 0);
    arm64_add_instruction_param(f, C("smull"), C("x0,x0,x1"));
    push_value(int64_t, f);
}

void gen_uint_Divide(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(int64_t, f, 1);
    arm64_add_text(f,
        "    cmp     x1,xzr\n"
        "    b.eq    _$divide_by_zero\n");
    pop_value(int64_t, f, 0);
    arm64_add_instruction_param(f, C("udiv"), C("x0,x0,x1"));
    push_value(int64_t, f);
}

void gen_int_Modulo(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(int64_t, f, 1);
    arm64_add_text(f,
        "    cmp      x1,xzr\n"
        "    b.eq     _$divide_by_zero\n");
    pop_value(int64_t, f, 0);
    arm64_add_text(f,
        "    sdiv     x2,x0,x1\n"
        "    smull    x3,w2,w1\n"
        "    sub      x0,x0,x3\n");
    push_value(int64_t, f);
}

void gen_uint_Modulo(arm64_function_t *f, nodeptr lhs, nodeptr rhs)
{
    (void) lhs;
    (void) rhs;
    pop_value(uint64_t, f, 1);
    arm64_add_text(f,
        "    cmp      x1,xzr\n"
        "    b.eq     _$divide_by_zero\n");
    pop_value(uint64_t, f, 0);
    arm64_add_text(f,
        "    udiv     x2,x0,x1\n"
        "    umull    x3,w2,w1\n"
        "    sub      x0,x0,x3\n");
    push_value(int64_t, f);
}

static op_gens_t op_gens[] = {
    [TYPK_BoolType] = {
        [OP_Equals] = gen_int_Equals,
        [OP_LogicalAnd] = gen_bool_LogicalAnd,
        [OP_LogicalInvert] = gen_bool_LogicalInvert,
        [OP_LogicalOr] = gen_bool_LogicalOr,
        [OP_NotEqual] = gen_int_NotEqual,
    },
    [TYPK_IntType] = {
        [OP_Add] = gen_int_Add,
        [OP_Equals] = gen_int_Equals,
        [OP_BinaryAnd] = gen_int_BinaryAnd,
        [OP_BinaryInvert] = gen_int_BinaryInvert,
        [OP_BinaryOr] = gen_int_BinaryOr,
        [OP_BinaryXor] = gen_int_BinaryXor,
        [OP_Divide] = gen_int_Divide,
        [OP_Greater] = gen_int_Greater,
        [OP_GreaterEqual] = gen_int_GreaterEqual,
        [OP_Less] = gen_int_Less,
        [OP_LessEqual] = gen_int_LessEqual,
        [OP_Modulo] = gen_int_Modulo,
        [OP_Multiply] = gen_int_Multiply,
        [OP_NotEqual] = gen_int_NotEqual,
        [OP_Subscript] = gen_int_Subtract,
    },
    [OP_MAX] = { 0 },
};

void arm64_binop(arm64_function_t *f, nodeptr lhs, operator_t op, nodeptr rhs)
{
    type_t  *lhs_type = get_type(lhs);
    op_gen_t fnc = op_gens[lhs_type->kind][op];
    assert(fnc != NULL);
    fnc(f, lhs, rhs);
}
