/*
 * Copyright (c) 2025, Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "parser.h"
#include "lexer.h"
#include "node.h"
#include "operators.h"
#include "slice.h"
#include <stdlib.h>

tokenlocation_t    parser_location(parser_t *this, nodeptr n);
tokenlocation_t    parser_location_merge(parser_t *this, nodeptr first_node, nodeptr second_node);
void               parser_verror(parser_t *parser, tokenlocation_t location, char const *fmt, va_list args);
void               parser_error(parser_t *parser, tokenlocation_t location, char const *fmt, ...);
bool               _parser_check_token(parser_t *parser, opt_token_t t, char const *fmt, ...);
bool               _parser_check_node(parser_t *parser, tokenlocation_t location, nodeptr n, char const *fmt, ...);
nodeptr            _parser_add_node(parser_t *this, node_t n);
node_t            *parser_node(parser_t *parser, nodeptr n);
nodeptr            parse_primary(parser_t *this);
nodeptr            parse_expression(parser_t *this, int minprec);
bool               check_op(parser_t *this);
opt_operator_def_t check_binop(parser_t *this);
opt_operator_def_t check_postfix_op(parser_t *this);
opt_operator_def_t check_prefix_op(parser_t *this);
opt_operator_def_t check_op_by_position(parser_t *this, position_t pos);
token_t            parse_statements(parser_t *this, nodeptrs *statements, nodeptr (*parser)(parser_t *));
nodeptr            parse_statement(parser_t *this);
nodeptr            parse_module_level_statement(parser_t *this);
nodeptr            parse_type(parser_t *this);
nodeptr            parse_preprocess(parser_t *this, nodetype_t node_type);
nodeptr            parse_break_continue(parser_t *this);
nodeptr            parse_defer(parser_t *this);
nodeptr            parse_enum(parser_t *parser);
nodeptr            parse_for_statement(parser_t *this);
nodeptr            parse_func(parser_t *parser);
nodeptr            parse_if_statement(parser_t *this);
nodeptr            parse_import(parser_t *this);
nodeptr            parse_loop(parser_t *this);
nodeptr            parse_public(parser_t *this);
nodeptr            parse_return_error(parser_t *this);
nodeptr            parse_struct(parser_t *this);
nodeptr            parse_var_decl(parser_t *this);
nodeptr            parse_while_statement(parser_t *this);
nodeptr            parse_yield_statement(parser_t *this);

#define parser_add_node(parser, nt, loc, ...) \
    _parser_add_node((parser), (node_t) { .node_type = (nt), .location = (loc), __VA_ARGS__ })

#define parser_check_token(parser, token, fmt, ...)                          \
    (                                                                        \
        {                                                                    \
            opt_token_t __t = (token);                                       \
            if (!_parser_check_token((parser), __t, (fmt), ##__VA_ARGS__)) { \
                return (nodeptr) { 0 };                                      \
            }                                                                \
            __t.value;                                                       \
        })
#define parser_check_node(parser, loc, n, fmt, ...)                                \
    (                                                                              \
        {                                                                          \
            nodeptr __n = (n);                                                     \
            if (!_parser_check_node((parser), (loc), __n, (fmt), ##__VA_ARGS__)) { \
                return (nodeptr) { 0 };                                            \
            }                                                                      \
            __n;                                                                   \
        })
#define parser_expect(parser, kind, fmt, ...)                                                    \
    (                                                                                            \
        {                                                                                        \
            lexerresult_t __res = lexer_expect(&((parser)->lexer), (kind));                      \
            if (!__res.ok) {                                                                     \
                parser_error((parser), parser_current_location((parser)), (fmt), ##__VA_ARGS__); \
                return nullptr;                                                                  \
            }                                                                                    \
            (__res.success);                                                                     \
        })
#define parser_expect_identifier(parser, fmt, ...)                                               \
    (                                                                                            \
        {                                                                                        \
            lexerresult_t __res = lexer_expect_identifier(&((parser)->lexer));                   \
            if (!__res.ok) {                                                                     \
                parser_error((parser), parser_current_location((parser)), (fmt), ##__VA_ARGS__); \
                return nullptr;                                                                  \
            }                                                                                    \
            (__res.success);                                                                     \
        })
#define parser_expect_symbol(parser, symbol, fmt, ...)                                                   \
    (                                                                                                    \
        {                                                                                                \
            lexerresult_t __res = lexer_expect_symbol(&((parser)->lexer), symbol);                       \
            if (!__res.ok) {                                                                             \
                if (fmt != NULL) {                                                                       \
                    parser_error((parser), parser_current_location((parser)), (fmt), ##__VA_ARGS__);     \
                } else {                                                                                 \
                    parser_error((parser), parser_current_location((parser)), "Expected `" #symbol "`"); \
                }                                                                                        \
                return nullptr;                                                                          \
            }                                                                                            \
            (__res.success);                                                                             \
        })

#define parser_text(parser, token) lexer_token_text(&((parser)->lexer), (token))
#define parser_current_location(parser) lexer_peek(&((parser)->lexer)).location

/* ------------------------------------------------------------------------ */

nodeptr nullptr = { 0 };

tokenlocation_t parser_location(parser_t *this, nodeptr n)
{
    return parser_node(this, n)->location;
}

