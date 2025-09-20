/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "node.h"
#include "type.h"

#undef S
#define S(TK)                                  \
    static slice_t  TK##_to_string(type_t *t); \
    static intptr_t TK##_size_of(type_t *t);   \
    static intptr_t TK##_align_of(type_t *t);
TYPEKINDS(S)
#undef S

typedef struct _type_vtable {
    slice_t (*to_string)(type_t *);
    intptr_t (*size_of)(type_t *);
    intptr_t (*align_of)(type_t *);
} type_vtable_t;

static type_vtable_t vtables[] = {
#undef S
#define S(TK) [TYPK_##TK] = { .to_string = TK##_to_string, .size_of = TK##_size_of, .align_of = TK##_align_of },
    TYPEKINDS(S)
#undef S
};

nodeptr F32 = OPTVAL(size_t, IX_F32);
nodeptr F64 = OPTVAL(size_t, IX_F64);
nodeptr U8 = OPTVAL(size_t, IX_U8);
nodeptr I8 = OPTVAL(size_t, IX_I8);
nodeptr U16 = OPTVAL(size_t, IX_U16);
nodeptr I16 = OPTVAL(size_t, IX_I16);
nodeptr U32 = OPTVAL(size_t, IX_U32);
nodeptr I32 = OPTVAL(size_t, IX_I32);
nodeptr U64 = OPTVAL(size_t, IX_U64);
nodeptr I64 = OPTVAL(size_t, IX_I64);
nodeptr Boolean = OPTVAL(size_t, IX_Boolean);
nodeptr String = OPTVAL(size_t, IX_String);
nodeptr StringBuilder = OPTVAL(size_t, IX_StringBuilder);
nodeptr CString = OPTVAL(size_t, IX_CString);
nodeptr Character = OPTVAL(size_t, IX_Character);
nodeptr Null = OPTVAL(size_t, IX_Null);
nodeptr Void = OPTVAL(size_t, IX_Void);
nodeptr Pointer = OPTVAL(size_t, IX_Pointer);
nodeptr VoidFnc = OPTVAL(size_t, IX_VoidFnc);

int_type_t _U8 = {
    .code = IC_U8, .is_signed = false, .width_bits = 8, .max_value = 0xFF, .min_value = 0
};
int_type_t _U16 = {
    .code = IC_U16, .is_signed = false, .width_bits = 16, .max_value = 0xFFFF, .min_value = 0
};
int_type_t _U32 = {
    .code = IC_U32, .is_signed = false, .width_bits = 32, .max_value = 0xFFFFFFFF, .min_value = 0
};
int_type_t _U64 = {
    .code = IC_U64, .is_signed = false, .width_bits = 64, .max_value = 0xFFFFFFFFFFFFFFFF, .min_value = 0
};
int_type_t _I8 = {
    .code = IC_I8, .is_signed = true, .width_bits = 8, .max_value = 0x7FUL, .min_value = -0x80L
};
int_type_t _I16 = {
    .code = IC_I16, .is_signed = true, .width_bits = 16, .max_value = 0x7FFFFUL, .min_value = -0x8000L
};
int_type_t _I32 = {
    .code = IC_I32, .is_signed = true, .width_bits = 32, .max_value = 0x7FFFFFFFUL, .min_value = -0x80000000L
};
int_type_t _I64 = {
    .code = IC_I64, .is_signed = true, .width_bits = 64, .max_value = 0x7FFFFFFFFFFFFFFFUL, .min_value = (int64_t) 0x8000000000000000UL
};

typedef struct _type_name {
    slice_t name;
    nodeptr type;
} type_name_t;

#define make_type(k, ...)                                                          \
    (                                                                              \
        {                                                                          \
            type_t __t = { .kind = (k), .str = { 0 }, __VA_ARGS__ };               \
            dynarr_append(&type_registry, __t);                                    \
            nodeptr __ret = OPTVAL(size_t, type_registry.len - 1);                 \
            printf("Created type %zu: %d " SL "\n",                                \
                __ret.value, get_type(__ret)->kind, SLARG(type_kind_name(__ret))); \
            __ret;                                                                 \
        })

