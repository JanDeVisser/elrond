/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "node.h"
#include "elrondlexer.h"
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

parser_t parse(slice_t text);
void     parser_print(parser_t *parser);

#endif /* __PARSER_H__ */
