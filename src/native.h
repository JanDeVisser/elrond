/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "da.h"
#include "resolve.h"
#include "slice.h"

#ifndef __NATIVE_H__
#define __NATIVE_H__

bool native_call(slice_t name, void *params, nodeptrs types, void *return_value, nodeptr return_type);

#endif /* __NATIVE_H__ */
