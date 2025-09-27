/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "slice.h"

#ifndef __ELRONDLEXER_H__
#define __ELRONDLEXER_H__

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

#endif /* __ELRONDLEXER_H__ */
