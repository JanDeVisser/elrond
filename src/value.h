/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __VALUE_H__
#define __VALUE_H__

#include "da.h"
#include "slice.h"
#include "type.h"
#include <stdint.h>

#define VALUE_INTFIELDS(S)              \
    S(8, true, IC_I8, i8, int8_t)       \
    S(8, false, IC_U8, u8, uint8_t)     \
    S(16, true, IC_I16, i16, int16_t)   \
    S(16, false, IC_U16, u16, uint16_t) \
    S(32, true, IC_I32, i32, int32_t)   \
    S(32, false, IC_U32, u32, uint32_t) \
    S(64, true, IC_I64, i64, int64_t)   \
    S(64, false, IC_U64, u64, uint64_t)

typedef DA(struct _value) values_t;

typedef struct _value {
    nodeptr type;
    union {
#undef S
#define S(W)          \
    int##W##_t  i##W; \
    uint##W##_t u##W;
        INTWIDTHS(S)
#undef S
        bool         boolean;
        float        f32;
        double       f64;
        slice_t      slice;
        generic_da_t da;
        array_t      array;
        void        *ptr;
        values_t     values;
    };
} value_t;

OPTDEF(value_t);

value_t     make_value_void();
value_t     make_value_from_string(slice_t str);
opt_value_t make_value_from_signed(nodeptr type, int64_t v);
opt_value_t make_value_from_unsigned(nodeptr type, uint64_t v);
opt_value_t make_value_from_double(nodeptr type, double d);
value_t     make_value_from_buffer(nodeptr type, void *buf);
void        value_print(FILE *f, value_t value);
opt_value_t value_coerce(value_t value, nodeptr type);
opt_value_t evaluate(value_t v1, operator_t op, value_t v2);

#endif /* __VALUE_H__ */