tokenlocation_t parser_location_merge(parser_t *this, nodeptr first_node, nodeptr second_node)
{
    return tokenlocation_merge(
        parser_location(this, first_node),
        parser_location(this, second_node));
}

void parser_verror(parser_t *parser, tokenlocation_t location, char const *fmt, va_list args)
{
    sb_t msg = { 0 };
    sb_printf(&msg, "%zu:%zu: ", location.line + 1, location.column + 1);
    sb_vprintf(&msg, fmt, args);
    dynarr_append(&(parser->errors), msg);
}

void parser_error(parser_t *parser, tokenlocation_t location, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    parser_verror(parser, location, fmt, args);
    va_end(args);
}

bool _parser_check_token(parser_t *parser, opt_token_t t, char const *fmt, ...)
{
    if (!t.ok) {
        va_list args;
        va_start(args, fmt);
        parser_verror(parser, parser_current_location(parser), fmt, args);
        va_end(args);
    }
    return t.ok;
}

bool _parser_check_node(parser_t *parser, tokenlocation_t location, nodeptr n, char const *fmt, ...)
{
    if (!n.ok) {
        va_list args;
        va_start(args, fmt);
        parser_verror(parser, location, fmt, args);
        va_end(args);
    }
    return n.ok;
}

node_t *parser_node(parser_t *parser, nodeptr n)
{
    if (!n.ok) {
        fprintf(stderr, "null node dereferenced\n");
        abort();
    }
    return parser->nodes.items + n.value;
}

nodeptr _parser_add_node(parser_t *this, node_t n)
{
    dynarr_append(&this->nodes, n);
    printf("%zu %s\n", this->nodes.len - 1, node_type_name(n.node_type));
    return nodeptr_ptr(this->nodes.len - 1);
}

token_t parse_statements(parser_t *this, nodeptrs *statements, nodeptr (*parser)(parser_t *))
{
    while (true) {
        token_t t = lexer_peek(&this->lexer);
        if (token_matches(t, TK_EndOfFile) || token_matches_symbol(t, '}')) {
            lexer_lex(&this->lexer);
            return t;
        }
        nodeptr stmt = parser(this);
        if (stmt.ok) {
            dynarr_append(statements, stmt);
        }
    }
}

nodeptr parse_module_level_statement(parser_t *this)
{
    token_t t = lexer_peek(&this->lexer);
    switch (t.kind) {
    case TK_EndOfFile:
        parser_error(this, t.location, "Unexpected end of file");
        return nullptr;
    case TK_Identifier:
        lexer_lex(&this->lexer);
        parser_expect_symbol(this, ':', "Expected variable declaration");
        return parse_statement(this);
    case TK_Keyword: {
        switch (t.keyword) {
        case KW_Const:
            lexer_lex(&this->lexer);
            return parse_module_level_statement(this);
        case KW_Enum:
            return parse_enum(this);
        case KW_Func:
            return parse_func(this);
        case KW_Import:
            return parse_import(this);
        case KW_Include:
            return parse_preprocess(this, NT_Include);
        case KW_Public:
            return parse_public(this);
        case KW_Struct:
            return parse_struct(this);
        default:
            break;
        }
    } break;
    default:
        break;
    }
    lexer_lex(&this->lexer);
    slice_t text = parser_text(this, t);
    parser_error(this, t.location, "Unexpected token `%.*s`", (int) text.len, text.items);
    return nullptr;
}

