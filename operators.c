/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "operators.h"
#include "elrondlexer.h"
#include "slice.h"

slice_t elrond_keywords[] = {
    C("&="),
    C("-="),
    C("/="),
    C("+="),
    C("%="),
    C("*="),
    C("|="),
    C("<<="),
    C(">>="),
    C("^="),
    C("break"),
    C("::"),
    C("const"),
    C("continue"),
    C("defer"),
    C("else"),
    C("@embed"),
    C("enum"),
    C("=="),
    C("error"),
    C("false"),
    C("->"),
    C("for"),
    C("func"),
    C(">="),
    C("if"),
    C("import"),
    C("@include"),
    C("<="),
    C("&&"),
    C("||"),
    C("loop"),
    C("!="),
    C("null"),
    C("public"),
    C("range"),
    C("return"),
    C("<<"),
    C(">>"),
    C("sizeof"),
    C("struct"),
    C("true"),
    C("var"),
    C("while"),
    C("yield"),
    C(""),
};

operator_def_t operators[OP_MAX] = {
    [OP_Add] = {
        .op = OP_Add,
        .kind = TK_Symbol,
        .sym = '+',
        .precedence = 11,
    },
    [OP_Assign] = {
        .op = OP_Assign,
        .kind = TK_Symbol,
        .sym = '=',
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignAnd] = {
        .op = OP_AssignAnd,
        .assignment_op_for = OPTVAL(operator_t, OP_LogicalAnd),
        .kind = TK_Keyword,
        .keyword = KW_AssignAnd,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignDecrement] = {
        .op = OP_AssignDecrement,
        .assignment_op_for = OPTVAL(operator_t, OP_Subtract),
        .kind = TK_Keyword,
        .keyword = KW_AssignDecrement,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignDivide] = {
        .op = OP_AssignDivide,
        .assignment_op_for = OPTVAL(operator_t, OP_Divide),
        .kind = TK_Keyword,
        .keyword = KW_AssignDivide,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AddressOf] = {
        .op = OP_AddressOf,
        .kind = TK_Symbol,
        .sym = '&',
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignIncrement] = {
        .op = OP_AssignIncrement,
        .assignment_op_for = OPTVAL(operator_t, OP_Add),
        .kind = TK_Keyword,
        .keyword = KW_AssignIncrement,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignModulo] = {
        .op = OP_AssignModulo,
        .assignment_op_for = OPTVAL(operator_t, OP_Modulo),
        .kind = TK_Keyword,
        .keyword = KW_AssignModulo,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignMultiply] = {
        .op = OP_AssignMultiply,
        .assignment_op_for = OPTVAL(operator_t, OP_Multiply),
        .kind = TK_Keyword,
        .keyword = KW_AssignMultiply,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignOr] = {
        .op = OP_AssignOr,
        .assignment_op_for = OPTVAL(operator_t, OP_LogicalOr),
        .kind = TK_Keyword,
        .keyword = KW_AssignOr,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignShiftLeft] = {
        .op = OP_AssignShiftLeft,
        .assignment_op_for = OPTVAL(operator_t, OP_ShiftLeft),
        .kind = TK_Keyword,
        .keyword = KW_AssignShiftLeft,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignShiftRight] = {
        .op = OP_AssignShiftRight,
        .assignment_op_for = OPTVAL(operator_t, OP_ShiftRight),
        .kind = TK_Keyword,
        .keyword = KW_AssignShiftRight,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_AssignXor] = {
        .op = OP_AssignXor,
        .assignment_op_for = OPTVAL(operator_t, OP_BinaryXor),
        .kind = TK_Keyword,
        .keyword = KW_AssignXor,
        .precedence = 1,
        .position = POS_Infix,
        .associativity = ASSOC_Right,
    },
    [OP_BinaryInvert] = {
        .op = OP_BinaryInvert,
        .kind = TK_Symbol,
        .sym = '~',
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_Call] = {
        .op = OP_Call,
        .kind = TK_Symbol,
        .sym = '(',
        .precedence = 15,
    },
    [OP_CallClose] = {
        .op = OP_CallClose,
        .kind = TK_Symbol,
        .sym = ')',
        .precedence = 15,
        .position = POS_Closing,
    },
    [OP_Cast] = {
        .op = OP_Cast,
        .kind = TK_Keyword,
        .keyword = KW_Cast,
        .precedence = 14,
    },
    [OP_Divide] = {
        .op = OP_Divide,
        .kind = TK_Symbol,
        .sym = '/',
        .precedence = 12,
    },
    [OP_Equals] = {
        .op = OP_Equals,
        .kind = TK_Keyword,
        .keyword = KW_Equals,
        .precedence = 8,
    },
    [OP_Greater] = {
        .op = OP_Greater,
        .kind = TK_Symbol,
        .sym = '>',
        .precedence = 8,
    },
    [OP_GreaterEqual] = {
        .op = OP_GreaterEqual,
        .kind = TK_Keyword,
        .keyword = KW_GreaterEqual,
        .precedence = 8,
    },
    [OP_Idempotent] = {
        .op = OP_Idempotent,
        .kind = TK_Symbol,
        .sym = '+',
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_Length] = {
        .op = OP_Length,
        .kind = TK_Symbol,
        .sym = '#',
        .precedence = 9,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_Less] = {
        .op = OP_Less,
        .kind = TK_Symbol,
        .sym = '<',
        .precedence = 8,
    },
    [OP_LessEqual] = {
        .op = OP_LessEqual,
        .kind = TK_Keyword,
        .keyword = KW_LessEqual,
        .precedence = 8,
    },
    [OP_LogicalAnd] = {
        .op = OP_LogicalAnd,
        .kind = TK_Keyword,
        .keyword = KW_LogicalAnd,
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_LogicalInvert] = {
        .op = OP_LogicalInvert,
        .kind = TK_Symbol,
        .sym = '!',
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_LogicalOr] = {
        .op = OP_LogicalOr,
        .kind = TK_Keyword,
        .keyword = KW_LogicalOr,
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_MemberAccess] = {
        .op = OP_MemberAccess,
        .kind = TK_Symbol,
        .sym = '.',
        .precedence = 15,
    },
    [OP_Modulo] = {
        .op = OP_Modulo,
        .kind = TK_Symbol,
        .sym = '%',
        .precedence = 12,
    },
    [OP_Multiply] = {
        .op = OP_Multiply,
        .kind = TK_Symbol,
        .sym = '*',
        .precedence = 12,
    },
    [OP_Negate] = {
        .op = OP_Negate,
        .kind = TK_Symbol,
        .sym = '-',
        .precedence = 14,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_NotEqual] = {
        .op = OP_NotEqual,
        .kind = TK_Keyword,
        .keyword = KW_NotEqual,
        .precedence = 8,
    },
    [OP_Range] = {
        .op = OP_Range,
        .kind = TK_Keyword,
        .keyword = KW_Range,
        .precedence = 2,
    },
    [OP_Sequence] = {
        .op = OP_Sequence,
        .kind = TK_Symbol,
        .sym = ',',
        .precedence = 1,
    },
    [OP_ShiftLeft] = {
        .op = OP_ShiftLeft,
        .kind = TK_Keyword,
        .keyword = KW_ShiftLeft,
        .precedence = 10,
    },
    [OP_ShiftRight] = {
        .op = OP_ShiftRight,
        .kind = TK_Keyword,
        .keyword = KW_ShiftRight,
        .precedence = 10,
    },
    [OP_Sizeof] = {
        .op = OP_Sizeof,
        .kind = TK_Keyword,
        .keyword = KW_Sizeof,
        .precedence = 9,
        .position = POS_Prefix,
        .associativity = ASSOC_Right,
    },
    [OP_Subscript] = {
        .op = OP_Subscript,
        .kind = TK_Symbol,
        .sym = '[',
        .precedence = 15,
        .position = POS_Postfix,
    },
    [OP_SubscriptClose] = {
        .op = OP_Subscript,
        .kind = TK_Symbol,
        .sym = ']',
        .precedence = 15,
        .position = POS_Closing,
    },
    [OP_Subtract] = {
        .op = OP_Subtract,
        .kind = TK_Symbol,
        .sym = '-',
        .precedence = 11,
    },
};

char const *operator_name(operator_t op)
{
    switch (op) {
#undef S
#define S(O)     \
    case OP_##O: \
        return #O;
        OPERATORS(S)
#undef S
    default:
        UNREACHABLE();
    }
}

binding_power_t binding_power(operator_def_t op)
{
    switch (op.position) {
    case POS_Infix:
        switch (op.associativity) {
        case ASSOC_Left:
            return (binding_power_t) { .left = op.precedence * 2 - 1, .right = op.precedence * 2 };
        case ASSOC_Right:
            return (binding_power_t) { .left = op.precedence * 2, .right = op.precedence * 2 - 1 };
        };
    case POS_Prefix:
        return (binding_power_t) { .left = -1, .right = op.precedence * 2 - 1 };
    case POS_Postfix:
        return (binding_power_t) { .left = op.precedence * 2 - 1, .right = -1 };
    case POS_Closing:
        return (binding_power_t) { .left = -1, .right = -1 };
    }
}
