/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "interpreter.h"
#include "node.h"
#include "type.h"

typedef int64_t (*int_unary_op_fnc_t)(int64_t);
typedef uint64_t (*uint_unary_op_fnc_t)(uint64_t);
typedef double (*double_unary_op_fnc_t)(double);
typedef bool (*bool_unary_op_fnc_t)(bool);
typedef int64_t (*int_binary_op_fnc_t)(int64_t, int64_t);
typedef uint64_t (*uint_binary_op_fnc_t)(uint64_t, uint64_t);
typedef double (*double_binary_op_fnc_t)(double, double);
typedef bool (*bool_binary_op_fnc_t)(bool, bool);

typedef struct _unary_up_fncs {
    int_unary_op_fnc_t    int_fnc;
    uint_unary_op_fnc_t   uint_fnc;
    double_unary_op_fnc_t double_fnc;
    bool_unary_op_fnc_t   bool_fnc;
} unary_op_fncs_t;

typedef struct _binary_op_fncs {
    int_binary_op_fnc_t    int_fnc;
    uint_binary_op_fnc_t   uint_fnc;
    double_binary_op_fnc_t double_fnc;
    bool_binary_op_fnc_t   bool_fnc;
} binary_op_fncs_t;

intptr_t stack_reserve(interp_stack_t *stack, size_t size)
{
    dynarr_ensure(stack, size);
    intptr_t ret = stack->len;
    stack->len = size;
    return ret;
}

intptr_t stack_push(interp_stack_t *stack, void *ptr, size_t size)
{
    intptr_t *p = (intptr_t *) ptr;
    size_t    pushed = 0;
    intptr_t  offset = stack->len * sizeof(intptr_t);
    while (pushed < size) {
        dynarr_append(stack, *p);
        ++p;
        pushed += sizeof(intptr_t);
    }
    return offset;
}

void stack_store(interp_stack_t *stack, void *src, intptr_t offset, size_t size)
{
    intptr_t *p = (intptr_t *) src;
    size_t    stored = 0;
    offset /= sizeof(intptr_t);
    size_t cap = ((offset + size) % sizeof(intptr_t) != 0)
        ? (offset + size) / sizeof(intptr_t) + 1
        : (offset + size) / sizeof(intptr_t);
    dynarr_ensure(stack, cap);
    while (stored < size) {
        stack->items[offset] = *p;
        ++p;
        ++offset;
        stored += sizeof(intptr_t);
    }
}

void stack_load(interp_stack_t *stack, void *dest, intptr_t offset, size_t size)
{
    assert(offset + size <= stack->len * sizeof(intptr_t));
    intptr_t *p = (intptr_t *) dest;
    size_t    loaded = 0;
    offset /= sizeof(intptr_t);
    while (loaded < size) {
        stack->items[offset] = *p;
        ++p;
        ++offset;
        loaded += sizeof(intptr_t);
    }
}

void stack_discard(interp_stack_t *stack, size_t bp)
{
    assert(bp <= stack->len * sizeof(intptr_t));
    if (bp < stack->len * sizeof(intptr_t)) {
        size_t diff = (bp % sizeof(intptr_t) != 0)
            ? bp / sizeof(intptr_t) + 1
            : bp / sizeof(intptr_t);
        stack->len -= diff;
    }
}

void stack_pop(interp_stack_t *stack, void *dest, size_t size)
{
    size_t offset = stack->len - align_at(sizeof(intptr_t), size) / sizeof(intptr_t);
    stack_load(stack, dest, offset, size);
    stack->len = offset;
}

void stack_copy(interp_stack_t *stack, intptr_t dest, intptr_t src, size_t size)
{
    stack_load(stack, (void *) (stack->items + dest / sizeof(intptr_t)), src, size);
}

void stack_copy_and_pop(interp_stack_t *stack, intptr_t dest, size_t size)
{
    size_t offset = stack->len - align_at(sizeof(intptr_t), size) / sizeof(intptr_t);
    stack_copy(stack, dest, offset * sizeof(intptr_t), size);
    stack->len = offset;
}

void stack_push_copy(interp_stack_t *stack, intptr_t src, size_t size)
{
    stack_push(stack, stack->items + src / sizeof(intptr_t), size);
}

