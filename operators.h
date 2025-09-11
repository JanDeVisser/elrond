/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "slice.h"

#ifndef __OPERATORS_H__
#define __OPERATORS_H__

typedef enum {
    KW_AssignAnd,
    KW_AssignDecrement,
    KW_AssignDivide,
    KW_AssignIncrement,
    KW_AssignModulo,
    KW_AssignMultiply,
    KW_AssignOr,
    KW_AssignShiftLeft,
    KW_AssignShiftRight,
    KW_AssignXor,
    KW_Break,
    KW_Cast,
    KW_Const,
    KW_Continue,
    KW_Defer,
    KW_Else,
    KW_Embed,
    KW_Enum,
    KW_Equals,
    KW_Error,
    KW_False,
    KW_ForeignLink,
    KW_For,
    KW_Func,
    KW_GreaterEqual,
    KW_If,
    KW_Import,
    KW_Include,
    KW_LessEqual,
    KW_LogicalAnd,
    KW_LogicalOr,
    KW_Loop,
    KW_NotEqual,
    KW_Null,
    KW_Public,
    KW_Range,
    KW_Return,
    KW_ShiftLeft,
    KW_ShiftRight,
    KW_Sizeof,
    KW_Struct,
    KW_True,
    KW_Var,
    KW_While,
    KW_Yield,
} elrondkeyword_t;

extern slice_t elrond_keywords[];

#define keywordcode elrondkeyword_t
#define keywords elrond_keywords
#include "lexer.h"

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

typedef enum {
    OP_Add,
    OP_AddressOf,
    OP_Assign,
    OP_AssignAnd,
    OP_AssignDecrement,
    OP_AssignDivide,
    OP_AssignIncrement,
    OP_AssignModulo,
    OP_AssignMultiply,
    OP_AssignOr,
    OP_AssignShiftLeft,
    OP_AssignShiftRight,
    OP_AssignXor,
    OP_BinaryAnd,
    OP_BinaryInvert,
    OP_BinaryOr,
    OP_BinaryXor,
    OP_Call,
    OP_Cast,
    OP_Divide,
    OP_Equals,
    OP_Greater,
    OP_GreaterEqual,
    OP_Idempotent,
    OP_Length,
    OP_Less,
    OP_LessEqual,
    OP_LogicalAnd,
    OP_LogicalInvert,
    OP_LogicalOr,
    OP_MemberAccess,
    OP_Modulo,
    OP_Multiply,
    OP_Negate,
    OP_NotEqual,
    OP_Range,
    OP_Sequence,
    OP_ShiftLeft,
    OP_ShiftRight,
    OP_Sizeof,
    OP_Subscript,
    OP_Subtract,
} operator_t;

typedef struct _operator_def {
    operator_t  op;
    tokenkind_t kind;
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

#define OPERATORS_SZ 39

extern operator_def_t  operators[];
extern binding_power_t binding_power(operator_def_t op);

#endif /* __OPERATORS_H__ */

#ifdef OPERATORS_IMPLEMENTATION

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

operator_def_t operators[OPERATORS_SZ] = {
    { .op = OP_Add, .kind = TK_Symbol, .sym = '+', .precedence = 11 },
    { .op = OP_Assign, .kind = TK_Symbol, .sym = '=', .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignAnd, .kind = TK_Keyword, .keyword = KW_AssignAnd, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignDecrement, .kind = TK_Keyword, .keyword = KW_AssignDecrement, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignDivide, .kind = TK_Keyword, .keyword = KW_AssignDivide, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AddressOf, .kind = TK_Symbol, .sym = '&', .precedence = 14, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_AssignIncrement, .kind = TK_Keyword, .keyword = KW_AssignIncrement, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignModulo, .kind = TK_Keyword, .keyword = KW_AssignModulo, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignMultiply, .kind = TK_Keyword, .keyword = KW_AssignMultiply, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignOr, .kind = TK_Keyword, .keyword = KW_AssignOr, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignShiftLeft, .kind = TK_Keyword, .keyword = KW_AssignShiftLeft, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignShiftRight, .kind = TK_Keyword, .keyword = KW_AssignShiftRight, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_AssignXor, .kind = TK_Keyword, .keyword = KW_AssignXor, .precedence = 1, .position = POS_Infix, .associativity = ASSOC_Right },
    { .op = OP_BinaryInvert, .kind = TK_Symbol, .sym = '~', .precedence = 14, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_Call, .kind = TK_Symbol, .sym = '(', .precedence = 15 },
    { .op = OP_Call, .kind = TK_Symbol, .sym = ')', .precedence = 15, .position = POS_Closing },
    { .op = OP_Cast, .kind = TK_Keyword, .keyword = KW_Cast, .precedence = 14 },
    { .op = OP_Divide, .kind = TK_Symbol, .sym = '/', .precedence = 12 },
    { .op = OP_Equals, .kind = TK_Keyword, .keyword = KW_Equals, .precedence = 8 },
    { .op = OP_Greater, .kind = TK_Symbol, .sym = '>', .precedence = 8 },
    { .op = OP_GreaterEqual, .kind = TK_Keyword, .keyword = KW_GreaterEqual, .precedence = 8 },
    { .op = OP_Idempotent, .kind = TK_Symbol, .sym = '+', .precedence = 14, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_Length, .kind = TK_Symbol, .sym = '#', .precedence = 9, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_Less, .kind = TK_Symbol, .sym = '<', .precedence = 8 },
    { .op = OP_LessEqual, .kind = TK_Keyword, .keyword = KW_LessEqual, .precedence = 8 },
    { .op = OP_LogicalInvert, .kind = TK_Symbol, .sym = '!', .precedence = 14, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_MemberAccess, .kind = TK_Symbol, .sym = '.', .precedence = 15 },
    { .op = OP_Modulo, .kind = TK_Symbol, .sym = '%', .precedence = 12 },
    { .op = OP_Multiply, .kind = TK_Symbol, .sym = '*', .precedence = 12 },
    { .op = OP_Negate, .kind = TK_Symbol, .sym = '-', .precedence = 14, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_NotEqual, .kind = TK_Keyword, .keyword = KW_NotEqual, .precedence = 8 },
    { .op = OP_Range, .kind = TK_Keyword, .keyword = KW_Range, .precedence = 2 },
    { .op = OP_Sequence, .kind = TK_Symbol, .sym = ',', .precedence = 1 },
    { .op = OP_ShiftLeft, .kind = TK_Keyword, .keyword = KW_ShiftLeft, .precedence = 10 },
    { .op = OP_ShiftRight, .kind = TK_Keyword, .keyword = KW_ShiftRight, .precedence = 10 },
    { .op = OP_Sizeof, .kind = TK_Keyword, .keyword = KW_Sizeof, .precedence = 9, .position = POS_Prefix, .associativity = ASSOC_Right },
    { .op = OP_Subscript, .kind = TK_Symbol, .sym = '[', .precedence = 15, .position = POS_Postfix },
    { .op = OP_Subscript, .kind = TK_Symbol, .sym = ']', .precedence = 15, .position = POS_Closing },
    { .op = OP_Subtract, .kind = TK_Symbol, .sym = '-', .precedence = 11 },
};

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
#undef OPERATORS_IMPLEMENTATION
#endif /* OPERATORS_IMPLEMENTATION */
