/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "value.h"
#include "type.h"
#include <stdio.h>

value_t make_value_from_string(slice_t str)
{
    return (value_t) { .type = String, .slice = str };
}

void value_print(FILE *f, value_t value)
{
    type_t *t = get_type(value.type);
    switch (t->kind) {
    case TYPK_BoolType:
        fprintf(f, "%s", (value.boolean) ? "true" : "false");
        break;
    case TYPK_VoidType:
        fputs("(null)", f);
        break;
    case TYPK_IntType:
        switch (t->int_type.code) {
        case IC_U8:
            fprintf(f, "%ud", value.u8);
            break;
        case IC_U16:
            fprintf(f, "%ud", value.u16);
            break;
        case IC_U32:
            fprintf(f, "%ud", value.u32);
            break;
        case IC_U64:
            fprintf(f, "%lld", value.u64);
            break;
        case IC_I8:
            fprintf(f, "%d", value.i8);
            break;
        case IC_I16:
            fprintf(f, "%d", value.i16);
            break;
        case IC_I32:
            fprintf(f, "%d", value.i32);
            break;
        case IC_I64:
            fprintf(f, "%lld", value.i64);
            break;
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            fprintf(f, "%f", value.f32);
            break;
        case FW_64:
            fprintf(f, "%f", value.f64);
            break;
        }
        break;
    case TYPK_SliceType: {
        sb_t escaped = { 0 };
        sb_escape(&escaped, value.slice);
        fprintf(f, SL, SLARG(sb_as_slice(escaped)));
    } break;
    default:
        UNREACHABLE();
    }
}

#undef S
#define S(O) \
    static opt_value_t evaluate_##O(value_t v1, value_t v2);
OPERATORS(S);
#undef S

opt_value_t evaluate(value_t v1, operator_t op, value_t v2)
{
    switch (op) {
#undef S
#define S(O)     \
    case OP_##O: \
        return evaluate_##O(v1, v2);
        OPERATORS(S);
#undef S
    default:
        UNREACHABLE();
    }
}

opt_value_t evaluate_Add(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AddressOf(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Assign(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignAnd(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignDecrement(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignDivide(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignIncrement(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignModulo(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignMultiply(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignOr(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignShiftLeft(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignShiftRight(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_AssignXor(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_BinaryAnd(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_BinaryInvert(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_BinaryOr(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_BinaryXor(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Call(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Cast(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Divide(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Equals(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Greater(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_GreaterEqual(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Idempotent(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Length(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Less(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_LessEqual(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_LogicalAnd(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_LogicalInvert(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_LogicalOr(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_MemberAccess(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Modulo(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Multiply(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Negate(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_NotEqual(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Range(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Sequence(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_ShiftLeft(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_ShiftRight(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Sizeof(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Subscript(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Subtract(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
};
