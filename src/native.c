/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <dlfcn.h>

#define RESOLVE_IMPLEMENTATION
#include "native.h"
#include "resolve.h"
#include "type.h"

typedef struct _trampoline {
    void_t   fnc;
    uint64_t x[8];
    double   d[8];
    uint64_t int_return_value;
    double   double_return_value;
} trampoline_t;

int trampoline(trampoline_t *tramp);

#define as_T(T, ptr, offset)                                    \
    (                                                           \
        {                                                       \
            T ret;                                              \
            memcpy(&ret, (char *) (ptr) + (offset), sizeof(T)); \
            (ret);                                              \
        })

#define set_T(T, ptr, val)                         \
    do {                                           \
        T __val = (val);                           \
        memcpy((char *) (ptr), &__val, sizeof(T)); \
    } while (0)

bool native_call(slice_t name, void *params, nodeptrs types, void *return_value, nodeptr return_type)
{
    if (types.len > 8) {
        fatal("Can't do native calls with more than 8 parameters");
    }
    trampoline_t      t;
    function_result_t res = resolve_function(name);
    if (!res.ok || res.success == NULL) {
        fatal("Function `" SL "` not found", SLARG(name));
    } else {
        t.fnc = res.success;
    }

    // Stage A - Initialization
    // This stage is performed exactly once, before processing of the arguments
    // commences.

    // A.1 The Next General-purpose Register Number (NGRN) is set to zero.
    size_t ngrn = 0;
    // A.2 The Next SIMD and Floating-point Register Number (NSRN) is set to
    // zero.
    size_t nsrn = 0;
    // A.3 The Next Scalable Predicate Register Number (NPRN) is set to zero.
    size_t nprn = 0;
    // A.4 The next stacked argument address (NSAA) is set to the current
    // stack-pointer value (SP).
    size_t nsaa = 0;

    (void) nsaa;
    (void) nprn;

    intptr_t offset = 0;
    trace("native_call(" SL ")", SLARG(name));
    for (size_t ix = 0; ix < types.len; ++ix) {
        nodeptr type = types.items[ix];

        trace("native_call param [%zu]: %zu `" SL "`", ix, type.value, SLARG(type_to_string(type)));
        // Stage B – Pre-padding and extension of arguments
        // For each argument in the list the first matching rule from the
        // following list is applied. If no rule matches the argument is used
        // unmodified.

        // B.1 If the argument type is a Pure Scalable Type, no change is made
        // at this stage.

        // B.2 If the argument type is a Composite Type whose size cannot be
        // statically determined by both the caller and the callee, the
        // argument is copied to memory and the argument is replaced by a
        // pointer to the copy. (There are no such types in C/C++ but they
        // exist in other languages or in language extensions).

        // B.3 If the argument type is an HFA or an HVA, then the argument is
        // used unmodified.
        // TODO

        // B.4 If the argument type is a Composite Type that is larger than 16
        // bytes, then the argument is copied to memory allocated by the caller
        // and the argument is replaced by a pointer to the copy.
        // TODO

        // B.5 If the argument type is a Composite Type then the size of the
        // argument is rounded up to the nearest multiple of 8 bytes.

        // B.6 If the argument is an alignment adjusted type its value is passed
        // as a copy of the actual value. The copy will have an alignment
        // defined as follows:
        //     • For a Fundamental Data Type, the alignment is the natural
        //       alignment of that type, after any promotions.
        //     • For a Composite Type, the alignment of the copy will have
        //       8-byte alignment if its natural alignment is ≤ 8 and 16-byte
        //       alignment if its natural alignment is ≥ 16.
        // The alignment of the copy is used for applying marshaling rules.

        // Stage C – Assignment of arguments to registers and stack
        // For each argument in the list the following rules are applied in
        // turn until the argument has been allocated. When an argument is
        // assigned to a register any unused bits in the register have
        // unspecified value. When an argument is assigned to a stack slot any
        // unused padding bytes have unspecified value.

        // C.1 If the argument is a Half-, Single-, Double- or Quad- precision
        // Floating-point or short vector type and the NSRN is less than 8, then
        // the argument is allocated to the least significant bits of register
        // v[NSRN]. The NSRN is incremented by one. The argument has now been
        // allocated.
        type_t *typ = get_type(type);
        switch (typ->kind) {
        case TYPK_FloatType: {
            if (nsrn < 8) {
                if (typ->float_width == FW_32) {
                    t.d[nsrn] = as_T(float, params, offset);
                } else {
                    t.d[nsrn] = as_T(double, params, offset);
                }
                ++nsrn;
            }
        } break;

            // C.2 If the argument is an HFA or an HVA and there are sufficient
            // unallocated SIMD and Floating-point registers (NSRN + number of
            // members ≤ 8), then the argument is allocated to SIMD and
            // Floating-point registers (with one register per member of the HFA or
            // HVA). The NSRN is incremented by the number of registers used. The
            // argument has now been allocated.
            // TODO

            // C.3 If the argument is an HFA or an HVA then the NSRN is set to 8 and
            // the size of the argument is rounded up to the nearest multiple of 8
            // bytes.
            // TODO

            // C.4 If the argument is an HFA, an HVA, a Quad-precision
            // Floating-point or short vector type then the NSAA is rounded up to
            // the next multiple of 8 if its natural alignment is ≤ 8 or the next
            // multiple of 16 if its natural alignment is ≥ 16.
            // TODO

            // C.5 If the argument is a Half- or Single- precision Floating Point
            // type, then the size of the argument is set to 8 bytes. The effect is
            // as if the argument had been copied to the least significant bits of a
            // 64-bit register and the remaining bits filled with unspecified
            // values.
            // Not supported

            // C.6 If the argument is an HFA, an HVA, a Half-, Single-, Double- or
            // Quad- precision Floating-point or short vector type, then the
            // argument is copied to memory at the adjusted NSAA. The NSAA is
            // incremented by the size of the argument. The argument has now been
            // allocated.
            // TODO

            // C.7 If the argument is a Pure Scalable Type that consists of NV
            // Scalable Vector Types and NP Scalable Predicate Types, if the
            // argument is named, if NSRN+NV ≤ 8, and if NPRN+NP ≤ 4, then the
            // Scalable Vector Types are allocated in order to
            // z[NSRN]...z[NSRN+NV-1] inclusive and the Scalable Predicate Types are
            // allocated in order to p[NPRN]...p[NPRN+NP-1] inclusive. The NSRN is
            // incremented by NV and the NPRN is incremented by NP. The argument has
            // now been allocated.
            // TODO

            // C.8 If the argument is a Pure Scalable Type that has not been
            // allocated by the rules above, then the argument is copied to memory
            // allocated by the caller and the argument is replaced by a pointer to
            // the copy (as for B.4 above). The argument is then allocated according
            // to the rules below.
            // TODO

            // C.9 If the argument is an Integral or Pointer Type, the size of the
            // argument is less than or equal to 8 bytes and the NGRN is less than
            // 8, the argument is copied to the least significant bits in x[NGRN].
            // The NGRN is incremented by one. The argument has now been allocated.
        case TYPK_IntType:

            if (ngrn < 8) {
#undef S
#define S(EType, CType)            \
    if (type.value == EType.value) \
        t.x[ngrn] = as_T(CType, params, offset);
                INTTYPES(S)
#undef S
                ++ngrn;
            }
            break;

        case TYPK_BoolType:
            if (ngrn < 8) {
                t.x[ngrn] = as_T(bool, params, offset);
                ++ngrn;
            }
            break;

        case TYPK_PointerType:
        case TYPK_ReferenceType:
        case TYPK_ZeroTerminatedArray:
            if (ngrn < 8) {
                t.x[ngrn] = (intptr_t) (as_T(void *, params, offset));
                ++ngrn;
            }
            break;

        case TYPK_SliceType:
            if (ngrn < 7) {
                slice_t s = as_T(slice_t, params, offset);
                t.x[ngrn++] = (intptr_t) (s.items);
                t.x[ngrn++] = s.len;
            }
            break;

        case TYPK_DynArrayType:
            if (ngrn < 6) {
                generic_da_t da = as_T(generic_da_t, params, offset);
                t.x[ngrn++] = (intptr_t) (da.items);
                t.x[ngrn++] = da.len;
                t.x[ngrn++] = da.capacity;
            }
            break;

        case TYPK_ArrayType:
            if (ngrn < 7) {
                array_t a = as_T(array_t, params, offset);
                t.x[ngrn++] = (intptr_t) (a.items);
                t.x[ngrn++] = a.size;
            }
            break;

        default:
            NYI("More value types");

            // C.10 If the argument has an alignment of 16 then the NGRN is rounded
            // up to the next even number.

            // C.11 If the argument is an Integral Type, the size of the argument
            // is equal to 16 and the NGRN is less than 7, the argument is copied to
            // x[NGRN] and x[NGRN+1]. x[NGRN] shall contain the lower addressed
            // double-word of the memory representation of the argument. The NGRN
            // is incremented by two. The argument has now been allocated.

            // C.12 If the argument is a Composite Type and the size in double-words
            // of the argument is not more than 8 minus NGRN, then the argument is
            // copied into consecutive general-purpose registers, starting at
            // x[NGRN]. The argument is passed as though it had been loaded into the
            // registers from a double-word-aligned address with an appropriate
            // sequence of LDR instructions loading consecutive registers from
            // memory (the contents of any unused parts of the registers are
            // unspecified by this standard). The NGRN is incremented by the number
            // of registers used. The argument has now been allocated.

            // if (type_kind(et) == TK_AGGREGATE) {
            //     size_t size_in_double_words = alignat(typeid_sizeof(et->type_id), 8) / 8;
            //     if (size_in_double_words <= (8 - ngrn)) {
            //         uint64_t buffer[size_in_double_words];
            //         size_t   sz = datum_binary_image(v, buffer);
            //         assert(sz <= size_in_double_words * 8);
            //         for (size_t ix2 = 0; ix2 < size_in_double_words; ++ix2) {
            //             t.x[ngrn] = buffer[ix2];
            //             ++ngrn;
            //         }
            //         continue;
            //     }
            // }

            // C.13 The NGRN is set to 8.

            // C.14 The NSAA is rounded up to the larger of 8 or the Natural
            // Alignment of the argument’s type.

            // C.15 If the argument is a composite type then the argument is copied
            // to memory at the adjusted NSAA. The NSAA is incremented by the size
            // of the argument. The argument has now been allocated.

            // C.16 If the size of the argument is less than 8 bytes then the size
            // of the argument is set to 8 bytes. The effect is as if the argument
            // was copied to the least significant bits of a 64-bit register and the
            // remaining bits filled with unspecified values.

            // C.17 The argument is copied to memory at the adjusted NSAA. The NSAA
            // is incremented by the size of the argument. The argument has now been
            // allocated.
        }
        offset += align_at(8, type_size_of(type));
    }

    trace("Trampoline:");
    trace("  Function: 0x%lx", (intptr_t) (t.fnc));
    trace("  Integer Registers:");
    for (size_t ix = 0; ix < 8; ++ix) {
        trace("    %zu: 0x%zx", ix, (size_t) t.x[ix]);
    }

    int trampoline_result = trampoline(&t);
    if (trampoline_result != 0) {
        fatal("Error executing `" SL "`. Trampoline returned %d", SLARG(name), trampoline_result);
    }
    trace("  Integer result: %zu", (size_t) t.int_return_value);

    type_t *ret = get_type(return_type);
    switch (ret->kind) {
    case TYPK_IntType: {
#undef S
#define S(EType, CType)                                         \
    if (return_type.value == EType.value) {                     \
        set_T(CType, return_value, (CType) t.int_return_value); \
        return true;                                            \
    }
        INTTYPES(S)
#undef S
        UNREACHABLE();
    } break;
    case TYPK_FloatType: {
        if (return_type.value == F32.value) {
            set_T(float, return_value, (float) t.double_return_value);
            return true;
        }
        if (return_type.value == F64.value) {
            set_T(double, return_value, (double) t.double_return_value);
            return true;
        }
        UNREACHABLE();
    } break;
    case TYPK_BoolType:
        set_T(bool, return_value, (bool) t.int_return_value);
        return true;
    case TYPK_PointerType:
    case TYPK_ReferenceType:
        set_T(void *, return_value, (void *) t.int_return_value);
        return true;
    case TYPK_VoidType:
        return true;
    default:
        UNREACHABLE();
    }
}
