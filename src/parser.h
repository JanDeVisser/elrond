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

typedef struct _parser_ctx {
    bool     is_const;
    slices_t labels;
} parser_ctx_t;

typedef struct _parser {
    lexer_t      lexer;
    nodes_t      nodes;
    nodeptr      root;
    nodeptrs     namespaces;
    strings_t    errors;
    int          bound;
    parser_ctx_t ctx;
} parser_t;

parser_t        parse(slice_t name, slice_t text);
nodeptr         parse_module(parser_t *parser, slice_t name, slice_t text);
nodeptr         parse_snippet(parser_t *parser, slice_t text);
void            parser_print(parser_t *parser);
nodeptr         parser_normalize(parser_t *parser);
nodeptr         parser_bind(parser_t *parser);
node_t         *_parser_node(parser_t *parser, nodeptr n, char const *file, int line);
nodeptr         parser_append_node(parser_t *this, node_t n);
tokenlocation_t parser_location(parser_t *this, nodeptr n);
tokenlocation_t parser_location_merge(parser_t *this, nodeptr first_node, nodeptr second_node);
void            parser_verror(parser_t *parser, tokenlocation_t location, char const *fmt, va_list args);
void            parser_error(parser_t *parser, tokenlocation_t location, char const *fmt, ...);
opt_name_t      parser_resolve(parser_t *parser, slice_t name);
void            parser_add_name(parser_t *parser, slice_t name, nodeptr type, nodeptr decl);

#define parser_node(p, n) _parser_node(p, n, __FILE__, __LINE__)

#define parser_add_node(parser, nt, loc, ...) \
    parser_append_node((parser), (node_t) { .node_type = (nt), .location = (loc), .namespace = { 0 }, __VA_ARGS__ })
#define parser_node_type(p, n)                      \
    (                                               \
        {                                           \
            node_t *__node = parser_node((p), (n)); \
            __node->node_type;                      \
        })

#define parser_bound_type(p, n)                     \
    (                                               \
        {                                           \
            node_t *__node = parser_node((p), (n)); \
            __node->bound_type;                     \
        })

#define N(n) parser_node(parser, (n))
#define NT(n) parser_node_type(parser, (n))

#endif /* __PARSER_H__ */