static DA(type_t) type_registry = { 0 };
static DA(type_name_t) type_by_name = { 0 };

/* ------------------------------------------------------------------------ */

intptr_t align_at(intptr_t alignment, intptr_t bytes)
{
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0); // Align must be power of 2
    return (bytes + (alignment - 1)) & ~(alignment - 1);
}

intptr_t words_needed(intptr_t word_size, intptr_t bytes)
{
    size_t ret = bytes / word_size;
    return (bytes % word_size != 0) ? ret + 1 : ret;
}

/* ------------------------------------------------------------------------ */

slice_t AliasType_to_string(type_t *t)
{
    nodeptr aliasof = t->alias_of;
    for (t = get_type(t->alias_of); t->kind == TYPK_AliasType; t = get_type(t->alias_of)) {
        aliasof = t->alias_of;
    }
    return sb_as_slice(sb_format("aliasof(" SL ")", type_to_string(aliasof)));
}

intptr_t AliasType_size_of(type_t *t)
{
    nodeptr aliasof = t->alias_of;
    for (t = get_type(t->alias_of); t->kind == TYPK_AliasType; t = get_type(t->alias_of)) {
        aliasof = t->alias_of;
    }
    return type_size_of(aliasof);
}

intptr_t AliasType_align_of(type_t *t)
{
    nodeptr aliasof = t->alias_of;
    for (t = get_type(t->alias_of); t->kind == TYPK_AliasType; t = get_type(t->alias_of)) {
        aliasof = t->alias_of;
    }
    return type_align_of(aliasof);
}

/* ------------------------------------------------------------------------ */

slice_t ArrayType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_printf(&ret, "[%zu]" SL, t->array_type.size, SLARG(type_to_string(t->array_type.array_of)));
    return sb_as_slice(ret);
}

intptr_t ArrayType_size_of(type_t *t)
{
    return align_at(type_align_of(t->array_type.array_of), type_size_of(t->array_type.array_of)) * t->array_type.size;
}

intptr_t ArrayType_align_of(type_t *t)
{
    return type_align_of(t->array_type.array_of);
}

/* ------------------------------------------------------------------------ */

slice_t BoolType_to_string(type_t *t)
{
    (void) t;
    return C("boolean");
}

intptr_t BoolType_size_of(type_t *t)
{
    (void) t;
    return 1;
}

intptr_t BoolType_align_of(type_t *t)
{
    (void) t;
    return 1;
}

/* ------------------------------------------------------------------------ */

slice_t DynArrayType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_printf(&ret, "[*]" SL, SLARG(type_to_string(t->array_of)));
    return sb_as_slice(ret);
}

intptr_t DynArrayType_size_of(type_t *t)
{
    (void) t;
    return sizeof(generic_da_t);
}

intptr_t DynArrayType_align_of(type_t *t)
{
    (void) t;
    return _Alignof(generic_da_t);
}

/* ------------------------------------------------------------------------ */

slice_t EnumType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_append(&ret, C("enum{"));
    for (size_t ix = 0; ix < t->enum_type.values.len; ++ix) {
        if (ix > 0) {
            sb_append_char(&ret, ',');
        }
        sb_append(&ret, t->enum_type.values.items[ix].label);
    }
    sb_append_char(&ret, '}');
    return sb_as_slice(ret);
}

intptr_t EnumType_size_of(type_t *t)
{
    intptr_t maxsize = 0;
    intptr_t a = EnumType_align_of(t);
    for (size_t ix = 0; ix < t->enum_type.values.len; ++ix) {
        enum_type_value_t *v = t->enum_type.values.items + ix;
        if (v->payload.ok) {
            maxsize = MAX(align_at(a, type_size_of(v->payload)), maxsize);
        }
    }
    return align_at(a, type_size_of(t->enum_type.underlying_type));
}