intptr_t stack_push_value(interp_stack_t *stack, value_t val)
{
    type_t  *type = get_type(val.type);
    intptr_t ret = stack->len * sizeof(intptr_t);
    switch (type->kind) {
    case TYPK_IntType:
        switch (type->int_type.code) {
        case IC_I8:
            stack_push_T(int8_t, stack, val.i8);
            break;
        case IC_U8:
            stack_push_T(int8_t, stack, val.u8);
            break;
        case IC_I16:
            stack_push_T(int16_t, stack, val.i16);
            break;
        case IC_U16:
            stack_push_T(int16_t, stack, val.u16);
            break;
        case IC_I32:
            stack_push_T(int32_t, stack, val.i32);
            break;
        case IC_U32:
            stack_push_T(int32_t, stack, val.u32);
            break;
        case IC_I64:
            stack_push_T(int64_t, stack, val.i64);
            break;
        case IC_U64:
            stack_push_T(int64_t, stack, val.u64);
            break;
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_FloatType:
        switch (type->float_width) {
        case FW_32:
            stack_push_T(float, stack, val.f32);
            break;
        case FW_64:
            stack_push_T(double, stack, val.f64);
            break;
        default:
            UNREACHABLE();
        }
        break;
    case TYPK_BoolType:
        stack_push_T(bool, stack, val.boolean);
        break;
    case TYPK_SliceType:
        stack_push_T(slice_t, stack, val.slice);
        break;
    case TYPK_VoidType:
        break;
    default:
        UNREACHABLE();
    }
    return ret;
}

intptr_t evaluate_AddressOf(interp_stack_t *stack, nodeptr t1)
{
    (void) t1;
    return (stack->len - 1) * sizeof(intptr_t);
}

intptr_t evaluate_Length(interp_stack_t *stack, nodeptr t1)
{
    type_t *t = get_type(t1);
    switch (t->kind) {
    case TYPK_SliceType: {
        slice_t operand = stack_pop_T(slice_t, stack);
        return stack_push_T(uint64_t, stack, operand.len);
    }
    case TYPK_DynArrayType: {
        generic_da_t operand = stack_pop_T(generic_da_t, stack);
        return stack_push_T(uint64_t, stack, operand.len);
    }
    case TYPK_ArrayType: {
        array_t operand = stack_pop_T(array_t, stack);
        return stack_push_T(uint64_t, stack, operand.size);
    }
    default:
        UNREACHABLE();
    }
}

int64_t int_BinaryInvert(int64_t operand)
{
    return ~operand;
}

uint64_t uint_BinaryInvert(uint64_t operand)
{
    return ~operand;
}

bool bool_LogicalInvert(bool operand)
{
    return !operand;
}

int64_t int_Negate(int64_t operand)
{
    return -operand;
}

double double_Negate(double operand)
{
    return -operand;
}

unary_op_fncs_t unary_op_fncs[] = {
    [OP_BinaryInvert] = { .int_fnc = int_BinaryInvert, .uint_fnc = uint_BinaryInvert, .double_fnc = NULL, .bool_fnc = NULL },
    [OP_LogicalInvert] = { .int_fnc = NULL, .uint_fnc = NULL, .double_fnc = NULL, .bool_fnc = bool_LogicalInvert },
    [OP_Negate] = { .int_fnc = int_Negate, .uint_fnc = NULL, .double_fnc = double_Negate, .bool_fnc = NULL },
};

#define INT_OP(opname, op)                             \
    int64_t int_##opname(int64_t lhs, int64_t rhs)     \
    {                                                  \
        return lhs op rhs;                             \
    }                                                  \
    uint64_t uint_##opname(uint64_t lhs, uint64_t rhs) \
    {                                                  \
        return lhs op rhs;                             \
    }

#define INT_OP_DIVZERO(opname, op)                     \
    int64_t int_##opname(int64_t lhs, int64_t rhs)     \
    {                                                  \
        if (rhs == 0) {                                \
            fprintf(stderr, "Division by zero\n");     \
            abort();                                   \
        }                                              \
        return lhs op rhs;                             \
    }                                                  \
    uint64_t uint_##opname(uint64_t lhs, uint64_t rhs) \
    {                                                  \
        if (rhs == 0) {                                \
            fprintf(stderr, "Division by zero\n");     \
            abort();                                   \
        }                                              \
        return lhs op rhs;                             \
    }

#define BOOL_OP(opname, op)                \
    bool bool_##opname(bool lhs, bool rhs) \
    {                                      \
        return lhs op rhs;                 \
    }

#define DOUBLE_OP(opname, op)                      \
    double double_##opname(double lhs, double rhs) \
    {                                              \
        return lhs op rhs;                         \
    }

#define NUMERIC_OP(opname, op) \
    INT_OP(opname, op)         \
    DOUBLE_OP(opname, op)

#define NUMERIC_BOOL_OP(opname, op) \
    NUMERIC_OP(opname, op)          \
    BOOL_OP(opname, op)

NUMERIC_OP(Add, +)
INT_OP(BinaryAnd, &)
INT_OP(BinaryOr, |)
INT_OP(BinaryXor, ^)
INT_OP_DIVZERO(Divide, /)
NUMERIC_BOOL_OP(Equals, ==)
NUMERIC_OP(Greater, >)
NUMERIC_OP(GreaterEqual, >=)
NUMERIC_OP(Less, >)
NUMERIC_OP(LessEqual, <=)
BOOL_OP(LogicalAnd, &&)
BOOL_OP(LogicalOr, ||)
INT_OP_DIVZERO(Modulo, %)
NUMERIC_OP(Multiply, *)
NUMERIC_BOOL_OP(NotEqual, !=)
NUMERIC_OP(Subtract, -)

double double_Divide(double lhs, double rhs)
{
    if (rhs == 0) {
        fprintf(stderr, "Division by zero\n");
        abort();
    }
    return lhs / rhs;
}

double double_Modulo(double lhs, double rhs)
{
    if (rhs == 0) {
        fprintf(stderr, "Division by zero\n");
        abort();
    }
    return fmod(lhs, rhs);
}

binary_op_fncs_t binary_op_fncs[] = {
    [OP_Add] = { .int_fnc = int_Add, .uint_fnc = uint_Add, .double_fnc = double_Add, .bool_fnc = NULL },
    [OP_BinaryAnd] = { .int_fnc = int_BinaryAnd, .uint_fnc = uint_BinaryAnd, .double_fnc = NULL, .bool_fnc = NULL },
    [OP_BinaryOr] = { .int_fnc = int_BinaryOr, .uint_fnc = uint_BinaryOr, .double_fnc = NULL, .bool_fnc = NULL },
    [OP_BinaryXor] = { .int_fnc = int_BinaryXor, .uint_fnc = uint_BinaryXor, .double_fnc = NULL, .bool_fnc = NULL },
    [OP_Divide] = { .int_fnc = int_Divide, .uint_fnc = uint_Divide, .double_fnc = double_Divide, .bool_fnc = NULL },
    [OP_Equals] = { .int_fnc = int_Equals, .uint_fnc = uint_Equals, .double_fnc = double_Equals, .bool_fnc = bool_Equals },
    [OP_Greater] = { .int_fnc = int_Greater, .uint_fnc = uint_Greater, .double_fnc = double_Greater, .bool_fnc = NULL },
    [OP_GreaterEqual] = { .int_fnc = int_GreaterEqual, .uint_fnc = uint_GreaterEqual, .double_fnc = double_GreaterEqual, .bool_fnc = NULL },
    [OP_Less] = { .int_fnc = int_Less, .uint_fnc = uint_Less, .double_fnc = double_Less, .bool_fnc = NULL },
    [OP_LessEqual] = { .int_fnc = int_LessEqual, .uint_fnc = uint_LessEqual, .double_fnc = double_LessEqual, .bool_fnc = NULL },
    [OP_LogicalAnd] = { .int_fnc = NULL, .uint_fnc = NULL, .double_fnc = NULL, .bool_fnc = bool_LogicalAnd },
    [OP_LogicalOr] = { .int_fnc = NULL, .uint_fnc = NULL, .double_fnc = NULL, .bool_fnc = bool_LogicalOr },
    [OP_Modulo] = { .int_fnc = int_Modulo, .uint_fnc = uint_Modulo, .double_fnc = double_Modulo, .bool_fnc = NULL },
    [OP_Multiply] = { .int_fnc = int_Multiply, .uint_fnc = uint_Multiply, .double_fnc = double_Multiply, .bool_fnc = NULL },
    [OP_NotEqual] = { .int_fnc = int_NotEqual, .uint_fnc = uint_NotEqual, .double_fnc = double_NotEqual, .bool_fnc = bool_NotEqual },
    [OP_Subtract] = { .int_fnc = int_Subtract, .uint_fnc = uint_Subtract, .double_fnc = double_Subtract, .bool_fnc = NULL },
};

intptr_t evaluate_Int_BinOp(interp_stack_t *stack, type_t *lhs_type, operator_t op)
{
    assert(op < sizeof(binary_op_fncs) / sizeof(binary_op_fncs_t));
    if (lhs_type->int_type.is_signed) {
        int64_t lhs, rhs;
        switch (lhs_type->int_type.code) {
#undef S
#define S(W, T)                                \
    case IC_##W:                               \
        rhs = (int64_t) stack_pop_T(T, stack); \
        lhs = (int64_t) stack_pop_T(T, stack); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
        assert(binary_op_fncs[op].int_fnc != NULL);
        int64_t res = binary_op_fncs[op].int_fnc(lhs, rhs);
        if (res < lhs_type->int_type.min_value || (uint64_t) res > lhs_type->int_type.max_value) {
            fprintf(stderr, "Integer overflow\n");
            abort();
        }
        switch (lhs_type->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
    } else {
        uint64_t lhs, rhs;
        switch (lhs_type->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        rhs = (uint64_t) stack_pop_T(T, stack); \
        lhs = (uint64_t) stack_pop_T(T, stack); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
        assert(binary_op_fncs[op].uint_fnc != NULL);
        uint64_t res = binary_op_fncs[op].uint_fnc(lhs, rhs);
        if (res > lhs_type->int_type.max_value) {
            fprintf(stderr, "Integer overflow\n");
            abort();
        }
        switch (lhs_type->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
    }
}

intptr_t evaluate_Int_UnaryOp(interp_stack_t *stack, type_t *operand, operator_t op)
{
    assert(op < sizeof(unary_op_fncs) / sizeof(unary_op_fncs_t));
    if (operand->int_type.is_signed) {
        int64_t val;
        switch (operand->int_type.code) {
#undef S
#define S(W, T)                                \
    case IC_##W:                               \
        val = (int64_t) stack_pop_T(T, stack); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
        assert(binary_op_fncs[op].int_fnc != NULL);
        int64_t res = unary_op_fncs[op].int_fnc(val);
        switch (operand->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
    } else {
        uint64_t val;
        switch (operand->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        val = (uint64_t) stack_pop_T(T, stack); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
        assert(binary_op_fncs[op].uint_fnc != NULL);
        uint64_t res = unary_op_fncs[op].uint_fnc(val);
        switch (operand->int_type.code) {
#undef S
#define S(W, T)                                 \
    case IC_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
            INTTYPES(S)
        default:
            UNREACHABLE();
        }
    }
}

intptr_t evaluate_Float_BinOp(interp_stack_t *stack, type_t *lhs_type, operator_t op)
{
    assert(op < sizeof(binary_op_fncs) / sizeof(binary_op_fncs_t));
    double lhs, rhs;
    switch (lhs_type->float_width) {
#undef S
#define S(W, T)                               \
    case FW_##W:                              \
        rhs = (double) stack_pop_T(T, stack); \
        lhs = (double) stack_pop_T(T, stack); \
        break;
        FLOATTYPES(S)
    default:
        UNREACHABLE();
    }
    assert(binary_op_fncs[op].double_fnc != NULL);
    double res = binary_op_fncs[op].double_fnc(lhs, rhs);
    switch (lhs_type->float_width) {
#undef S
#define S(W, T)                                 \
    case FW_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
        FLOATTYPES(S)
    default:
        UNREACHABLE();
    }
}

intptr_t evaluate_Float_UnaryOp(interp_stack_t *stack, type_t *operand, operator_t op)
{
    assert(op < sizeof(unary_op_fncs) / sizeof(unary_op_fncs_t));
    double val;
    switch (operand->float_width) {
#undef S
#define S(W, T)                               \
    case FW_##W:                              \
        val = (double) stack_pop_T(T, stack); \
        break;
        FLOATTYPES(S)
    default:
        UNREACHABLE();
    }
    assert(binary_op_fncs[op].double_fnc != NULL);
    double res = unary_op_fncs[op].double_fnc(val);
    switch (operand->float_width) {
#undef S
#define S(W, T)                                 \
    case FW_##W:                                \
        return stack_push_T(T, stack, (T) res); \
        break;
        FLOATTYPES(S)
    default:
        UNREACHABLE();
    }
}

intptr_t concatenate_slices(interp_stack_t *stack, size_t elem_size, slice_t lhs, slice_t rhs)
{
    generic_da_t ret = { 0 };
    for (size_t ix = 0; ix < lhs.len; ++ix) {
        generic_da_append(&ret, elem_size, lhs.items + (ix * elem_size));
    }
    for (size_t ix = 0; ix < rhs.len; ++ix) {
        generic_da_append(&ret, elem_size, rhs.items + (ix * elem_size));
    }
    return stack_push_T(generic_da_t, stack, ret);
}

intptr_t evaluate_arithmatic_op(interp_stack_t *stack, nodeptr t1, nodeptr t2, operator_t op)
{
    type_t *lhs_type = get_type(t1);
    switch (lhs_type->kind) {
    case TYPK_IntType:
        assert(t1.value == t2.value);
        return evaluate_Int_BinOp(stack, lhs_type, op);
    case TYPK_FloatType:
        assert(t1.value == t2.value);
        return evaluate_Float_BinOp(stack, lhs_type, op);
    case TYPK_BoolType: {
        bool rhs = (bool) stack_pop_T(bool, stack);
        bool lhs = (bool) stack_pop_T(bool, stack);
        assert(binary_op_fncs[op].bool_fnc != NULL);
        bool res = binary_op_fncs[op].bool_fnc(lhs, rhs);
        return stack_push_T(bool, stack, res);
    }
    default:
        UNREACHABLE();
    }
}

intptr_t evaluate_Add(interp_stack_t *stack, nodeptr t1, nodeptr t2)
{
    type_t *lhs_type = get_type(t1);
    type_t *rhs_type = get_type(t2);
    switch (lhs_type->kind) {
    case TYPK_IntType:
    case TYPK_FloatType:
        return evaluate_arithmatic_op(stack, t1, t2, OP_Add);
    case TYPK_DynArrayType: {
        slice_t rhs = { 0 };
        size_t  elem_size = type_size_of(lhs_type->array_of);
        switch (rhs_type->kind) {
        case TYPK_DynArrayType: {
            assert(rhs_type->array_of.value == lhs_type->array_of.value);
            generic_da_t rhs_da = stack_pop_T(generic_da_t, stack);
            rhs = dynarr_as_slice(rhs_da);
        } break;
        case TYPK_SliceType: {
            assert(rhs_type->slice_of.value == lhs_type->array_of.value);
            rhs = stack_pop_T(slice_t, stack);
        } break;
        default:
            UNREACHABLE();
        }
        generic_da_t lhs_da = stack_pop_T(generic_da_t, stack);
        return concatenate_slices(stack, elem_size, dynarr_as_slice(lhs_da), rhs);
    }
    case TYPK_SliceType: {
        slice_t rhs = { 0 };
        size_t  elem_size = type_size_of(lhs_type->slice_of);
        switch (rhs_type->kind) {
        case TYPK_DynArrayType: {
            assert(rhs_type->array_of.value == lhs_type->array_of.value);
            generic_da_t rhs_da = stack_pop_T(generic_da_t, stack);
            rhs = dynarr_as_slice(rhs_da);
        } break;
        case TYPK_SliceType: {
            assert(rhs_type->slice_of.value == lhs_type->array_of.value);
            rhs = stack_pop_T(slice_t, stack);
        } break;
        default:
            UNREACHABLE();
        }
        return concatenate_slices(stack, elem_size, stack_pop_T(slice_t, stack), rhs);
    }
    default:
        UNREACHABLE();
    }
}

intptr_t stack_evaluate(interp_stack_t *stack, nodeptr lhs_type, operator_t op, nodeptr rhs_type)
{
    switch (op) {
    case OP_Add:
        return evaluate_Add(stack, lhs_type, rhs_type);
    default:
        return evaluate_arithmatic_op(stack, lhs_type, rhs_type, op);
    }
}

intptr_t stack_evaluate_unary(interp_stack_t *stack, nodeptr operand, operator_t op)
{
    type_t *op_type = get_type(operand);
    switch (op) {
    case OP_AddressOf:
        return evaluate_AddressOf(stack, operand);
    case OP_Length:
        return evaluate_Length(stack, operand);
    default: {
        switch (op_type->kind) {
        case TYPK_IntType:
            return evaluate_Int_UnaryOp(stack, op_type, op);
        case TYPK_FloatType:
            return evaluate_Float_UnaryOp(stack, op_type, op);
        case TYPK_BoolType: {
            bool rhs = (bool) stack_pop_T(bool, stack);
            bool lhs = (bool) stack_pop_T(bool, stack);
            assert(binary_op_fncs[op].bool_fnc != NULL);
            bool res = binary_op_fncs[op].bool_fnc(lhs, rhs);
            return stack_push_T(bool, stack, res);
        }
        default:
            UNREACHABLE();
        }
    }
    }
}
