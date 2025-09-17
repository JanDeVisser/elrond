/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "node.h"
#include "type.h"

nodeptr typespec_resolve(type_specification_t typespec)
{
    switch (typespec.kind) {
    case TYPN_Alias:
        return find_type(typespec.alias_descr.name);
    default:
        UNREACHABLE();
    }
}
