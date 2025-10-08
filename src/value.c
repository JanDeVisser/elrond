/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>

#include "node.h"
#include "type.h"
#include "value.h"

static opt_long   value_as_signed(value_t val);
static opt_ulong  value_as_unsigned(value_t val);
static opt_double value_as_double(value_t val);

value_t make_value_void()
{
    return (value_t) {
        .type = Null,
    };
}

value_t make_value_from_string(slice_t str)
{
    return (value_t) { .type = String, .slice = str };
}

opt_value_t make_value_from_signed(nodeptr type, int64_t v)
{
    type_t *t = get_type(type);
    value_t ret = { .type = type };
    switch (t->kind) {
    case TYPK_IntType: {
        if ((v > 0 && (uint64_t) v > t->int_type.max_value) || v < t->int_type.min_value) {
            return OPTNULL(value_t);
        }
        switch (t->int_type.code) {
#undef S
#define S(W)                        \
    case IC_U##W:                   \
        ret.u##W = (uint##W##_t) v; \
        break;                      \
    case IC_I##W:                   \
        ret.i##W = (int##W##_t) v;  \
        break;
            INTWIDTHS(S)
#undef S
        default:
            UNREACHABLE();
        }
        return OPTVAL(value_t, ret);
    }
    case TYPK_FloatType:
        return make_value_from_double(type, (double) v);
    case TYPK_BoolType:
        ret.boolean = v != 0;
        return OPTVAL(value_t, ret);
    default:
        return OPTNULL(value_t);
    }
}

opt_value_t make_value_from_unsigned(nodeptr type, uint64_t v)
{
    type_t *t = get_type(type);
    value_t ret = { .type = type };
    switch (t->kind) {
    case TYPK_IntType: {
        if (v > t->int_type.max_value) {
            return OPTNULL(value_t);
        }
        switch (t->int_type.code) {
#undef S
#define S(W)                        \
    case IC_U##W:                   \
        ret.u##W = (uint##W##_t) v; \
        break;                      \
    case IC_I##W:                   \
        ret.i##W = (int##W##_t) v;  \
        break;
            INTWIDTHS(S)
#undef S
        default:
            UNREACHABLE();
        }
        return OPTVAL(value_t, ret);
    }
    case TYPK_FloatType:
        return make_value_from_double(type, (double) v);
    case TYPK_BoolType:
        ret.boolean = v != 0;
        return OPTVAL(value_t, ret);
    default:
        return OPTNULL(value_t);
    }
}

opt_value_t make_value_from_double(nodeptr type, double d)
{
    type_t *t = get_type(type);
    value_t ret = { .type = type };
    switch (t->kind) {
    case TYPK_IntType: {
        if (d < (double) t->int_type.min_value || d > (double) t->int_type.max_value) {
            return OPTNULL(value_t);
        }
        switch (t->int_type.code) {
#undef S
#define S(W)                        \
    case IC_U##W:                   \
        ret.u##W = (uint##W##_t) d; \
        break;                      \
    case IC_I##W:                   \
        ret.i##W = (int##W##_t) d;  \
        break;
            INTWIDTHS(S)
#undef S
        default:
            UNREACHABLE();
        }
        return OPTVAL(value_t, ret);
    }
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            ret.f32 = (float) d;
            break;
        case FW_64:
            ret.f64 = d;
            break;
        default:
            UNREACHABLE();
        }
        return OPTVAL(value_t, ret);
    default:
        return OPTNULL(value_t);
    }
}

