/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __TYPE_H__
#define __TYPE_H__

#include <stdint.h>

#include "operators.h"
#include "slice.h"

#define TYPEKINDS(S) \
    S(AliasType)     \
    S(ArrayType)     \
    S(BoolType)      \
    S(DynArrayType)  \
    S(EnumType)      \
    S(FloatType)     \
    S(IntType)       \
    S(OptionalType)  \
    S(PointerType)   \
    S(RangeType)     \
    S(ReferenceType) \
    S(ResultType)    \
    S(Signature)     \
    S(SliceType)     \
    S(StructType)    \
    S(TypeList)      \
    S(VoidType)      \
    S(ZeroTerminatedArray)

typedef enum _type_kind {
#undef S
#define S(TK) TYPK_##TK,
    TYPEKINDS(S)
#undef S
} type_kind_t;

typedef struct _array_type {
    nodeptr array_of;
    size_t  size;
} array_type_t;

typedef struct _enum_type_value {
    slice_t  label;
    intptr_t value;
    nodeptr  payload;
} enum_type_value_t;

typedef struct _enum_type {
    DA(enum_type_value_t)
    values;
    nodeptr underlying_type;
} enum_type_t;

#define FLOATWIDTHS(S) \
    S(32)              \
    S(64)

#define FLOATTYPES(S) \
    S(32, float)      \
    S(64, double)

typedef enum _float_width {
    FW_32 = sizeof(float),
    FW_64 = sizeof(double),
} float_width_t;

#define INTWIDTHS(S) \
    S(8)             \
    S(16)            \
    S(32)            \
    S(64)

#define INTTYPES(S)  \
    S(I8, int8_t)    \
    S(U8, uint8_t)   \
    S(I16, int16_t)  \
    S(U16, uint16_t) \
    S(I32, int32_t)  \
    S(U32, uint32_t) \
    S(I64, int64_t)  \
    S(U64, uint64_t)

typedef enum _int_code {
#undef S
#define S(W)     \
    IC_U##W = W, \
    IC_I##W = W + 1,
    INTWIDTHS(S)
#undef S
} int_code_t;

typedef struct _int_type {
    int_code_t code;
    bool       is_signed;
    int        width_bits;
    uint64_t   max_value;
    int64_t    min_value;
} int_type_t;

typedef struct _signature_type {
    nodeptrs parameters;
    nodeptr  result;
    bool     noreturn;
    bool     nodiscard;
} signature_type_t;

typedef struct _struct_field {
    slice_t name;
    nodeptr type;
} struct_field_t;

typedef DA(struct_field_t) struct_fields_t;

typedef struct _result_type {
    nodeptr success;
    nodeptr failure;
} result_type_t;

typedef struct _type {
    type_kind_t kind;
    slice_t     str;
    union {
        nodeptr          alias_of;
        nodeptr          array_of;
        array_type_t     array_type;
        enum_type_t      enum_type;
        float_width_t    float_width;
        int_type_t       int_type;
        nodeptr          optional_of;
        nodeptr          range_of;
        nodeptr          referencing;
        result_type_t    result_type;
        signature_type_t signature_type;
        nodeptr          slice_of;
        struct_fields_t  struct_fields;
        nodeptrs         type_list_types;
    };
} type_t;

OPTDEF(type_t);

#undef S
#define S(W) extern nodeptr F##W;
FLOATWIDTHS(S)
#undef S
#define S(W)             \
    extern nodeptr I##W; \
    extern nodeptr U##W;
INTWIDTHS(S)
#undef S

#define IX_F32 0
#define IX_F64 1
#define IX_U8 2
#define IX_I8 3
#define IX_U16 4
#define IX_I16 5
#define IX_U32 6
#define IX_I32 7
#define IX_U64 8
#define IX_I64 9
#define IX_Boolean 10
#define IX_String 11
#define IX_StringBuilder 12
#define IX_CString 13
#define IX_Character 14
#define IX_Pointer 15
#define IX_Null 16
#define IX_Void 17
#define IX_VoidFnc 18

extern nodeptr Boolean;
extern nodeptr String;
extern nodeptr StringBuilder;
extern nodeptr CString;
extern nodeptr Character;
extern nodeptr Null;
extern nodeptr Void;
extern nodeptr Pointer;
extern nodeptr VoidFnc;

slice_t  type_kind_name(nodeptr p);
slice_t  type_to_string(nodeptr p);
intptr_t type_align_of(nodeptr p);
intptr_t type_size_of(nodeptr p);
nodeptr  alias_of(nodeptr aliased);
nodeptr  referencing(nodeptr type);
nodeptr  slice_of(nodeptr type);
nodeptr  array_of(nodeptr type, size_t size);
nodeptr  dyn_array_of(nodeptr type);
nodeptr  zero_terminated_array_of(nodeptr type);
nodeptr  optional_of(nodeptr type);
nodeptr  result_of(nodeptr success, nodeptr failure);
nodeptr  signature(nodeptrs parameters, nodeptr result);
nodeptr  typelist_of(nodeptrs types);
nodeptr  struct_of(struct_fields_t fields);
type_t  *get_type_file_line(nodeptr p, char const *file, int line);
nodeptr  find_type(slice_t name);
void     type_registry_init();

#define get_type(type) (get_type_file_line(type, __FILE__, __LINE__))
#define type_kind(type) (get_type_file_line((type), __FILE__, __LINE__)->kind)
#define type_is_int(type) (type_kind((type)) == TYPK_IntType)
#define type_is_number(type)                                \
    (                                                       \
        {                                                   \
            type_kind_t __k = type_kind((type));            \
            (__k == TYPK_IntType || __k == TYPK_FloatType); \
        })

#define type_value_type(type)                              \
    (                                                      \
        {                                                  \
            nodeptr __v = (type);                          \
            assert(__v.ok);                                \
            while (type_kind(__v) == TYPK_ReferenceType) { \
                __v = get_type(__v)->referencing;          \
            }                                              \
            __v;                                           \
        });

#endif /* __TYPE_H__ */
