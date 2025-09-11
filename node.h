/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __NODE_H__
#define __NODE_H__

#include "operators.h"
#include "slice.h"

#define KEYWORDS(S)           \
    S(NT_BinaryExpression)    \
    S(NT_BoolConstant)        \
    S(NT_Break)               \
    S(NT_Continue)            \
    S(NT_Defer)               \
    S(NT_Embed)               \
    S(NT_Enum)                \
    S(NT_EnumValue)           \
    S(NT_Error)               \
    S(NT_ForeignFunction)     \
    S(NT_ForStatement)        \
    S(NT_Function)            \
    S(NT_Identifier)          \
    S(NT_IfStatement)         \
    S(NT_Import)              \
    S(NT_Include)             \
    S(NT_Insert)              \
    S(NT_LoopStatement)       \
    S(NT_Module)              \
    S(NT_Null)                \
    S(NT_Number)              \
    S(NT_Parameter)           \
    S(NT_PublicDeclaration)   \
    S(NT_Return)              \
    S(NT_Signature)           \
    S(NT_StatementBlock)      \
    S(NT_Struct)              \
    S(NT_StructField)         \
    S(NT_String)              \
    S(NT_TypeSpecification)   \
    S(NT_UnaryExpression)     \
    S(NT_VariableDeclaration) \
    S(NT_Void)                \
    S(NT_WhileStatement)      \
    S(NT_YieldStatement)

typedef enum _node_type {
#undef S
#define S(KW) KW,
    KEYWORDS(S)
#undef S
} nodetype_t;

typedef opt_size_t nodeptr;
#ifndef __cplusplus
extern nodeptr nullptr;
#else
#warning "Can't compile elrond with C++ yet"
#endif
#define nodeptr_ptr(v) ((nodeptr) { .ok = true, .value = (v) })
typedef DA(nodeptr) nodeptrs;

typedef struct _binary_expression {
    nodeptr    lhs;
    operator_t op;
    nodeptr    rhs;
} binary_expression_t;

typedef struct _enumeration {
    slice_t  name;
    nodeptr  underlying;
    nodeptrs values;
} enumeration_t;

typedef struct _enum_value {
    slice_t label;
    nodeptr value;
    nodeptr payload;
} enum_value_t;

typedef struct _for_statement {
    slice_t     variable;
    nodeptr     range;
    nodeptr     statement;
    opt_slice_t label;
} for_statement_t;

typedef struct _function {
    slice_t name;
    nodeptr signature;
    nodeptr implementation;
} function_t;

typedef struct _if_statement {
    nodeptr condition;
    nodeptr if_branch;
    nodeptr else_branch;
} if_statement_t;

typedef struct _loop_statement {
    nodeptr     statement;
    opt_slice_t label;
} loop_statement_t;

typedef struct _number {
    slice_t      number;
    numbertype_t number_type;
} number_t;

typedef struct _public_declaration {
    slice_t name;
    nodeptr declaration;
} public_declaration_t;

typedef struct _signature {
    slice_t  name;
    nodeptrs parameters;
    nodeptr  return_type;
} signature_t;

typedef struct _statement_block {
    nodeptrs    statements;
    opt_slice_t label;
} statement_block_t;

typedef struct _string {
    slice_t     string;
    quotetype_t quote_type;
} string_t;

typedef struct _structure {
    slice_t  name;
    nodeptrs fields;
} structure_t;

typedef enum {
    TYP_Alias,
    TYP_Array,
    TYP_DynArray,
    TYP_Optional,
    TYP_Reference,
    TYP_Result,
    TYP_Slice,
    TYP_ZeroTerminatedArray,
} type_kind_t;

typedef struct _alias_description {
    slice_t  name;
    nodeptrs arguments;
} alias_description_t;

typedef struct _array_description {
    nodeptr array_of;
    size_t  size;
} array_description_t;

typedef struct _result_description {
    nodeptr success;
    nodeptr error;
} result_description_t;

typedef struct _type_specification {
    type_kind_t kind;
    union {
        alias_description_t  alias_descr;
        array_description_t  array_descr;
        nodeptr              array_of;
        nodeptr              optional_of;
        nodeptr              referencing;
        result_description_t result_descr;
        nodeptr              slice_of;
    };
} type_specification_t;

typedef struct _unary_expression {
    operator_t op;
    nodeptr    operand;
} unary_expression_t;

typedef struct _variable_declaration {
    slice_t name;
    nodeptr type;
    nodeptr initializer;
} variable_declaration_t;

typedef struct _while_statement {
    nodeptr     condition;
    nodeptr     statement;
    opt_slice_t label;
} while_statement_t;

typedef struct _yield_statement {
    opt_slice_t label;
    nodeptr     statement;
} yield_statement_t;

typedef struct _node {
    nodetype_t      node_type;
    tokenlocation_t location;
    union {
        binary_expression_t    binary_expression;
        bool                   bool_constant;
        enumeration_t          enumeration;
        enum_value_t           enum_value;
        for_statement_t        for_statement;
        function_t             function;
        opt_slice_t            label;
        slice_t                identifier;
        if_statement_t         if_statement;
        loop_statement_t       loop_statement;
        number_t               number;
        slice_t                raw_text;
        variable_declaration_t variable_declaration;
        public_declaration_t   public_declaration;
        signature_t            signature;
        nodeptr                statement;
        statement_block_t      statement_block;
        string_t               string;
        structure_t            structure;
        type_specification_t   type_specification;
        unary_expression_t     unary_expression;
        while_statement_t      while_statement;
        yield_statement_t      yield_statement;
    };
} node_t;

char const * node_type_name(nodetype_t type);

typedef DA(node_t) nodes_t;


#endif /* __NODE_H__ */

#if defined(NODE_IMPLEMENTATION) && !defined(NODE_IMPLEMENTED)

char const *node_type_name(nodetype_t type)
{
    switch (type) {
#undef S
#define S(KW) \
    case KW:  \
        return #KW;
        KEYWORDS(S)
#undef S
    default:
        UNREACHABLE();
    }        
}    


#define NODE_IMPLEMENTED
#endif


