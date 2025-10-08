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
#define FS_IMPLEMENTATION
#define CMDLINE_IMPLEMENTATION
#define PROCESS_IMPLEMENTATION
#define WS_IGNORE
#define COMMENT_IGNORE

#include "cmdline.h"
#include "da.h"
#include "fs.h"
#include "io.h"
#include "process.h"
#include "slice.h"

#include "arm64.h"
#include "ir.h"
#include "operators.h"
#include "parser.h"
#include "type.h"

bool do_list = false;

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
    if (do_list || do_trace) {
        printf("\nStage %d: %s\n", stage, hdr);
        printf("------------------------\n");
    }
    if (parser->errors.len > 0) {
        for (size_t ix = 0; ix < parser->errors.len; ++ix) {
            slice_t msg = sb_as_slice(parser->errors.items[ix]);
            printf("%.*s\n", (int) msg.len, msg.items);
        }
        exit(1);
    }
    if (do_list || do_trace) {
        parser_print(parser);
    }
    ++stage;
}

static app_description_t app_descr = {
    .name = "elrond",
    .shortdescr = "Elrond compiler",
    .description = "Compiler for the elrond language\n"
                   "https:://www.elrond-lang.com\n",
    .legal = "(c) finiandarcy.com",
    .options = {
        {
            .longopt = "keep-assembly",
            .description = "Do not remove intermediate assembler files",
            .value_required = false,
            .cardinality = COC_Set,
            .type = COT_Boolean,
        },
        {
            .longopt = "list",
            .option = 'l',
            .description = "Display intermediate listings",
            .value_required = false,
            .cardinality = COC_Set,
            .type = COT_Boolean,
        },
        {
            .longopt = "trace",
            .option = 't',
            .description = "Emit tracing/debug output",
            .value_required = false,
            .cardinality = COC_Set,
            .type = COT_Boolean,
        },
        { 0 } }
};

int main(int argc, char const **argv)
{
    parse_cmdline_args(&app_descr, argc, argv);
    do_trace = cmdline_is_set("trace");
    do_list = cmdline_is_set("list");
    slices_t args = cmdline_arguments();
    assert(args.len > 0);
    slice_t  file_name = C(args.items[0].items);
    opt_sb_t contents_maybe = slurp_file(file_name);
    if (!contents_maybe.ok) {
        fprintf(stderr, "Error reading file `%.*s`\n", (int) file_name.len, file_name.items);
        exit(1);
    }
    type_registry_init();

    slice_t name = file_name;
    if (slice_endswith(name, C(".elr"))) {
        name = slice_sub(name, 0, name.len - 4);
    }
    parser_t parser = parse(name, sb_as_slice(contents_maybe.value));
    report("Parsing", &parser);
    parser_normalize(&parser);
    report("Normalizing", &parser);
    do {
        parser_bind(&parser);
    } while (!parser_bound_type(&parser, parser.root).ok && parser.bound != 0);
    report("Binding", &parser);

    ir_generator_t gen = generate_ir(&parser, parser.root);
    if (do_trace) {
        list(stdout, &gen, nodeptr_ptr(0));
    }
    arm64_generate(&gen, nodeptr_ptr(0));
    return 0;
}
