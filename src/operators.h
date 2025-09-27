/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#include "elrondlexer.h"
#include "slice.h"

#ifndef __OPERATORS_H__
#define __OPERATORS_H__

typedef int precedence_t;

typedef enum {
    POS_Infix = 0,
    POS_Prefix,
    POS_Postfix,
    POS_Closing,
} position_t;

typedef enum {
    ASSOC_Left = 0,
    ASSOC_Right,
} associativity_t;

typedef enum {
    BO_Add,
    BO_AddressOf,
    BO_BinaryAnd,
    BO_BinaryInvert,
    BO_BinaryOr,
    BO_BinaryXor,
    BO_Call,
    BO_Cast,
    BO_Divide,
    BO_Equals,
    BO_Greater,
    BO_GreaterEqual,
    BO_Idempotent,
    BO_Length,
    BO_Less,
    BO_LessEqual,
    BO_LogicalAnd,
    BO_LogicalInvert,
    BO_LogicalOr,
    BO_MemberAccess,
    BO_Modulo,
    BO_Multiply,
    BO_Negate,
    BO_NotEqual,
    BO_Range,
    BO_Sequence,
    BO_ShiftLeft,
    BO_ShiftRight,
    BO_Sizeof,
    BO_Subscript,
    BO_Subtract,
} binary_operator_t;

typedef enum {
    AO_Assign,
    AO_AssignAnd,
    AO_AssignDecrement,
    AO_AssignDivide,
    AO_AssignIncrement,
    AO_AssignModulo,
    AO_AssignMultiply,
    AO_AssignOr,
    AO_AssignShiftLeft,
    AO_AssignShiftRight,
    AO_AssignXor,
} assignment_operator_t;

#define OPERATORS(S)    \
    S(Add)              \
    S(AddressOf)        \
    S(Assign)           \
    S(AssignAnd)        \
    S(AssignDecrement)  \
    S(AssignDivide)     \
    S(AssignIncrement)  \
    S(AssignModulo)     \
    S(AssignMultiply)   \
    S(AssignOr)         \
    S(AssignShiftLeft)  \
    S(AssignShiftRight) \
    S(AssignXor)        \
    S(BinaryAnd)        \
    S(BinaryInvert)     \
    S(BinaryOr)         \
    S(BinaryXor)        \
    S(Call)             \
    S(CallClose)        \
    S(Cast)             \
    S(Divide)           \
    S(Equals)           \
    S(Greater)          \
    S(GreaterEqual)     \
    S(Idempotent)       \
    S(Length)           \
    S(Less)             \
    S(LessEqual)        \
    S(LogicalAnd)       \
    S(LogicalInvert)    \
    S(LogicalOr)        \
    S(MemberAccess)     \
    S(Modulo)           \
    S(Multiply)         \
    S(Negate)           \
    S(NotEqual)         \
    S(Range)            \
    S(Sequence)         \
    S(ShiftLeft)        \
    S(ShiftRight)       \
    S(Sizeof)           \
    S(Subscript)        \
    S(SubscriptClose)   \
    S(Subtract)         \
    S(MAX)

typedef enum _operator {
#undef S
#define S(O) OP_##O,
    OPERATORS(S)
#undef S
} operator_t;
OPTDEF(operator_t);

typedef struct _operator_def {
    operator_t     op;
    opt_operator_t assignment_op_for;
    tokenkind_t    kind;
    union {
        int             sym;
        elrondkeyword_t keyword;
    };
    precedence_t    precedence;
    position_t      position;
    associativity_t associativity;
} operator_def_t;

OPTDEF(operator_def_t);

typedef struct _binding_power {
    int left;
    int right;
} binding_power_t;

extern operator_def_t  operators[];
extern char const     *operator_name(operator_t op);
extern binding_power_t binding_power(operator_def_t op);

#endif /* __OPERATORS_H__ */
