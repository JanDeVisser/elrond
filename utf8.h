/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __UTF8_H__
#define __UTF8_H__

#include <stdlib.h>

#include "slice.h"

size_t      utf32_length_for_utf8_slice(slice_t slice);
size_t      utf32_length_for_cstring(char const *c_string);
size_t      utf8_length_for_utf32_slice(slice_t slice);
slice_t     to_utf8(slice_t utf32);
slice_t     to_utf32(slice_t utf8);
slice_t     cstring_to_string(char const *cstring);
char const *string_to_cstring(slice_t string);

#endif /* __UTF8_H__ */
