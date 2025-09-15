/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __DA_H__
#define __DA_H__

#include <stdio.h>
#include <stdlib.h>

#ifdef DA_TEST
#define SLICE_IMPLEMENTATION
#define DA_IMPLEMENTATION
#endif

#include "slice.h"

typedef struct _generic_da {
    void  *items;
    size_t len;
    size_t capacity;
} generic_da_t;

#define DA(T)            \
    struct {             \
        T     *items;    \
        size_t len;      \
        size_t capacity; \
    }

typedef DA(char) sb_t;
OPTDEF(sb_t);
typedef DA(sb_t) strings_t;

#define dynarr_ensure(arr, C)                                            \
    do {                                                                 \
        size_t __mincap = (C);                                           \
        if ((arr)->capacity < __mincap) {                                \
            size_t __elem_size = sizeof(*((arr)->items));                \
            size_t cap = ((arr)->capacity > 0) ? (arr)->capacity : 4;    \
            while (cap < __mincap) {                                     \
                cap *= 1.6;                                              \
            }                                                            \
            void *newitems = malloc(cap * __elem_size);                  \
            if (newitems == NULL) {                                      \
                fprintf(stderr, "Out of memory.\n");                     \
                abort();                                                 \
            }                                                            \
            if ((arr)->items) {                                          \
                memcpy(newitems, (arr)->items, __elem_size *(arr)->len); \
                free((arr)->items);                                      \
            }                                                            \
            (arr)->items = newitems;                                     \
            (arr)->capacity = cap;                                       \
        }                                                                \
    } while (0)

#define dynarr_clear(arr) \
    do {                  \
        (arr)->len = 0;   \
    } while (0)

#define dynarr_free(arr)                  \
    do {                                  \
        free((arr)->items);               \
        (arr)->len = (arr)->capacity = 0; \
    } while (0)

#define dynarr_append(arr, elem)              \
    do {                                      \
        dynarr_ensure((arr), (arr)->len + 1); \
        (arr)->items[(arr)->len++] = (elem);  \
    } while (0)

#define dynarr_cmp(arr1, arr2)                                                            \
    (                                                                                     \
        {                                                                                 \
            size_t __sz_1 = sizeof(*((arr1).items));                                      \
            size_t __sz_2 = sizeof(*((arr2).items));                                      \
            if (__sz_1 != __sz_2) {                                                       \
                fprintf(stderr, "Comparing dynamic arrays of different types\n");         \
                abort();                                                                  \
            }                                                                             \
            (generic_da_cmp((generic_da_t *) &(arr1), (generic_da_t *) &(arr2), __sz_1)); \
        })

#define dynarr_eq(arr1, arr2) (dynarr_cmp((arr1), (arr2)) == 0)

#define sb_as_slice(sb) (slice_make((sb).items, (sb).len))
#define sb_clear(sb) dynarr_clear((sb))
#define sb_free(sb) dynarr_free((sb))
#define sb_append_char(sb, ch)     \
    do {                           \
        dynarr_append((sb), (ch)); \
        dynarr_append((sb), '\0'); \
    } while (0);
#define sb_append_sb(sb, a) sb_append((sb), (sb_as_slice((a))))

int   generic_da_cmp(generic_da_t *da1, generic_da_t *da2, size_t elem_size);
sb_t *sb_append(sb_t *sb, slice_t slice);
sb_t *sb_printf(sb_t *sb, char const *fmt, ...);
sb_t *sb_vprintf(sb_t *sb, char const *fmt, va_list args);

#endif /* __DA_H__ */

#ifdef DA_IMPLEMENTATION
#undef DA_IMPLEMENTATION
#ifndef DA_IMPLEMENTED
#define DA_IMPLEMENTED

int generic_da_cmp(generic_da_t *da1, generic_da_t *da2, size_t elem_size)
{
    if (da1->len != da2->len) {
        return da1->len - da2->len;
    }
    return memcmp(da1->items, da2->items, da1->len * elem_size);
}

sb_t *sb_append(sb_t *sb, slice_t slice)
{
    dynarr_ensure(sb, sb->len + slice.len + 1);
    sb->items[sb->len + slice.len + 1] = 0;
    memcpy(sb->items + sb->len, slice.items, slice.len);
    sb->len += slice.len;
    return sb;
}

sb_t *sb_vprintf(sb_t *sb, char const *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    size_t n = vsnprintf(NULL, 0, fmt, copy);
    dynarr_ensure(sb, sb->len + n + 1);
    va_end(copy);
    vsnprintf(sb->items + sb->len, n + 1, fmt, args);
    sb->len += n;
    return sb;
}

sb_t *sb_printf(sb_t *sb, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    sb_vprintf(sb, fmt, args);
    return sb;
}

#endif /* DA_IMPLEMENTED */
#endif /* DA_IMPLEMENTATION */

#ifdef DA_TEST

int main()
{
    sb_t sb = { 0 };
    sb_printf(&sb, "Hello, %s\n", "World");
    assert(strncmp(sb.items, "Hello, World\n", strlen("Hello, World\n")) == 0);
    return 0;
}

#endif