nodeptr parse_statement(parser_t *this)
{
    lexer_t *l = &this->lexer;
    token_t  t = lexer_peek(l);
    switch (t.kind) {
    case TK_EndOfFile:
        parser_error(this, t.location, "Unexpected end of file");
        return nullptr;
    case TK_Identifier:
        if (lexer_has_lookback(l, 2)
            && token_matches_symbol(lexer_lookback(l, 2), ':')
            && token_matches(lexer_lookback(l, 1), TK_Identifier)) {
            // This is the type of a variable decl:
            return parse_var_decl(this);
        }
        lexer_lex(l);
        if (lexer_accept_symbol(l, ':')) {
            return parse_statement(this);
        }
        lexer_push_back(l);
        // Fall through
    case TK_Number:
    case TK_String:
        return parse_expression(this, 0);
    case TK_Keyword: {
        switch (t.keyword) {
        case KW_Break:
        case KW_Continue:
            return parse_break_continue(this);
        case KW_Const:
            lexer_lex(l);
            return parse_statement(this);
        case KW_Defer:
            return parse_defer(this);
        case KW_Embed:
            return parse_preprocess(this, NT_Embed);
        case KW_Enum:
            return parse_enum(this);
        case KW_Error:
            return parse_return_error(this);
        case KW_For:
            return parse_for_statement(this);
        case KW_Func:
            return parse_func(this);
        case KW_If:
            return parse_if_statement(this);
        case KW_Include:
            return parse_preprocess(this, NT_Include);
        case KW_Loop:
            return parse_loop(this);
        case KW_Return:
            return parse_return_error(this);
        case KW_Struct:
            return parse_struct(this);
        case KW_While:
            return parse_while_statement(this);
        case KW_Yield:
            return parse_yield_statement(this);
        default: {
            slice_t text = parser_text(this, t);
            parser_error(this, t.location, "Unexpected keyword `%.*s`", (int) text.len, text.items);
            lexer_lex(l);
            return nullptr;
        }
        }
    } break;
    case TK_Symbol:
        switch (t.symbol) {
        case ';':
            return parser_add_node(this, NT_Void, lexer_lex(l).location);
        case '{': {
            lexer_lex(l);
            opt_slice_t label = { 0 };
            nodeptrs    block = { 0 };
            token_t     end_token = parse_statements(this, &block, parse_statement);
            if (!token_matches_symbol(end_token, '}')) {
                parser_error(this, t.location, "Unexpected end of block");
                return nullptr;
            } else {
                if (block.len == 0) {
                    return parser_add_node(this, NT_Void, tokenlocation_merge(t.location, end_token.location));
                }
                return parser_add_node(
                    this,
                    NT_StatementBlock,
                    tokenlocation_merge(t.location, end_token.location),
                    .statement_block = { .statements = block, .label = label });
            }
        }
        case '=':
            if (lexer_has_lookback(l, 2)
                && token_matches_symbol(lexer_lookback(l, 1), ':')
                && token_matches(lexer_lookback(l, 2), TK_Identifier)) {
                // This is the '=' of a variable decl with implied type:
                return parse_var_decl(this);
            }
            // Fall through
        default: {
            nodeptr expr = parse_expression(this, 0);
            if (expr.ok) {
                return expr;
            }
            parser_error(this, t.location, "Unexpected symbol `%c`", t.symbol);
            lexer_lex(l);
            return nullptr;
        }
        }
        break;
    case TK_Raw: {
        rawtext_t raw = t.rawtext;
        lexer_lex(l);
        if (raw.terminated) {
            return parser_add_node(
                this,
                NT_Insert,
                tokenlocation_merge(t.location, parser_current_location(this)),
                .raw_text = parser_text(this, t));
        } else {
            parser_error(this, t.location, "Unclosed `@insert` block");
            return nullptr;
        }
    }
    default: {
        lexer_lex(l);
        slice_t text = parser_text(this, t);
        parser_error(this, t.location, "Unexpected token `%.*s`", (int) text.len, text.items);
        return nullptr;
    }
    }
}

nodeptr parse_primary(parser_t *this)
{
    token_t token = lexer_peek(&this->lexer);
    nodeptr ret = nullptr;
    switch (token.kind) {
    case TK_Number: {
        ret = parser_add_node(
            this,
            NT_Number,
            token.location,
            .number = (number_t) { .number = parser_text(this, token), .number_type = token.number });
        lexer_lex(&this->lexer);
    } break;
    case TK_String: {
        lexer_lex(&this->lexer);
        if (token.quoted_string.quote_type == QT_SingleQuote && token.location.length != 1) {
            parser_error(this, token.location, "Single quoted string should contain exactly one character");
            return nullptr;
        }
        ret = parser_add_node(
            this,
            NT_String,
            token.location,
            .string = (string_t) { .string = parser_text(this, token), .quote_type = token.quoted_string.quote_type });
    } break;
    case TK_Identifier: {
        lexer_lex(&this->lexer);
        // const bm = this.lexer.bookmark();
        // if (this.lexer.accept_symbol('<')) {
        //     TypeSpecifications specs;
        //     while (true) {
        //         const spec = parse_type();
        //         if (spec == null) {
        //             break;
        //         }
        //         specs.push_back(spec);
        //         if (this.lexer.accept_symbol('>')) {
        //             return this.tree.add(StampedIdentifier, token.location + this.lexer.location, this.text_of(token), specs);
        //         }
        //         if (!this.lexer.accept_symbol(',')) {
        //             break;
        //         }
        //     }
        // }
        // this.lexer.push_back(bm);
        ret = parser_add_node(
            this,
            NT_Identifier,
            token.location,
            .identifier = parser_text(this, token));
    } break;
    case TK_Keyword: {
        switch (token.keyword) {
        case KW_Embed:
            return parse_preprocess(this, NT_Embed);
        case KW_Include:
            return parse_preprocess(this, NT_Include);
        case KW_False:
            lexer_lex(&this->lexer);
            return parser_add_node(
                this,
                NT_BoolConstant,
                token.location,
                .bool_constant = false);
        case KW_True:
            lexer_lex(&this->lexer);
            return parser_add_node(this, NT_BoolConstant, token.location, .bool_constant = true);
        case KW_Null:
            lexer_lex(&this->lexer);
            return parser_add_node(this, NT_Null, token.location);
        default: {
            opt_operator_def_t op_maybe = check_prefix_op(this);
            if (op_maybe.ok) {
                operator_def_t  operator= op_maybe.value;
                binding_power_t bp = binding_power(operator);
                lexer_lex(&this->lexer);
                nodeptr operand = parser_check_node(
                    this,
                    token.location,
                    ((operator.op == OP_Sizeof) ? parse_type(this) : parse_expression(this, bp.right)),
                    "Expected operand following prefix1 operator");
                return parser_add_node(
                    this,
                    NT_UnaryExpression,
                    token.location,
                    .unary_expression = { .op = operator.op, .operand = operand });
            } else {
                slice_t token_text = parser_text(this, token);
                parser_error(this, token.location,
                    "Expected operand following prefix operator `%.*s`",
                    (int) token_text.len, token_text.items);
                return nullptr;
            }
        } break;
        }
    }
    case TK_Symbol: {
        if (token.symbol == '(') {
            lexer_lex(&this->lexer);
            if (lexer_accept_symbol(&this->lexer, ')')) {
                return parser_add_node(this, NT_Void, token.location);
            }
            ret = parse_expression(this, 0);
            if (!lexer_expect_symbol(&this->lexer, ')').ok) {
                parser_error(this, token.location, "Expected `)`");
                return nullptr;
            }
        }
    } break;
    default: {
        slice_t token_text = parser_text(this, token);
        parser_error(this, token.location,
            "Unexpected token %s `%.*s`",
            tokenkind_name(token.kind), (int) token_text.len, token_text.items);
    }
    }
    if (!ret.ok) {
        parser_error(this, token.location, "Expected primary expression");
    }
    return ret;
}

