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

slice_t typespec_to_string(nodes_t tree, nodeptr typespec)
{
    node_t *node = tree.items + typespec.value;
    assert(node->node_type == NT_TypeSpecification);
    switch (node->type_specification.kind) {
    case TYPN_Alias:
        return node->type_specification.alias_descr.name;
    default:
	return C("");
    }        
}    

