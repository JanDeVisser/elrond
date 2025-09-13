/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define SLICE_IMPLEMENTATION
#define DA_IMPLEMENTATION
#define IO_IMPLEMENTATION
#define OPERATORS_IMPLEMENTATION
#define LEXER_IMPLEMENTATION
#define NODE_IMPLEMENTATION
#define WS_IGNORE
#define COMMENT_IGNORE

#include "slice.h"
#include "da.h"
#include "io.h"

#include "operators.h"
#include "lexer.h"
#include "parser.h"

opt_sb_t get_command_string()
{
    printf("*> ");
    char *line;
    size_t line_len;
    if (getline(&line, &line_len, stdin) < 0) {
        fprintf(stderr, "Error reading command\n");
	abort();
    }
    line[line_len] = 0;
    sb_t ret = {.items = line, .len = line_len - 1, .capacity = line_len };
    return OPTVAL(sb_t, ret);
}


int main(int argc, char const **argv)
{
    assert(argc > 1);
    slice_t file_name = C(argv[1]);
    opt_sb_t contents_maybe = slurp_file(file_name);
    if (!contents_maybe.ok) {
        fprintf(stderr, "Error reading file `%.*s`\n", (int) file_name.len, file_name.items);
        exit(1);
    }
    parser_t parser = parse(sb_as_slice(contents_maybe.value));
    if (parser.errors.len > 0) {
        for (size_t ix = 0; ix < parser.errors.len; ++ix) {
	    slice_t msg = sb_as_slice(parser.errors.items[ix]);
	    printf("%.*s\n", (int) msg.len, msg.items);
        }
	exit(1);
    }
    parser_print(&parser);
    return 0;
}