intptr_t EnumType_align_of(type_t *t)
{
    intptr_t ret = type_align_of(t->enum_type.underlying_type);
    for (size_t ix = 0; ix < t->enum_type.values.len; ++ix) {
        enum_type_value_t *value = t->enum_type.values.items + ix;
        ret = MAX((value->payload.ok) ? type_align_of(value->payload) : 0, ret);
    }
    return ret;
}

/* ------------------------------------------------------------------------ */

slice_t FloatType_to_string(type_t *t)
{
    return sb_as_slice(sb_format("f%d", (int) t->float_width));
}

intptr_t FloatType_size_of(type_t *t)
{
    return (intptr_t) t->float_width;
}

intptr_t FloatType_align_of(type_t *t)
{
    switch (t->float_width) {
    case FW_32:
        return _Alignof(float);
    case FW_64:
        return _Alignof(double);
    default:
        UNREACHABLE();
    }
}

/* ------------------------------------------------------------------------ */

slice_t IntType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_printf(&ret, "%c%d", (t->int_type.is_signed) ? 'i' : 'u', t->int_type.width_bits);
    return sb_as_slice(ret);
}

intptr_t IntType_size_of(type_t *t)
{
    return t->int_type.width_bits / 8;
}

intptr_t IntType_align_of(type_t *t)
{
    return t->int_type.width_bits / 8;
}

/* ------------------------------------------------------------------------ */

slice_t OptionalType_to_string(type_t *t)
{
    return sb_as_slice(sb_format("?" SL, SLARG(type_to_string(t->optional_of))));
}

intptr_t OptionalType_size_of(type_t *t)
{
    return align_at(type_align_of(t->optional_of), 1) + type_size_of(t->optional_of);
}

intptr_t OptionalType_align_of(type_t *t)
{
    return type_align_of(t->optional_of);
}

/* ------------------------------------------------------------------------ */

slice_t PointerType_to_string(type_t *t)
{
    (void) t;
    return C("pointer");
}

intptr_t PointerType_size_of(type_t *t)
{
    (void) t;
    return sizeof(void *);
}

intptr_t PointerType_align_of(type_t *t)
{
    (void) t;
    return _Alignof(void *);
}

/* ------------------------------------------------------------------------ */

slice_t RangeType_to_string(type_t *t)
{
    return sb_as_slice(sb_format(SL "..", SLARG(type_to_string(t->range_of))));
}

intptr_t RangeType_size_of(type_t *t)
{
    return 2 * align_at(type_align_of(t->range_of), type_size_of(t->range_of)) + type_size_of(t->range_of);
}

intptr_t RangeType_align_of(type_t *t)
{
    return type_align_of(t->range_of);
}

/* ------------------------------------------------------------------------ */

slice_t ReferenceType_to_string(type_t *t)
{
    return sb_as_slice(sb_format("&" SL, SLARG(type_to_string(t->referencing))));
}

intptr_t ReferenceType_size_of(type_t *t)
{
    (void) t;
    return sizeof(void *);
}

intptr_t ReferenceType_align_of(type_t *t)
{
    (void) t;
    return _Alignof(void *);
}

/* ------------------------------------------------------------------------ */

slice_t ResultType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_printf(&ret, SL "/" SL,
        SLARG(type_to_string(t->result_type.success)),
        SLARG(type_to_string(t->result_type.failure)));
    return sb_as_slice(ret);
}

intptr_t ResultType_size_of(type_t *t)
{
    return align_at(
               type_align_of(t->result_type.failure),
               type_size_of(t->result_type.success))
        + type_size_of(t->result_type.failure);
}

intptr_t ResultType_align_of(type_t *t)
{
    return MAX(
        type_align_of(t->result_type.success),
        type_align_of(t->result_type.failure));
}

/* ------------------------------------------------------------------------ */

slice_t Signature_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_append(&ret, C("fn("));
    for (size_t ix = 0; ix < t->signature_type.parameters.len; ++ix) {
        if (ix > 0) {
            sb_append_char(&ret, ',');
        }
        sb_append(&ret, type_to_string(t->signature_type.parameters.items[ix]));
    }
    sb_printf(&ret, ")" SL, SLARG(type_to_string(t->signature_type.result)));
    return sb_as_slice(ret);
}

