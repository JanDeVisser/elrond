/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "elrondlexer.h"
#include "node.h"
#include "operators.h"
#include "slice.h"

#ifndef __PARSER_H__
#define __PARSER_H__

typedef struct _parser {
    lexer_t   lexer;
    nodes_t   nodes;
    nodeptr   root;
    strings_t errors;
} parser_t;

parser_t        parse(slice_t text);
void            parser_print(parser_t *parser);
nodeptr         parser_normalize(parser_t *parser);
nodeptr         parser_append_node(parser_t *this, node_t n);
node_t         *parser_node(parser_t *parser, nodeptr n);
tokenlocation_t parser_location(parser_t *this, nodeptr n);
tokenlocation_t parser_location_merge(parser_t *this, nodeptr first_node, nodeptr second_node);
void            parser_verror(parser_t *parser, tokenlocation_t location, char const *fmt, va_list args);
void            parser_error(parser_t *parser, tokenlocation_t location, char const *fmt, ...);

#define parser_add_node(parser, nt, loc, ...) \
    parser_append_node((parser), (node_t) { .node_type = (nt), .location = (loc), __VA_ARGS__ })
#define parser_copy_node(parser, node, nt, ...) \
    parser_append_node((parser), (node_t) { .node_type = (nt), .location = (node).location, __VA_ARGS__ })

#endif /* __PARSER_H__ */
