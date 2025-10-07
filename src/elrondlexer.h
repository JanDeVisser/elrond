/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "slice.h"

#ifndef __ELRONDLEXER_H__
#define __ELRONDLEXER_H__

#define KEYWORDS(S)            \
    S(AssignAnd, "&=")         \
    S(AssignDecrement, "-=")   \
    S(AssignDivide, "/=")      \
    S(AssignIncrement, "+=")   \
    S(AssignModulo, "%=")      \
    S(AssignMultiply, "*=")    \
    S(AssignOr, "|=")          \
    S(AssignShiftLeft, "<<=")  \
    S(AssignShiftRight, ">>=") \
    S(AssignXor, "^=")         \
    S(Break, "break")          \
    S(Cast, "::")              \
    S(Const, "const")          \
    S(Continue, "continue")    \
    S(Defer, "defer")          \
    S(Else, "else")            \
    S(Embed, "@embed")         \
    S(Enum, "enum")            \
    S(Equals, "==")            \
    S(Error, "error")          \
    S(False, "false")          \
    S(ForeignLink, "->")       \
    S(For, "for")              \
    S(Func, "func")            \
    S(GreaterEqual, ">=")      \
    S(If, "if")                \
    S(Import, "import")        \
    S(Include, "@include")     \
    S(LessEqual, "<=")         \
    S(LogicalAnd, "&&")        \
    S(LogicalOr, "||")         \
    S(Loop, "loop")            \
    S(NotEqual, "!=")          \
    S(Null, "null")            \
    S(Public, "public")        \
    S(Range, "range")          \
    S(Return, "return")        \
    S(ShiftLeft, "<<")         \
    S(ShiftRight, ">>")        \
    S(Sizeof, "sizeof")        \
    S(Struct, "struct")        \
    S(True, "true")            \
    S(Var, "var")              \
    S(While, "while")          \
    S(Yield, "yield")          \
    S(Max, "")

typedef enum _elrondkeyword {
#undef S
#define S(K, Str) KW_##K,
    KEYWORDS(S)
#undef S
} elrondkeyword_t;

extern slice_t elrond_keywords[];

#define keywordcode elrondkeyword_t
#define keywords elrond_keywords

#include "lexer.h"

#endif /* __ELRONDLEXER_H__ */