intptr_t Signature_size_of(type_t *t)
{
    (void) t;
    return 0;
}

intptr_t Signature_align_of(type_t *t)
{
    (void) t;
    return 0;
}

/* ------------------------------------------------------------------------ */

slice_t SliceType_to_string(type_t *t)
{
    return sb_as_slice(sb_format("[]" SL, SLARG(type_to_string(t->slice_of))));
}

intptr_t SliceType_size_of(type_t *t)
{
    (void) t;
    return sizeof(slice_t);
}

intptr_t SliceType_align_of(type_t *t)
{
    (void) t;
    return _Alignof(slice_t);
}

/* ------------------------------------------------------------------------ */

slice_t StructType_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_append(&ret, C("struct{"));
    for (size_t ix = 0; ix < t->struct_fields.len; ++ix) {
        if (ix > 0) {
            sb_append_char(&ret, ',');
        }
        sb_printf(&ret, SL ":" SL,
            SLARG(t->struct_fields.items[ix].name),
            SLARG(type_to_string(t->struct_fields.items[ix].type)));
    }
    sb_append_char(&ret, '}');
    return sb_as_slice(ret);
}

intptr_t StructType_size_of(type_t *t)
{
    intptr_t size = 0;
    for (size_t ix = 0; ix < t->struct_fields.len; ++ix) {
        struct_field_t *fld = t->struct_fields.items + ix;
        size = align_at(type_align_of(fld->type), size) + type_size_of(fld->type);
    }
    return size;
}

intptr_t StructType_align_of(type_t *t)
{
    intptr_t ret;
    for (size_t ix = 0; ix < t->struct_fields.len; ++ix) {
        ret = MAX(type_align_of(t->struct_fields.items[ix].type), ret);
    }
    return ret;
}

/* ------------------------------------------------------------------------ */

slice_t TypeList_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_append(&ret, C("("));
    for (size_t ix = 0; ix < t->type_list_types.len; ++ix) {
        if (ix > 0) {
            sb_append_char(&ret, ',');
        }
        sb_append(&ret, type_to_string(t->type_list_types.items[ix]));
    }
    sb_append_char(&ret, ')');
    return sb_as_slice(ret);
}

intptr_t TypeList_size_of(type_t *t)
{
    intptr_t ret = 0;
    for (size_t ix = 0; ix < t->type_list_types.len; ++ix) {
        ret = align_at(type_align_of(t->type_list_types.items[ix]), ret) + type_size_of(t->type_list_types.items[ix]);
    }
    return ret;
}

intptr_t TypeList_align_of(type_t *t)
{
    intptr_t ret = 0;
    for (size_t ix = 0; ix < t->type_list_types.len; ++ix) {
        ret = MAX(type_align_of(t->type_list_types.items[ix]), ret);
    }
    return ret;
}

/* ------------------------------------------------------------------------ */

slice_t VoidType_to_string(type_t *t)
{
    (void) t;
    return C("void");
}

intptr_t VoidType_size_of(type_t *t)
{
    (void) t;
    return 0;
}

intptr_t VoidType_align_of(type_t *t)
{
    (void) t;
    return 0;
}

/* ------------------------------------------------------------------------ */

slice_t ZeroTerminatedArray_to_string(type_t *t)
{
    sb_t ret = { 0 };
    sb_printf(&ret, "[0]" SL, SLARG(type_to_string(t->array_of)));
    return sb_as_slice(ret);
}

intptr_t ZeroTerminatedArray_size_of(type_t *t)
{
    (void) t;
    return sizeof(void *);
}