// Shamelessly stolen from here:
// https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
nodeptr parse_expression(parser_t *this, precedence_t min_prec)
{
    nodeptr lhs = parse_primary(this);
    if (!lhs.ok) {
        return lhs;
    }
    while (!lexer_matches(&this->lexer, TK_EndOfFile) && check_op(this)) {
        opt_operator_def_t op_maybe = check_postfix_op(this);
        if (op_maybe.ok) {
            operator_def_t  operator= op_maybe.value;
            binding_power_t bp = binding_power(operator);
            if (bp.left < min_prec) {
                break;
            }
            if (operator.op == OP_Subscript) {
                lexer_lex(&this->lexer);
                nodeptr rhs = parser_check_node(
                    this,
                    lexer_peek(&this->lexer).location,
                    parse_expression(this, 0),
                    "Expected subscript expression");
                lexerresult_t bracket_maybe = lexer_expect_symbol(&this->lexer, ']');
                if (!bracket_maybe.ok) {
                    parser_error(this, lexer_peek(&this->lexer).location, "Expected ']'");
                    return nullptr;
                };
                lhs = parser_add_node(
                    this,
                    NT_BinaryExpression,
                    parser_location_merge(this, lhs, rhs),
                    .binary_expression = { .lhs = lhs, .op = operator.op, .rhs = rhs });
            } else {
                lhs = parser_add_node(
                    this,
                    NT_UnaryExpression,
                    tokenlocation_merge(parser_node(this, lhs)->location, lexer_peek(&this->lexer).location),
                    .unary_expression = { .op = operator.op, .operand = lhs });
            }
            continue;
        }
        opt_operator_def_t binop_maybe = check_binop(this);
        if (binop_maybe.ok) {
            operator_def_t  operator= binop_maybe.value;
            binding_power_t bp = binding_power(operator);
            if (bp.left < min_prec) {
                break;
            }
            if (operator.op == OP_Call) {
                // Don't lex the '(' so parse_primary will return a
                // single expression, probably a binop with op = ','.
                nodeptr param_list = parser_check_node(
                    this,
                    parser_node(this, lhs)->location,
                    parse_primary(this),
                    "Could not parse function call argument list");
                lhs = parser_add_node(
                    this,
                    NT_BinaryExpression,
                    parser_location_merge(this, lhs, param_list),
                    .binary_expression = { .lhs = lhs, .op = OP_Call, .rhs = param_list });
            } else {
                lexer_lex(&this->lexer);
                nodeptr rhs = parser_check_node(
                    this,
                    parser_node(this, lhs)->location,
                    ((operator.op == OP_Cast) ? parse_type(this) : parse_expression(this, bp.right)),
                    "Expected right-hand expression");
                lhs = parser_add_node(
                    this,
                    NT_BinaryExpression,
                    parser_location_merge(this, lhs, rhs),
                    .binary_expression = { .lhs = lhs, .op = operator.op, .rhs = rhs });
            }
            continue;
        }
        break;
    }
    return lhs;
}

