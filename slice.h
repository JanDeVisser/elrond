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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a < b) ? (a) : (b))
#define MAX(a, b) ((a > b) ? (a) : (b))
#define ALIGNAT(bytes, align) ((bytes + (align - 1)) & ~(align - 1))

#define UNREACHABLE()                                              \
    do {                                                           \
        fprintf(stderr, "%s:%d: Unreachable", __FILE__, __LINE__); \
        exit(1);                                                   \
    } while (0);

#define OPT(T) opt_##T
#define OPTDEF(T)                  \
    typedef struct _##T##_option { \
        bool ok;                   \
        T    value;                \
    } OPT(T)

#define OPTVAL(T, V) ((OPT(T)) { .ok = true, .value = (V) })
#define OPTNULL(T) ((OPT(T)) { 0 })

#define UNWRAP(T, V)		   \
    (                              \
        {                          \
            opt_##T __value = (V); \
            assert(__value.ok);    \
            __value.value;         \
        })

#define ORELSE(T, V, E)			      \
    (                                         \
        {                                     \
            opt_##T __val = (V);              \
            ((__val.ok) ? __val.value : (E)); \
        })

#define RES(T, E)      \
    struct {	       \
        bool ok;       \
        union {        \
            T success; \
            E error;   \
        };             \
    }

#define RESVAL(T, V) (T) { .ok = true, .success = (V) }
#define RESERR(T, E) (T) { .ok = false, .error = (E) }

OPTDEF(size_t);

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
    return slice_make(slice.items + start, slice.len - (end - start));
}

slice_t slice_sub_by_length(slice_t slice, size_t start, size_t num)
{
    assert(start <= slice.len && start + num <= slice.len);
    return slice_make(slice.items + start, slice.len - num);
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
}

#endif /* SLICE_TEST */