intptr_t ZeroTerminatedArray_align_of(type_t *t)
{
    (void) t;
    return _Alignof(void *);
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

slice_t type_kind_name(nodeptr p)
{
    if (!p.ok) {
        return C("type_kind_name of NULL type pointer");
    }
    if (p.value >= type_registry.len) {
        static char msgbuffer[1024];
        snprintf(msgbuffer, 1023, "type_kind_name of p = %zu, but only %zu types registered", p.value, type_registry.len);
        return C(msgbuffer);
    }
    switch (get_type(p)->kind) {
#undef S
#define S(K)       \
    case TYPK_##K: \
        return C(#K);
        TYPEKINDS(S)
    default:
        UNREACHABLE();
    }
}

slice_t type_to_string(nodeptr p)
{
    type_t *t = get_type(p);
    if (t->str.len == 0) {
        t->str = vtables[t->kind].to_string(t);
        assert(t->str.len > 0);
    }
    return t->str;
}

intptr_t type_align_of(nodeptr p)
{
    type_t *t = get_type(p);
    return vtables[t->kind].align_of(t);
}

intptr_t type_size_of(nodeptr p)
{
    type_t *t = get_type(p);
    return vtables[t->kind].size_of(t);
}

nodeptr referencing(nodeptr type)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_ReferenceType
            && type_registry.items[ix].referencing.value == type.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_ReferenceType, .referencing = type);
}

nodeptr alias_of(nodeptr aliased)
{
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_AliasType
            && type_registry.items[ix].alias_of.value == aliased.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_AliasType, .alias_of = aliased);
}

nodeptr slice_of(nodeptr type)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_SliceType
            && type_registry.items[ix].slice_of.value == type.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_SliceType, .slice_of = type);
}

nodeptr array_of(nodeptr type, size_t size)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_ArrayType
            && type_registry.items[ix].array_type.array_of.value == type.value
            && type_registry.items[ix].array_type.size == size) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_ArrayType, .array_type = { .array_of = type, .size = size });
}

nodeptr dyn_array_of(nodeptr type)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_DynArrayType
            && type_registry.items[ix].array_of.value == type.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_DynArrayType, .array_of = type);
}

nodeptr zero_terminated_array_of(nodeptr type)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_ZeroTerminatedArray
            && type_registry.items[ix].array_of.value == type.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_ZeroTerminatedArray, .array_of = type);
}

nodeptr optional_of(nodeptr type)
{
    assert(type.ok && (type.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_OptionalType
            && type_registry.items[ix].optional_of.value == type.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_OptionalType, .optional_of = type);
}

nodeptr result_of(nodeptr success, nodeptr failure)
{
    assert(success.ok && (success.value < type_registry.len));
    assert(failure.ok && (failure.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_ResultType
            && type_registry.items[ix].result_type.success.value == success.value
            && type_registry.items[ix].result_type.failure.value == failure.value) {
            return OPTVAL(size_t, ix);
        }
    }
    return make_type(TYPK_ResultType, .result_type = { .success = success, .failure = failure });
}

nodeptr signature(nodeptrs parameters, nodeptr result)
{
    for (size_t ix = 0; ix < parameters.len; ++ix) {
        assert(parameters.items[ix].ok && (parameters.items[ix].value < type_registry.len));
    }
    assert(result.ok && (result.value < type_registry.len));
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        type_t *t = type_registry.items + ix;
        if (t->kind == TYPK_Signature && t->signature_type.result.value == result.value && t->signature_type.parameters.len == parameters.len) {
            bool all_matched = true;
            for (size_t iix = 0; iix < parameters.len; ++iix) {
                if (parameters.items[iix].value != t->signature_type.parameters.items[iix].value) {
                    all_matched = false;
                    break;
                }
            }
            if (all_matched) {
                dynarr_free(&parameters);
                return OPTVAL(size_t, ix);
            }
        }
    }
    return make_type(
        TYPK_Signature,
        .signature_type = { .parameters = parameters, .result = result });
}

nodeptr typelist_of(nodeptrs types)
{
    for (size_t ix = 0; ix < types.len; ++ix) {
        assert(types.items[ix].ok && (types.items[ix].value < type_registry.len));
    }
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        if (type_registry.items[ix].kind == TYPK_TypeList
            && type_registry.items[ix].type_list_types.len == types.len) {
            bool all_matched = true;
            for (size_t ix = 0; ix < types.len; ++ix) {
                if (types.items[ix].value
                    != type_registry.items[ix].type_list_types.items[ix].value) {
                    all_matched = false;
                    break;
                }
            }
            if (all_matched) {
                dynarr_free(&types);
                return OPTVAL(size_t, ix);
            }
        }
    }
    return make_type(TYPK_TypeList, .type_list_types = types);
}