bool check_op(parser_t *this)
{
    token_t token = lexer_peek(&this->lexer);
    if (!token_matches(token, TK_Symbol) && !token_matches(token, TK_Keyword)) {
        return false;
    }
    for (int ix = 0; ix < OPERATORS_SZ; ++ix) {
        operator_def_t operator= operators[ix];
        switch (operator.kind) {
        case TK_Symbol:
            if (token_matches_symbol(token, operator.sym)) {
                return true;
            }
            break;
        case TK_Keyword:
            if (token_matches_keyword(token, operator.keyword)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

opt_operator_def_t check_binop(parser_t *this)
{
    return check_op_by_position(this, POS_Infix);
}

opt_operator_def_t check_prefix_op(parser_t *this)
{
    return check_op_by_position(this, POS_Prefix);
}

opt_operator_def_t check_postfix_op(parser_t *this)
{
    return check_op_by_position(this, POS_Postfix);
}

opt_operator_def_t check_op_by_position(parser_t *this, position_t pos)
{
    token_t token = lexer_peek(&this->lexer);
    if (!token_matches(token, TK_Symbol) && !token_matches(token, TK_Keyword)) {
        return OPTNULL(operator_def_t);
    }
    for (int ix = 0; ix < OPERATORS_SZ; ++ix) {
        operator_def_t operator= operators[ix];
        if (operator.position != pos) {
            continue;
        }
        switch (operator.kind) {
        case TK_Symbol:
            if (token_matches_symbol(token, operator.sym)) {
                return OPTVAL(operator_def_t, operator);
            }
            break;
        case TK_Keyword:
            if (token_matches_keyword(token, operator.keyword)) {
                return OPTVAL(operator_def_t, operator);
            }
            break;
        default:
            break;
        }
    }
    return OPTNULL(operator_def_t);
}

nodeptr parse_type(parser_t *this)
{
    token_t t = lexer_peek(&this->lexer);
    if (lexer_accept_symbol(&this->lexer, '&')) {
        nodeptr type = parse_type(this);
        if (!type.ok) {
            return nullptr;
        }
        return parser_add_node(
            this,
            NT_TypeSpecification,
            tokenlocation_merge(t.location, parser_location(this, type)),
            .type_specification = { .kind = TYP_Reference, .referencing = type });
    }
    if (lexer_accept_symbol(&this->lexer, '?')) {
        nodeptr type = parse_type(this);
        if (!type.ok) {
            return nullptr;
        }
        return parser_add_node(
            this,
            NT_TypeSpecification,
            tokenlocation_merge(t.location, parser_location(this, type)),
            .type_specification = { .kind = TYP_Optional, .optional_of = type });
    }
    if (lexer_accept_symbol(&this->lexer, '[')) {
        if (lexer_accept_symbol(&this->lexer, ']')) {
            nodeptr type = parse_type(this);
            if (!type.ok) {
                return nullptr;
            }
            return parser_add_node(
                this,
                NT_TypeSpecification,
                tokenlocation_merge(t.location, parser_location(this, type)),
                .type_specification = { .kind = TYP_Slice, .slice_of = type });
        }
        if (lexer_accept_symbol(&this->lexer, '0')) {
            lexerresult_t res = lexer_expect_symbol(&this->lexer, ']');
            if (!res.ok) {
                parser_error(this, t.location, "Expected `]` to close `[0`");
                return nullptr;
            }
            nodeptr type = parse_type(this);
            if (!type.ok) {
                return nullptr;
            }
            return parser_add_node(
                this,
                NT_TypeSpecification,
                tokenlocation_merge(t.location, parser_location(this, type)),
                .type_specification = { .kind = TYP_ZeroTerminatedArray, .array_of = type });
        }
        if (lexer_accept_symbol(&this->lexer, '*')) {
            lexerresult_t res = lexer_expect_symbol(&this->lexer, ']');
            if (!res.ok) {
                parser_error(this, t.location, "Expected `]` to close `[*`");
                return nullptr;
            }
            nodeptr type = parse_type(this);
            if (!type.ok) {
                return nullptr;
            }
            return parser_add_node(
                this,
                NT_TypeSpecification,
                tokenlocation_merge(t.location, parser_location(this, type)),
                .type_specification = { .kind = TYP_DynArray, .array_of = type });
        }
        lexerresult_t res = lexer_expect(&this->lexer, TK_Number);
        if (!res.ok) {
            parser_error(this, t.location, "Expected array size, `0` or `]`");
            return nullptr;
        } else if (res.success.number == NUM_Decimal) {
            parser_error(this, res.success.location, "Array size must be integer");
            return nullptr;
        } else {
            token_t size_token = res.success;
            res = lexer_expect_symbol(&this->lexer, ']');
            if (!res.ok) {
                parser_error(this, t.location, "Expected `]` to close array descriptor");
                return nullptr;
            }
            ulong   size = UNWRAP(ulong, slice_to_ulong(parser_text(this, size_token), 0));
            nodeptr type = parse_type(this);
            if (!type.ok) {
                return nullptr;
            }
            return parser_add_node(
                this,
                NT_TypeSpecification,
                tokenlocation_merge(t.location, parser_location(this, type)),
                .type_specification = { .kind = TYP_Array, .array_descr = { .array_of = type, .size = size } });
        }
    }

    token_t name = parser_check_token(this, lexer_accept_identifier(&this->lexer), "Expected type name");

    nodeptrs arguments = { 0 };
    if (lexer_accept_symbol(&this->lexer, '<')) {
        while (true) {
            if (lexer_accept_symbol(&this->lexer, '>')) {
                break;
            }
            nodeptr arg = parser_check_node(this, lexer_peek(&this->lexer).location, parse_type(this), "Expected template argument specification");
            dynarr_append(&arguments, arg);
            if (lexer_accept_symbol(&this->lexer, '>')) {
                break;
            }
            lexerresult_t res = lexer_expect_symbol(&this->lexer, ',');
            if (!res.ok) {
                parser_error(this, t.location, "Expected `,` or `>`");
                return nullptr;
            }
        }
    }
    nodeptr type = parser_add_node(
        this,
        NT_TypeSpecification,
        tokenlocation_merge(t.location, parser_current_location(this)),
        .type_specification = { .kind = TYP_Alias, .alias_descr = { .name = parser_text(this, name), .arguments = arguments } });
    if (lexer_accept_symbol(&this->lexer, '/')) {
        nodeptr error_type = parse_type(this);
        if (error_type.ok) {
            return parser_add_node(
                this,
                NT_TypeSpecification,
                tokenlocation_merge(t.location, parser_location(this, type)),
                .type_specification = { .kind = TYP_Result, .result_descr = { .success = type, .error = error_type } });
        }
        return nullptr;
    }
    return type;
}

nodeptr parse_preprocess(parser_t *this, nodetype_t node_type)
{
    token_t kw = lexer_lex(&this->lexer);
    parser_expect_symbol(this, '(', NULL);
    token_t file_name = parser_expect(this, TK_String, "Expected file name");
    slice_t fname = parser_text(this, file_name);
    fname = slice_sub(fname, 1, fname.len - 1);
    parser_expect_symbol(this, ')', NULL);
    return parser_add_node(
        this,
        node_type,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .identifier = fname);
}

nodeptr parse_break_continue(parser_t *this)
{
    token_t kw = lexer_lex(&this->lexer);
    assert(token_matches_keyword(kw, KW_Break) || token_matches_keyword(kw, KW_Continue));
    opt_slice_t label = { 0 };
    if (lexer_accept_symbol(&this->lexer, ':')) {
        opt_token_t lbl = lexer_accept_identifier(&this->lexer);
        if (lbl.ok) {
            parser_error(this, parser_current_location(this), "Expected label name after `:`");
            return nullptr;
        }
        label = OPTVAL(slice_t, parser_text(this, lbl.value));
    }
    return parser_add_node(
        this,
        (token_matches_keyword(kw, KW_Break)) ? NT_Break : NT_Continue,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .label = label);
}

nodeptr parse_defer(parser_t *this)
{
    token_t kw = lexer_lex(&this->lexer);
    nodeptr stmt = parser_check_node(this, parser_current_location(this), parse_statement(this), "Could not parse defer statement");
    return parser_add_node(
        this,
        NT_Defer,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .statement = stmt);
}

nodeptr parse_enum(parser_t *this)
{
    token_t kw = lexer_lex(&this->lexer);
    token_t name = parser_expect_identifier(this, "Expected enum name");
    nodeptr underlying = { 0 };
    if (lexer_accept_symbol(&this->lexer, ':')) {
        underlying = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected underlying type after `:`");
    }
    parser_expect_symbol(this, '{', NULL);
    nodeptrs enum_values = { 0 };
    while (!lexer_accept_symbol(&this->lexer, '}')) {
        token_t label = parser_expect_identifier(this, "Expected enum value label");
        nodeptr payload = { 0 };
        if (lexer_accept_symbol(&this->lexer, '(')) {
            payload = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected enum value payload type");
            parser_expect_symbol(this, ')', "Expected `)` to close enum value payload type");
        }
        nodeptr value_node = { 0 };
        if (lexer_accept_symbol(&this->lexer, '=')) {
            token_t value = parser_expect(this, TK_Number, "Expected enum value");
            if (value.number == NUM_Decimal) {
                parser_error(this, value.location, "Enum value must be integer number");
                return nullptr;
            }
            value_node = parser_add_node(
                this,
                NT_Number,
                value.location,
                .number = { .number = parser_text(this, value), .number_type = value.number });
        }
        dynarr_append(
            &enum_values,
            parser_add_node(
                this,
                NT_EnumValue,
                tokenlocation_merge(label.location, parser_current_location(this)),
                .enum_value = { .label = parser_text(this, label), .value = value_node, .payload = payload }));
        if (!lexer_accept_symbol(&this->lexer, ',') && !token_matches_symbol(lexer_peek(&this->lexer), '}')) {
            parser_error(this, parser_current_location(this), "Expected `,` or `}`");
            return nullptr;
        }
    }
    return parser_add_node(
        this,
        NT_Enum,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .enumeration = { .name = parser_text(this, name), .underlying = underlying, .values = enum_values });
}

nodeptr parse_for_statement(parser_t *this)
{
    opt_slice_t label;
    if (lexer_has_lookback(&this->lexer, 2)
        && token_matches_symbol(lexer_lookback(&this->lexer, 1), ':')
        && token_matches(lexer_lookback(&this->lexer, 2), TK_Identifier)) {
        label = OPTVAL(slice_t, parser_text(this, lexer_lookback(&this->lexer, 2)));
    }
    token_t for_token = lexer_lex(&this->lexer);

    token_t var_name = parser_expect_identifier(this, "Expected `for` range variable name");
    token_t t = lexer_peek(&this->lexer);
    if (token_matches(t, TK_Identifier) && slice_eq(parser_text(this, t), C("in"))) {
        lexer_lex(&this->lexer);
    }
    nodeptr range = parser_check_node(this, parser_current_location(this), parse_expression(this, 0), "Error parsing `for` range");
    nodeptr stmt = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `for` block");
    return parser_add_node(
        this,
        NT_ForStatement,
        tokenlocation_merge(for_token.location, parser_current_location(this)),
        .for_statement = { .variable = parser_text(this, var_name), .range = range, .statement = stmt, .label = label });
}

nodeptr parse_func(parser_t *this)
{
    token_t  func = lexer_lex(&this->lexer);
    token_t  name_tok = parser_expect_identifier(this, "Expected function name");
    slice_t  name = parser_text(this, name_tok);
    nodeptrs generics = { 0 };
    if (lexer_accept_symbol(&this->lexer, '<')) {
        while (true) {
            if (lexer_accept_symbol(&this->lexer, '>')) {
                break;
            }
            token_t generic_tok = parser_expect_identifier(this, "Expected generic name");
            slice_t generic_name = parser_text(this, generic_tok);
            dynarr_append(
                &generics,
                parser_add_node(
                    this,
                    NT_Identifier,
                    generic_tok.location,
                    .identifier = generic_name));
            if (lexer_accept_symbol(&this->lexer, '>')) {
                break;
            }
            parser_expect_symbol(this, ',', "Expected `,` or `>` in generic parameter list");
        }
    }
    parser_expect_symbol(this, '(', NULL);
    nodeptrs params = { 0 };
    while (true) {
        if (lexer_accept_symbol(&this->lexer, ')')) {
            break;
        }
        token_t param_name_tok = parser_expect_identifier(this, "Expected parameter name");
        slice_t param_name = parser_text(this, param_name_tok);
        parser_expect_symbol(this, ':', "Expected `:` in function parameter declaration");
        nodeptr param_type = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected parameter type");
        dynarr_append(
            &params,
            parser_add_node(
                this,
                NT_Parameter,
                tokenlocation_merge(param_name_tok.location, parser_current_location(this)),
                .variable_declaration = { .name = param_name, .type = param_type }));
        if (lexer_accept_symbol(&this->lexer, ')')) {
            break;
        }
        parser_expect_symbol(this, ',', "Expected `,` or `)`");
    }
    nodeptr return_type = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected return type");
    nodeptr signature = parser_add_node(
        this,
        NT_Signature,
        tokenlocation_merge(func.location, parser_current_location(this)),
        .signature = { .name = name, .parameters = params, .return_type = return_type });
    nodeptr impl = nullptr;
    if (lexer_accept_keyword(&this->lexer, KW_ForeignLink)) {
        token_t foreign_func_tok = parser_expect(this, TK_String, "Expected foreign function name");
        if (foreign_func_tok.quoted_string.quote_type != QT_DoubleQuote) {
            parser_error(this, foreign_func_tok.location, "Expected double-quoted foreign function name");
            return nullptr;
        }
        slice_t foreign_func = parser_text(this, foreign_func_tok);
        if (foreign_func.len <= 2) {
            parser_error(this, foreign_func_tok.location, "Invalid foreign function name");
            return nullptr;
        }
        foreign_func = slice_sub(foreign_func, 1, foreign_func.len - 1);
        impl = parser_add_node(
            this,
            NT_ForeignFunction,
            foreign_func_tok.location,
            .identifier = foreign_func);
    } else {
        impl = parser_check_node(this, parser_current_location(this), parse_statement(this), "Could not parse function implementation");
    }
    return parser_add_node(
        this,
        NT_Function,
        tokenlocation_merge(func.location, parser_current_location(this)),
        .function = { .name = name, .signature = signature, .implementation = impl });
}

nodeptr parse_if_statement(parser_t *this)
{
    token_t if_token = lexer_lex(&this->lexer);
    nodeptr condition = parser_check_node(this, parser_current_location(this), parse_expression(this, 0), "Error parsing `if` condition");
    nodeptr if_branch = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `if` branch");
    nodeptr else_branch = nullptr;
    if (lexer_accept_keyword(&this->lexer, KW_Else)) {
        else_branch = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `else` branch");
    }
    return parser_add_node(
        this,
        NT_IfStatement,
        tokenlocation_merge(if_token.location, parser_current_location(this)),
        .if_statement = { .condition = condition, .if_branch = if_branch, .else_branch = else_branch });
}

nodeptr parse_import(parser_t *this)
{
    token_t import_token = lexer_lex(&this->lexer);
    sb_t    path = { 0 };
    do {
        token_t path_piece = parser_expect_identifier(this, "Expected import path component");
        sb_append(&path, parser_text(this, path_piece));
        if (!lexer_accept_symbol(&this->lexer, '.')) {
            break;
        }
        sb_append_char(&path, '.');
    } while (true);
    return parser_add_node(
        this,
        NT_Import,
        tokenlocation_merge(import_token.location, parser_current_location(this)),
        .identifier = sb_as_slice(path));
}

nodeptr parse_loop(parser_t *this)
{
    opt_slice_t label;
    if (lexer_has_lookback(&this->lexer, 2)
        && token_matches_symbol(lexer_lookback(&this->lexer, 1), ':')
        && token_matches(lexer_lookback(&this->lexer, 2), TK_Identifier)) {
        label = OPTVAL(slice_t, parser_text(this, lexer_lookback(&this->lexer, 2)));
    }
    token_t loop_token = lexer_lex(&this->lexer);
    nodeptr stmt = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `loop` block");
    return parser_add_node(
        this,
        NT_LoopStatement,
        tokenlocation_merge(loop_token.location, parser_current_location(this)),
        .loop_statement = { .statement = stmt, .label = label });
}

nodeptr parse_public(parser_t *this)
{
    token_t kw = lexer_lex(&this->lexer);
    nodeptr decl = parse_module_level_statement(this);
    if (!decl.ok) {
        return nullptr;
    }
    slice_t name;
    node_t *decl_node = parser_node(this, decl);
    switch (decl_node->node_type) {
    case NT_Enum:
        name = decl_node->enumeration.name;
        break;
    case NT_Function:
        name = decl_node->function.name;
        break;
    case NT_PublicDeclaration:
        parser_error(this, decl_node->location, "Double public declaration");
        return nullptr;
    case NT_Struct:
        name = decl_node->structure.name;
        break;
    case NT_VariableDeclaration:
        name = decl_node->variable_declaration.name;
        break;
    default:
        parser_error(this, decl_node->location, "Cannot declare statement public");
        return nullptr;
    }
    return parser_add_node(
        this,
        NT_PublicDeclaration,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .public_declaration = { .name = name, .declaration = decl });
}

nodeptr parse_return_error(parser_t *this)
{
    token_t    kw = lexer_lex(&this->lexer);
    nodeptr    expr = parser_check_node(this, parser_current_location(this), parse_expression(this, 0), "Error parsing return expression");
    nodetype_t node_type = (token_matches_keyword(kw, KW_Return)) ? NT_Return : NT_Error;
    return parser_add_node(
        this,
        node_type,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .statement = expr);
}

nodeptr parse_struct(parser_t *this)
{
    token_t kw = lexer_lex(&this->lexer);
    token_t name_tok = parser_expect_identifier(this, "Expected struct name");
    parser_expect_symbol(this, '{', NULL);
    nodeptrs fields = { 0 };
    while (!lexer_accept_symbol(&this->lexer, '}')) {
        token_t field_name_tok = parser_expect_identifier(this, "Expected field name");
        parser_expect_symbol(this, ':', NULL);
        nodeptr type = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected field type");
        dynarr_append(
            &fields,
            parser_add_node(
                this,
                NT_StructField,
                tokenlocation_merge(field_name_tok.location, parser_current_location(this)),
                .variable_declaration = { .name = parser_text(this, field_name_tok), .type = type }));
        if (!lexer_accept_symbol(&this->lexer, ',') && !token_matches_symbol(lexer_peek(&this->lexer), '}')) {
            parser_error(this, parser_current_location(this), "Expected `,` or `}`");
            return nullptr;
        }
    }
    return parser_add_node(
        this,
        NT_Struct,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .structure = { .name = parser_text(this, name_tok), .fields = fields });
}

nodeptr parse_var_decl(parser_t *this)
{
    lexer_t *lexer = &this->lexer;
    assert(lexer_has_lookback(lexer, 1)
        && token_matches_symbol(lexer_lookback(lexer, 1), ':')
        && token_matches(lexer_lookback(lexer, 1), TK_Identifier));
    bool            is_const = lexer_has_lookback(lexer, 2) && token_matches_keyword(lexer_lookback(lexer, 2), KW_Const);
    token_t         name = lexer_lookback(lexer, 1);
    token_t         token = lexer_peek(lexer);
    nodeptr         type = nullptr;
    tokenlocation_t location = lexer_lookback(lexer, is_const ? 2 : 1).location;
    tokenlocation_t end_location = token.location;
    if (token_matches(token, TK_Identifier)) {
        type = parser_check_node(this, parser_current_location(this), parse_type(this), "Expected variable type");
        end_location = parser_current_location(this);
    }
    token = lexer_peek(lexer);
    nodeptr initializer = nullptr;
    if (token_matches_symbol(token, '=')) {
        lexer_lex(lexer);
        initializer = parser_check_node(this, parser_current_location(this), parse_expression(this, 0), "Error parsing initialization expression");
        end_location = parser_current_location(this);
    } else if (!type.ok) {
        parser_error(this, token.location, "Expected variable initialization expression");
        return nullptr;
    } else {
        end_location = token.location;
    }
    return parser_add_node(
        this,
        NT_VariableDeclaration,
        tokenlocation_merge(location, end_location),
        .variable_declaration = { .name = parser_text(this, name), .type = type, .initializer = initializer });
}

nodeptr parse_while_statement(parser_t *this)
{
    opt_slice_t label = { 0 };
    if (lexer_has_lookback(&this->lexer, 2)
        && token_matches_symbol(lexer_lookback(&this->lexer, 1), ':')
        && token_matches(lexer_lookback(&this->lexer, 2), TK_Identifier)) {
        label = OPTVAL(slice_t, parser_text(this, lexer_lookback(&this->lexer, 2)));
    }
    token_t kw = lexer_lex(&this->lexer);
    nodeptr condition = parser_check_node(this, parser_current_location(this), parse_expression(this, 0), "Error parsing `while` condition");
    nodeptr stmt = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `while` block");
    return parser_add_node(
        this,
        NT_WhileStatement,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .while_statement = { .condition = condition, .statement = stmt, .label = label });
}

nodeptr parse_yield_statement(parser_t *this)
{
    token_t     kw = lexer_lex(&this->lexer);
    opt_slice_t label = { 0 };
    if (lexer_accept_symbol(&this->lexer, ':')) {
        token_t label_tok = parser_expect_identifier(this, "Expected label name after `:`");
        label = OPTVAL(slice_t, parser_text(this, label_tok));
    }
    nodeptr stmt = parser_check_node(this, parser_current_location(this), parse_statement(this), "Error parsing `while` block");
    return parser_add_node(
        this,
        NT_YieldStatement,
        tokenlocation_merge(kw.location, parser_current_location(this)),
        .yield_statement = { .label = label, .statement = stmt });
}

parser_t parse(slice_t text)
{
    parser_t parser = { 0 };
    printf("parsing\n%.*s\n", (int) text.len, text.items);
    lexer_push_source(&parser.lexer, text, c_scanner);
    printf("%zu tokens\n", parser.lexer.tokens.len);
    nodeptrs block = { 0 };
    token_t  t = parse_statements(&parser, &block, parse_module_level_statement);
    parser.root = parser_add_node(&parser, NT_Module, t.location, .statement_block = { .statements = block });
    return parser;
}
