/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef SLICE_TEST
#define SLICE_IMPLEMENTATION
#endif

#ifndef __SLICE_H__
#define __SLICE_H__

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a < b) ? (a) : (b))
#define MAX(a, b) ((a > b) ? (a) : (b))
#define ALIGNAT(bytes, align) ((bytes + (align - 1)) & ~(align - 1))

#define _fatal(prefix, msg, ...)                        \
    do {                                                \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fputs(prefix, stderr);                          \
        fprintf(stderr, msg, ##__VA_ARGS__);            \
        fputc('\n', stderr);                            \
        abort();                                        \
    } while (0)

#define UNREACHABLE_MSG(msg, ...) _fatal("Unreachable", msg, ##__VA_ARGS__)
#define UNREACHABLE() _fatal("Unreachable: ", "")
#define NYI(msg, ...) _fatal("Not Yet Implemented: ", msg, ##__VA_ARGS__)
#define fatal(msg, ...) _fatal("", msg, ##__VA_ARGS__);
#ifdef NDEBUG
#define trace(msg, ...)
#else
#define trace(msg, ...)                                 \
    do {                                                \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, msg, ##__VA_ARGS__);            \
        fputc('\n', stderr);                            \
    } while (0)
#endif

#define OPT(T) opt_##T
#define OPTDEF(T)                  \
    typedef struct _##T##_option { \
        bool ok;                   \
        T    value;                \
    } OPT(T)

#define OPTVAL(T, V) ((OPT(T)) { .ok = true, .value = (V) })
#define OPTNULL(T) ((OPT(T)) { 0 })