opt_long value_as_signed(value_t val)
{
    type_t *t = get_type(val.type);
    switch (t->kind) {
    case TYPK_IntType:
        switch (t->int_type.code) {
#undef S
#define S(W)                                         \
    case IC_I##W:                                    \
        return OPTVAL(long, (int64_t) val.i##W);     \
    case IC_U##W: {                                  \
        uint64_t v = (uint64_t) val.u##W;            \
        if (v > get_type(I64)->int_type.max_value) { \
            return OPTNULL(long);                    \
        }                                            \
        return OPTVAL(long, (int64_t) val.u##W);     \
    }
            INTWIDTHS(S)
#undef S
        default:
            printf("%d\n", t->int_type.code);
            UNREACHABLE();
        }
    case TYPK_FloatType: {
        double d = TRYOPT_ADAPT(double, value_as_double(val), OPTNULL(long));
        if (d < (double) (get_type(U64)->int_type.min_value) || d > (double) (get_type(U64)->int_type.max_value)) {
            return OPTNULL(long);
        }
        return OPTVAL(long, (int64_t) d);
    }
    case TYPK_BoolType:
        return OPTVAL(long, val.boolean ? 1 : 0);
    default:
        return OPTNULL(long);
    }
}

opt_ulong value_as_unsigned(value_t val)
{
    type_t *t = get_type(val.type);
    switch (t->kind) {
    case TYPK_IntType:
        switch (t->int_type.code) {
#undef S
#define S(W)                                       \
    case IC_U##W:                                  \
        return (uint64_t) val.u##W;                \
    case IC_I##W: {                                \
        int64_t v = (int64_t) val.i##W;            \
        if (v < 0) {                               \
            return OPTNULL(long);                  \
        }                                          \
        return OPTVAL(ulong, (uint64_t) val.u##W); \
    }                                              \
        INTWIDTHS(S)
#undef S
        default:
            UNREACHABLE();
        }
    case TYPK_FloatType: {
        double d = TRYOPT_ADAPT(double, value_as_double(val), OPTNULL(ulong));
        if (d < 0 || d > (double) (get_type(U64)->int_type.max_value)) {
            return OPTNULL(ulong);
        }
        return OPTVAL(ulong, (int64_t) d);
    }
    case TYPK_BoolType:
        return OPTVAL(ulong, val.boolean ? 1 : 0);
    default:
        return OPTNULL(ulong);
    }
}

opt_double value_as_double(value_t val)
{
    type_t *t = get_type(val.type);
    switch (t->kind) {
    case TYPK_IntType:
        switch (t->int_type.code) {
#undef S
#define S(W)                                      \
    case IC_U##W:                                 \
        return OPTVAL(double, (double) val.u##W); \
    case IC_I##W:                                 \
        return OPTVAL(double, (double) val.i##W);
            INTWIDTHS(S)
#undef S
        default:
            UNREACHABLE();
        }
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            return OPTVAL(double, (double) val.f32);
        case FW_64:
            return OPTVAL(double, val.f64);
        default:
            UNREACHABLE();
        }
    case TYPK_BoolType:
        return OPTVAL(double, val.boolean ? 1.0 : 0.0);
    default:
        return OPTNULL(double);
    }
}

value_t make_value_from_buffer(nodeptr type, void *buf)
{
    type_t *t = get_type(type);
    switch (t->kind) {
    case TYPK_IntType:
        switch (t->int_type.code) {
#undef S
#define S(W, Signed, Code, Fld, T) \
    case Code:                     \
        return (value_t) { .type = type, .Fld = *((T *) buf) };
            VALUE_INTFIELDS(S)
        default:
            UNREACHABLE();
        }
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            return (value_t) { .type = type, .f32 = *((float *) buf) };
        case FW_64:
            return (value_t) { .type = type, .f64 = *((double *) buf) };
        default:
            UNREACHABLE();
        }
    case TYPK_BoolType:
        return (value_t) { .type = type, .boolean = *((bool *) buf) };
    case TYPK_SliceType:
        return (value_t) { .type = type, .slice = *((slice_t *) buf) };
    case TYPK_DynArrayType:
        return (value_t) { .type = type, .da = *((generic_da_t *) buf) };
    default:
        UNREACHABLE();
    }
}

void value_print(sb_t *sb, value_t value)
{
    type_t *t = get_type(value.type);
    switch (t->kind) {
    case TYPK_BoolType:
        sb_printf(sb, "%s", (value.boolean) ? "true" : "false");
        break;
    case TYPK_VoidType:
        sb_append_cstr(sb, "(null)");
        break;
    case TYPK_IntType:
        switch (t->int_type.code) {
        case IC_U8:
            sb_printf(sb, "%ud", value.u8);
            break;
        case IC_U16:
            sb_printf(sb, "%ud", value.u16);
            break;
        case IC_U32:
            sb_printf(sb, "%ud", value.u32);
            break;
        case IC_U64:
            sb_printf(sb, "%lld", value.u64);
            break;
        case IC_I8:
            sb_printf(sb, "%d", value.i8);
            break;
        case IC_I16:
            sb_printf(sb, "%d", value.i16);
            break;
        case IC_I32:
            sb_printf(sb, "%d", value.i32);
            break;
        case IC_I64:
            sb_printf(sb, "%lld", value.i64);
            break;
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_FloatType:
        switch (t->float_width) {
        case FW_32:
            sb_printf(sb, "%f", value.f32);
            break;
        case FW_64:
            sb_printf(sb, "%f", value.f64);
            break;
        }
        break;
    case TYPK_SliceType: {
        sb_t escaped = { 0 };
        sb_escape(&escaped, value.slice);
        sb_printf(sb, SL, SLARG(sb_as_slice(escaped)));
    } break;
    default:
        UNREACHABLE();
    }
}

opt_value_t value_coerce(value_t value, nodeptr type)
{
    if (value.type.value == type.value) {
        return OPTVAL(value_t, value);
    }
    type_t *from = get_type(value.type);
    value_t ret = { .type = type };
    switch (from->kind) {
    case TYPK_IntType:
        if (from->int_type.is_signed) {
            int64_t v = TRYOPT_ADAPT(long, value_as_signed(value), OPTNULL(value_t));
            return make_value_from_signed(type, v);
        } else {
            uint64_t v = TRYOPT_ADAPT(ulong, value_as_unsigned(value), OPTNULL(value_t));
            return make_value_from_unsigned(type, v);
        }
    case TYPK_FloatType: {
        double d = TRYOPT_ADAPT(double, value_as_double(value), OPTNULL(value_t));
        return make_value_from_double(type, d);
    }
    default:
        return OPTNULL(value_t);
    }
    return OPTVAL(value_t, ret);
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
    type_t *t1 = get_type(v1.type);
    switch (t1->kind) {
    case TYPK_IntType:
        if (t1->int_type.is_signed) {
            int64_t i1 = UNWRAP(long, value_as_signed(v1));
            int64_t i2 = TRYOPT_ADAPT(long, value_as_signed(v2), OPTNULL(value_t));
            return make_value_from_signed(v1.type, i1 + i2);
        } else {
            uint64_t i1 = UNWRAP(long, value_as_signed(v1));
            uint64_t i2 = TRYOPT_ADAPT(ulong, value_as_unsigned(v2), OPTNULL(value_t));
            return make_value_from_unsigned(v1.type, i1 + i2);
        }
    case TYPK_FloatType: {
        double d1 = UNWRAP(double, value_as_double(v1));
        double d2 = TRYOPT_ADAPT(double, value_as_double(v2), OPTNULL(value_t));
        return make_value_from_double(v1.type, d1 + d2);
    }
    default:
        return OPTNULL(value_t);
    }
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

opt_value_t evaluate_CallClose(value_t v1, value_t v2)
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
    type_t *t1 = get_type(v1.type);
    switch (t1->kind) {
    case TYPK_IntType:
        if (t1->int_type.is_signed) {
            int64_t i1 = UNWRAP(long, value_as_signed(v1));
            int64_t i2 = TRYOPT_ADAPT(long, value_as_signed(v2), OPTNULL(value_t));
            return make_value_from_signed(v1.type, i1 / i2);
        } else {
            uint64_t i1 = UNWRAP(long, value_as_signed(v1));
            uint64_t i2 = TRYOPT_ADAPT(ulong, value_as_unsigned(v2), OPTNULL(value_t));
            return make_value_from_unsigned(v1.type, i1 / i2);
        }
    case TYPK_FloatType: {
        double d1 = UNWRAP(double, value_as_double(v1));
        double d2 = TRYOPT_ADAPT(double, value_as_double(v2), OPTNULL(value_t));
        return make_value_from_double(v1.type, d1 / d2);
    }
    default:
        return OPTNULL(value_t);
    }
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
    type_t *t1 = get_type(v1.type);
    switch (t1->kind) {
    case TYPK_IntType:
        if (t1->int_type.is_signed) {
            int64_t i1 = UNWRAP(long, value_as_signed(v1));
            int64_t i2 = TRYOPT_ADAPT(long, value_as_signed(v2), OPTNULL(value_t));
            return make_value_from_signed(v1.type, i1 * i2);
        } else {
            uint64_t i1 = UNWRAP(long, value_as_signed(v1));
            uint64_t i2 = TRYOPT_ADAPT(ulong, value_as_unsigned(v2), OPTNULL(value_t));
            return make_value_from_unsigned(v1.type, i1 * i2);
        }
    case TYPK_FloatType: {
        double d1 = UNWRAP(double, value_as_double(v1));
        double d2 = TRYOPT_ADAPT(double, value_as_double(v2), OPTNULL(value_t));
        return make_value_from_double(v1.type, d1 * d2);
    }
    default:
        return OPTNULL(value_t);
    }
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

opt_value_t evaluate_SubscriptClose(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}

opt_value_t evaluate_Subtract(value_t v1, value_t v2)
{
    type_t *t1 = get_type(v1.type);
    switch (t1->kind) {
    case TYPK_IntType:
        if (t1->int_type.is_signed) {
            int64_t i1 = UNWRAP(long, value_as_signed(v1));
            int64_t i2 = TRYOPT_ADAPT(long, value_as_signed(v2), OPTNULL(value_t));
            return make_value_from_signed(v1.type, i1 - i2);
        } else {
            uint64_t i1 = UNWRAP(long, value_as_signed(v1));
            uint64_t i2 = TRYOPT_ADAPT(ulong, value_as_unsigned(v2), OPTNULL(value_t));
            return make_value_from_unsigned(v1.type, i1 - i2);
        }
    case TYPK_FloatType: {
        double d1 = UNWRAP(double, value_as_double(v1));
        double d2 = TRYOPT_ADAPT(double, value_as_double(v2), OPTNULL(value_t));
        return make_value_from_double(v1.type, d1 - d2);
    }
    default:
        return OPTNULL(value_t);
    }
}

opt_value_t evaluate_MAX(value_t v1, value_t v2)
{
    (void) v1;
    (void) v2;
    return OPTNULL(value_t);
}