nodeptr struct_of(struct_fields_t fields)
{
    for (size_t ix = 0; ix < type_registry.len; ++ix) {
        type_t *t = type_registry.items + ix;
        if (t->kind == TYPK_StructType && t->struct_fields.len == fields.len) {
            bool all_matched = true;
            for (size_t fix = 0; fix < fields.len; ++fix) {
                if (fields.items[fix].type.value != t->struct_fields.items[fix].type.value
                    || !slice_eq(fields.items[fix].name, t->struct_fields.items[fix].name)) {
                    all_matched = false;
                    break;
                }
            }
            if (all_matched) {
                dynarr_free(&fields);
                return OPTVAL(size_t, ix);
            }
        }
    }
    return make_type(TYPK_StructType, .struct_fields = fields);
}

void type_registry_init()
{
#define MAKE_INTERNAL(T, N, K, ...)                                                   \
    {                                                                                 \
        type_t __t = { .kind = (K), .str = { 0 }, __VA_ARGS__ };                      \
        dynarr_append(&type_registry, __t);                                           \
        nodeptr __ix = OPTVAL(size_t, type_registry.len - 1);                         \
        assert((T).ok &&__ix.value == (T).value);                                     \
        dynarr_append(&type_by_name, ((type_name_t) { .name = C(#N), .type = (T) })); \
    }
#undef S
#define S(W) MAKE_INTERNAL(F##W, f##W, TYPK_FloatType, .float_width = FW_##W);
    FLOATWIDTHS(S)
#undef S
#define S(W)                                                    \
    MAKE_INTERNAL(U##W, u##W, TYPK_IntType, .int_type = _U##W); \
    MAKE_INTERNAL(I##W, i##W, TYPK_IntType, .int_type = _I##W);
    INTWIDTHS(S)
#undef S
    MAKE_INTERNAL(Boolean, boolean, TYPK_BoolType, .array_of = { 0 });
    MAKE_INTERNAL(String, string, TYPK_SliceType, .slice_of = U8);
    MAKE_INTERNAL(StringBuilder, string_builder, TYPK_DynArrayType, .array_of = U8);
    MAKE_INTERNAL(CString, cstring, TYPK_ZeroTerminatedArray, .array_of = U8);
    MAKE_INTERNAL(Character, char, TYPK_AliasType, .alias_of = U8);
    MAKE_INTERNAL(Pointer, pointer, TYPK_PointerType, .array_of = { 0 });
    MAKE_INTERNAL(Null, null, TYPK_VoidType, .array_of = { 0 });
    MAKE_INTERNAL(Void, void, TYPK_AliasType, .alias_of = Null);
    MAKE_INTERNAL(VoidFnc, void_fnc, TYPK_Signature, .signature_type = {
                                                         .parameters = { 0 },
                                                         .result = Void,
                                                     });
}

#define GETTYPE(p) (type_registry.items + (p).value)

type_t *get_type(nodeptr p)
{
    if (type_registry.len == 0) {
        type_registry_init();
    }
    assert(p.ok && p.value < type_registry.len);
    type_t *t = GETTYPE(p);
    while (t->kind == TYPK_AliasType) {
        t = GETTYPE(t->alias_of);
    }
    return t;
}

nodeptr find_type(slice_t name)
{
    printf("find_type(" SL ")\n", SLARG(name));
    while (true) {
        for (size_t ix = 0; ix < type_by_name.len; ++ix) {
            if (slice_eq(name, type_by_name.items[ix].name)) {
                nodeptr type = type_by_name.items[ix].type;
                assert(type.ok && type.value < type_registry.len);
                type_t *t = GETTYPE(type);
                while (t->kind == TYPK_AliasType) {
                    type = t->alias_of;
                    t = GETTYPE(type);
                }
                printf("find_type: found %zu\n", type.value);
                return type;
            }
        }
    }
    return nullptr;
}