#define UNWRAP_T(T, Expr)                            \
    (                                                \
        {                                            \
            T __value = (Expr);                      \
            if (!__value.ok) {                       \
                fprintf(stderr, #Expr " is null\n"); \
                abort();                             \
            }                                        \
            (__value.value);                         \
        })

#define UNWRAP(T, Expr) UNWRAP_T(opt_##T, (Expr))

#define ORELSE_T(T, V, E)                     \
    (                                         \
        {                                     \
            T __val = (V);                    \
            ((__val.ok) ? __val.value : (E)); \
        })

#define ORELSE(T, V, E) ORELSE_T(opt_##T, (V), (E))

#define TRYOPT_T(T, V)            \
    (                             \
        {                         \
            T __val = (V);        \
            if (!__val.ok) {      \
                return (T) { 0 }; \
            }                     \
            (__val.value);        \
        })

#define TRYOPT(T, V) TRYOPT_T(opt_##T, (V))

#define TRYOPT_ADAPT_T(T, V, Ret) \
    (                             \
        {                         \
            T __val = (V);        \
            if (!__val.ok) {      \
                return (Ret);     \
            }                     \
            (__val.value);        \
        })

#define TRYOPT_ADAPT(T, V, Ret) \
    TRYOPT_ADAPT_T(opt_##T, (V), (Ret))

#define RES(T, E)      \
    struct {           \
        bool ok;       \
        union {        \
            T success; \
            E error;   \
        };             \
    }

#define RESVAL(T, V) \
    (T) { .ok = true, .success = (V) }
#define RESERR(T, E) \
    (T) { .ok = false, .error = (E) }

#define TRY(T, Expr)          \
    (                         \
        {                     \
            T __res = (Expr); \
            if (!__res.ok) {  \
                return __res; \
            }                 \
            (__res.success);  \
        })

#define MUST(T, Expr)                               \
    (                                               \
        {                                           \
            T __res = (Expr);                       \
            if (!__res.ok) {                        \
                fprintf(stderr, #Expr " failed\n"); \
                abort();                            \
            }                                       \
            (__res.success);                        \
        })

OPTDEF(int);
OPTDEF(size_t);
OPTDEF(long);
typedef unsigned long ulong;
OPTDEF(ulong);
OPTDEF(double);

typedef opt_size_t nodeptr;
#ifndef __cplusplus
extern nodeptr nullptr;
#else
#warning "Can't compile elrond with C++ yet"
#endif
#define nodeptr_ptr(v) ((nodeptr) { .ok = true, .value = (v) })

typedef struct slice {
    char  *items;
    size_t len;
} slice_t;

OPTDEF(slice_t);

typedef struct array {
    void  *items;
    size_t size;
} array_t;

#define slice_make(s, l) ((slice_t) { .items = (s), .len = (l) })
#define slice_from_cstr(s) ((slice_t) { .items = ((char *) s), .len = strlen((s)) })
#define C(s) slice_from_cstr(s)
#define slice_is_cstr(s) ((s).items[(s).len] == '\0')
#define SL "%.*s"
#define SLARG(s) (int) (s).len, (s).items

#define slice_fwrite(s, f)                    \
    do {                                      \
        slice_t __s = (s);                    \
        if (__s.len > 0) {                    \
            fwrite(__s.items, 1, __s.len, f); \
            fputc('\n', f);                   \
        }                                     \
    } while (0)

slice_t    slice_head(slice_t slice, size_t from_back);
slice_t    slice_first(slice_t slice, size_t num);
slice_t    slice_tail(slice_t slice, size_t from_start);
slice_t    slice_last(slice_t slice, size_t num);
slice_t    slice_sub(slice_t slice, size_t start, size_t end);
slice_t    slice_sub_by_length(slice_t slice, size_t start, size_t num);
bool       slice_startswith(slice_t slice, slice_t head);
bool       slice_endswith(slice_t slice, slice_t tail);
bool       slice_contains(slice_t haystack, slice_t needle);
opt_size_t slice_find(slice_t haystack, slice_t needle);
opt_size_t slice_rfind(slice_t haystack, slice_t needle);
opt_size_t slice_indexof(slice_t haystack, char needle);
opt_size_t slice_last_indexof(slice_t haystack, char needle);
opt_size_t slice_first_of(slice_t haystack, slice_t needles);
int        slice_cmp(slice_t s1, slice_t s2);
bool       slice_eq(slice_t s1, slice_t s2);
slice_t    slice_trim(slice_t s);
slice_t    slice_rtrim(slice_t s);
slice_t    slice_ltrim(slice_t s);
opt_ulong  slice_to_ulong(slice_t s, unsigned int base);
opt_long   slice_to_long(slice_t s, unsigned int base);

#endif /* __SLICE_H__ */

#ifdef SLICE_IMPLEMENTATION
#undef SLICE_IMPLEMENTATION
#ifndef SLICE_IMPLEMENTED
#define SLICE_IMPLEMENTED

slice_t slice_head(slice_t slice, size_t from_back)
{
    assert(from_back <= slice.len);
    return slice_make(slice.items, slice.len - from_back);
}

slice_t slice_first(slice_t slice, size_t num)
{
    assert(num <= slice.len);
    return slice_make(slice.items, num);
}

slice_t slice_tail(slice_t slice, size_t from_start)
{
    if (from_start > slice.len) {
        return C("");
    }
    return slice_make(slice.items + from_start, slice.len - from_start);
}

slice_t slice_last(slice_t slice, size_t num)
{
    assert(num <= slice.len);
    return slice_make(slice.items + (slice.len - num), num);
}

slice_t slice_sub(slice_t slice, size_t start, size_t end)
{
    assert(start <= slice.len && end <= slice.len && start <= end);
    return slice_make(slice.items + start, end - start);
}

slice_t slice_sub_by_length(slice_t slice, size_t start, size_t num)
{
    assert(start <= slice.len && start + num <= slice.len);
    return slice_make(slice.items + start, num);
}

bool slice_startswith(slice_t slice, slice_t head)
{
    if (slice.len < head.len) {
        return false;
    }
    return strncmp(slice.items, head.items, head.len) == 0;
}

bool slice_endswith(slice_t slice, slice_t tail)
{
    if (slice.len < tail.len) {
        return false;
    }
    return strncmp(slice.items + (slice.len - tail.len), tail.items, tail.len) == 0;
}

bool slice_contains(slice_t haystack, slice_t needle)
{
    if (haystack.len < needle.len) {
        return false;
    }
    for (size_t i = 0; i < haystack.len - needle.len; ++i) {
        if (strncmp(haystack.items + i, needle.items, needle.len) == 0) {
            return true;
        }
    }
    return false;
}

opt_size_t slice_find(slice_t haystack, slice_t needle)
{
    if (haystack.len < needle.len) {
        return OPTNULL(size_t);
    }
    for (size_t i = 0; i < haystack.len - needle.len; ++i) {
        if (strncmp(haystack.items + i, needle.items, needle.len) == 0) {
            return OPTVAL(size_t, i);
        }
    }
    return OPTNULL(size_t);
}

opt_size_t slice_rfind(slice_t haystack, slice_t needle)
{
    if (haystack.len < needle.len) {
        return OPTNULL(size_t);
    }
    size_t i = haystack.len - needle.len;
    do {
        if (strncmp(haystack.items + i, needle.items, needle.len) == 0) {
            return OPTVAL(size_t, i);
        }
    } while (i-- != 0);
    return OPTNULL(size_t);
}

opt_size_t slice_indexof(slice_t haystack, char needle)
{
    for (size_t i = 0; i < haystack.len; ++i) {
        if (haystack.items[i] == needle) {
            return OPTVAL(size_t, i);
        }
    }
    return OPTNULL(size_t);
}

opt_size_t slice_last_indexof(slice_t haystack, char needle)
{
    if (haystack.len > 0) {
        size_t i = haystack.len - 1;
        do {
            if (haystack.items[i] == needle) {
                return OPTVAL(size_t, i);
            }
        } while (i-- != 0);
    }
    return OPTNULL(size_t);
}

opt_size_t slice_first_of(slice_t haystack, slice_t needles)
{
    for (size_t i = 0; i < haystack.len; ++i) {
        if (slice_indexof(needles, haystack.items[i]).ok) {
            return OPTVAL(size_t, i);
        }
    }
    return OPTNULL(size_t);
}

int slice_cmp(slice_t s1, slice_t s2)
{
    if (s1.len != s2.len) {
        return s1.len - s2.len;
    }
    return memcmp(s1.items, s2.items, s1.len);
}

bool slice_eq(slice_t s1, slice_t s2)
{
    return slice_cmp(s1, s2) == 0;
}

slice_t slice_trim(slice_t s)
{
    return slice_rtrim(slice_ltrim(s));
}

slice_t slice_ltrim(slice_t s)
{
    char *ptr = s.items;
    while (isspace(*ptr) && ptr < s.items + s.len) {
        ++ptr;
    }
    if (ptr == s.items + s.len) {
        return (slice_t) { 0 };
    }
    return slice_make(ptr, s.len - (ptr - s.items));
}

slice_t slice_rtrim(slice_t s)
{
    size_t l = s.len;
    while (l > 0 && isspace(s.items[l - 1])) {
        --l;
    }
    if (l == 0) {
        return (slice_t) { 0 };
    }
    return slice_make(s.items, l);
}

opt_int digit_for_base(unsigned int digit, unsigned int base)
{
    if (digit >= '0' && digit < '0' + base) {
        return OPTVAL(int, digit - '0');
    } else if (digit >= 'A' && digit < 'A' + (base - 10)) {
        return OPTVAL(int, 10 + digit - 'A');
    } else if (digit >= 'a' && digit < 'a' + (base - 10)) {
        return OPTVAL(int, 10 + digit - 'a');
    }
    return (opt_int) { 0 };
}

opt_ulong slice_to_ulong(slice_t s, unsigned int base)
{
    if (s.len == 0) {
        return (opt_ulong) { 0 };
    }

    size_t ix = 0;
    while (ix < s.len && isspace(s.items[ix])) {
        ++ix;
    }
    if (ix == s.len) {
        return (opt_ulong) { 0 };
    }

    if (s.len > ix + 2 && s.items[ix] == '0') {
        if (s.items[ix + 1] == 'x' || s.items[ix + 1] == 'X') {
            if (base != 0 && base != 16) {
                return (opt_ulong) { 0 };
            }
            base = 16;
            ix = 2;
        }
        if (s.items[ix + 1] == 'b' || s.items[ix + 1] == 'B') {
            if (base != 0 && base != 2) {
                return (opt_ulong) { 0 };
            }
            base = 2;
            ix = 2;
        }
    }
    if (base == 0) {
        base = 10;
    }
    if (base > 36) {
        return (opt_ulong) { 0 };
    }

    size_t first = ix;
    ulong  val = 0;
    while (ix < s.len) {
        opt_int d = digit_for_base(s.items[ix], base);
        if (!d.ok) {
            break;
        }
        val = (val * base) + d.value;
        ++ix;
    }
    if (ix == first) {
        return (opt_ulong) { 0 };
    }
    while (ix < s.len && isspace(s.items[ix])) {
        ++ix;
    }
    if (ix < s.len) {
        return (opt_ulong) { 0 };
    }
    return OPTVAL(ulong, val);
}

opt_long slice_to_long(slice_t s, unsigned int base)
{
    if (s.len == 0) {
        return (opt_long) { 0 };
    }

    size_t ix = 0;
    while (ix < s.len && isspace(s.items[ix])) {
        ++ix;
    }
    if (ix == s.len) {
        return (opt_long) { 0 };
    }
    long    sign = 1;
    slice_t tail = slice_tail(s, ix);
    if (s.len > ix + 1 && (s.items[ix] == '-' || s.items[ix] == '+')) {
        sign = s.items[ix] == '-' ? -1 : 1;
        tail = slice_tail(s, ix + 1);
    }
    opt_ulong val = slice_to_ulong(tail, base);
    if (!val.ok) {
        return (opt_long) { 0 };
    }
    if (sign == 1) {
        if (val.value > (ulong) LONG_MAX) {
            return (opt_long) { 0 };
        }
        return OPTVAL(long, ((long) val.value));
    }
    if (val.value > ((ulong) (-(LONG_MIN + 1))) + 1) {
        return (opt_long) { 0 };
    }
    return OPTVAL(long, ((long) (~val.value + 1)));
}

#endif /* SLICE_IMPLEMENTED */
#endif /* SLICE_IMPLEMENTATION */

#ifdef SLICE_TEST

slice_t X = C("X");

int main()
{
    assert(X.len == 1);
    slice_t s = C("Hello");
    assert(s.len == 5);
    assert(memcmp(s.items, "Hello", 5) == 0);
    assert(slice_startswith(s, C("He")));
    assert(slice_endswith(s, C("lo")));
    assert(!slice_startswith(s, C("he")));
    assert(!slice_endswith(s, C("la")));
    slice_t spaces = C("   Hello   ");
    assert(slice_eq(slice_ltrim(spaces), C("Hello   ")));
    assert(slice_eq(slice_rtrim(spaces), C("   Hello")));
    assert(slice_eq(slice_trim(spaces), s));
    slice_t tabs = C(" \t Hello \t ");
    assert(slice_eq(slice_ltrim(tabs), C("Hello \t ")));
    assert(slice_eq(slice_rtrim(tabs), C(" \t Hello")));
    assert(slice_eq(slice_trim(tabs), s));
}

#endif /* SLICE_TEST */
