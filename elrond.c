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

#include "da.h"
#include "io.h"
#include "slice.h"

#include "operators.h"
#include "parser.h"
#include "type.h"

opt_sb_t get_command_string()
{
    printf("*> ");
    char  *line;
    size_t line_len;
    if (getline(&line, &line_len, stdin) < 0) {
        fprintf(stderr, "Error reading command\n");
        abort();
    }
    line[line_len] = 0;
    sb_t ret = { .items = line, .len = line_len - 1, .capacity = line_len };
    return OPTVAL(sb_t, ret);
}

void report(char const *hdr, parser_t *parser)
{
    static int stage = 1;
    printf("\nStage %d: %s\n", stage, hdr);
    printf("------------------------\n");
    if (parser->errors.len > 0) {
        for (size_t ix = 0; ix < parser->errors.len; ++ix) {
            slice_t msg = sb_as_slice(parser->errors.items[ix]);
            printf("%.*s\n", (int) msg.len, msg.items);
        }
        exit(1);
    }
    parser_print(parser);
    ++stage;
}    

int main(int argc, char const **argv)
{
    assert(argc > 1);
    slice_t  file_name = C(argv[1]);
    opt_sb_t contents_maybe = slurp_file(file_name);
    if (!contents_maybe.ok) {
        fprintf(stderr, "Error reading file `%.*s`\n", (int) file_name.len, file_name.items);
        exit(1);
    }
    type_registry_init();
    parser_t parser = parse(sb_as_slice(contents_maybe.value));
    report("Parsing", &parser);
    parser_normalize(&parser);
    report("Normalizing", &parser);
    do {    
	parser_bind(&parser);
    } while (!parser_bound_type(&parser, parser.root).ok && parser.bound != 0);
    report("Binding", &parser);
    return 0;
}
