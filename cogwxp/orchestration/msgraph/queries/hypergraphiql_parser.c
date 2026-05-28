/**
 * @file hypergraphiql_parser.c
 * @brief HyperGraphiQL Query Parser Implementation
 * 
 * Parses HyperGraphiQL queries (GraphQL extended with hypergraph operations)
 * into an AST for execution.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#define _HGQL_INTERNAL
#include "hypergraphiql.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Lexer Types
 *===========================================================================*/

typedef enum {
    TOK_EOF = 0,
    TOK_NAME,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_BANG,           /* ! */
    TOK_DOLLAR,         /* $ */
    TOK_AMP,            /* & */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_SPREAD,         /* ... */
    TOK_COLON,          /* : */
    TOK_EQUALS,         /* = */
    TOK_AT,             /* @ */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_LBRACE,         /* { */
    TOK_PIPE,           /* | */
    TOK_RBRACE,         /* } */
    TOK_ERROR
} token_type_t;

typedef struct {
    token_type_t type;
    const char* start;
    size_t length;
    int line;
    int column;
    
    /* Value for literals */
    union {
        int64_t int_value;
        double float_value;
        char* string_value;
    } value;
} token_t;

typedef struct {
    const char* source;
    const char* current;
    int line;
    int column;
    
    token_t current_token;
    token_t peek_token;
    bool has_peek;
    
    char* error_message;
} lexer_t;

/*===========================================================================
 * AST Node Types
 *===========================================================================*/

typedef enum {
    AST_DOCUMENT,
    AST_OPERATION,
    AST_SELECTION_SET,
    AST_FIELD,
    AST_ARGUMENT,
    AST_FRAGMENT_SPREAD,
    AST_INLINE_FRAGMENT,
    AST_FRAGMENT_DEF,
    AST_VARIABLE_DEF,
    AST_DIRECTIVE,
    AST_NAMED_TYPE,
    AST_LIST_TYPE,
    AST_NON_NULL_TYPE,
    AST_VALUE_INT,
    AST_VALUE_FLOAT,
    AST_VALUE_STRING,
    AST_VALUE_BOOLEAN,
    AST_VALUE_NULL,
    AST_VALUE_ENUM,
    AST_VALUE_LIST,
    AST_VALUE_OBJECT,
    AST_VALUE_VARIABLE
} ast_node_type_t;

typedef struct ast_node {
    ast_node_type_t type;
    int line;
    int column;
    
    /* Children */
    struct ast_node** children;
    size_t child_count;
    size_t child_capacity;
    
    /* Type-specific data */
    union {
        struct {
            char* name;
            hgql_operation_type_t op_type;
        } operation;
        struct {
            char* alias;
            char* name;
        } field;
        struct {
            char* name;
        } argument;
        struct {
            char* name;
        } fragment_spread;
        struct {
            char* type_condition;
        } inline_fragment;
        struct {
            char* name;
            char* type_condition;
        } fragment_def;
        struct {
            char* name;
            char* default_value;
        } variable_def;
        struct {
            hgql_directive_type_t dir_type;
            char* name;
        } directive;
        struct {
            char* name;
        } named_type;
        struct {
            int64_t value;
        } int_value;
        struct {
            double value;
        } float_value;
        struct {
            char* value;
        } string_value;
        struct {
            bool value;
        } bool_value;
        struct {
            char* name;
        } enum_value;
        struct {
            char* name;
        } variable;
    } data;
} ast_node_t;

/*===========================================================================
 * Document Structure
 *===========================================================================*/

/* hgql_document struct is defined in hypergraphiql.h */

/*===========================================================================
 * Lexer Implementation
 *===========================================================================*/

static void lexer_init(lexer_t* lexer, const char* source) {
    lexer->source = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->has_peek = false;
    lexer->error_message = NULL;
}

static void lexer_destroy(lexer_t* lexer) {
    if (lexer->error_message) free(lexer->error_message);
}

static char lexer_peek_char(lexer_t* lexer) {
    return *lexer->current;
}

static char lexer_advance(lexer_t* lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void skip_whitespace(lexer_t* lexer) {
    while (1) {
        char c = lexer_peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',') {
            lexer_advance(lexer);
        } else if (c == '#') {
            /* Comment - skip to end of line */
            while (lexer_peek_char(lexer) != '\n' && lexer_peek_char(lexer) != '\0') {
                lexer_advance(lexer);
            }
        } else {
            break;
        }
    }
}

static token_t scan_name(lexer_t* lexer) {
    token_t token;
    token.type = TOK_NAME;
    token.start = lexer->current;
    token.line = lexer->line;
    token.column = lexer->column;
    
    while (isalnum(lexer_peek_char(lexer)) || lexer_peek_char(lexer) == '_') {
        lexer_advance(lexer);
    }
    
    token.length = lexer->current - token.start;
    return token;
}

static token_t scan_number(lexer_t* lexer) {
    token_t token;
    token.start = lexer->current;
    token.line = lexer->line;
    token.column = lexer->column;
    
    bool is_float = false;
    
    /* Optional negative sign */
    if (lexer_peek_char(lexer) == '-') {
        lexer_advance(lexer);
    }
    
    /* Integer part */
    while (isdigit(lexer_peek_char(lexer))) {
        lexer_advance(lexer);
    }
    
    /* Fractional part */
    if (lexer_peek_char(lexer) == '.') {
        is_float = true;
        lexer_advance(lexer);
        while (isdigit(lexer_peek_char(lexer))) {
            lexer_advance(lexer);
        }
    }
    
    /* Exponent part */
    if (lexer_peek_char(lexer) == 'e' || lexer_peek_char(lexer) == 'E') {
        is_float = true;
        lexer_advance(lexer);
        if (lexer_peek_char(lexer) == '+' || lexer_peek_char(lexer) == '-') {
            lexer_advance(lexer);
        }
        while (isdigit(lexer_peek_char(lexer))) {
            lexer_advance(lexer);
        }
    }
    
    token.length = lexer->current - token.start;
    token.type = is_float ? TOK_FLOAT : TOK_INT;
    
    /* Parse value */
    char* temp = strndup(token.start, token.length);
    if (is_float) {
        token.value.float_value = strtod(temp, NULL);
    } else {
        token.value.int_value = strtoll(temp, NULL, 10);
    }
    free(temp);
    
    return token;
}

static token_t scan_string(lexer_t* lexer) {
    token_t token;
    token.type = TOK_STRING;
    token.line = lexer->line;
    token.column = lexer->column;
    
    lexer_advance(lexer); /* Skip opening quote */
    token.start = lexer->current;
    
    /* Check for block string """...""" */
    bool is_block = false;
    if (lexer_peek_char(lexer) == '"' && lexer->current[1] == '"') {
        is_block = true;
        lexer_advance(lexer);
        lexer_advance(lexer);
        token.start = lexer->current;
    }
    
    size_t value_capacity = 256;
    size_t value_length = 0;
    char* value = malloc(value_capacity);
    
    while (1) {
        char c = lexer_peek_char(lexer);
        
        if (is_block) {
            if (c == '"' && lexer->current[1] == '"' && lexer->current[2] == '"') {
                lexer_advance(lexer);
                lexer_advance(lexer);
                lexer_advance(lexer);
                break;
            }
        } else {
            if (c == '"') {
                lexer_advance(lexer);
                break;
            }
        }
        
        if (c == '\0') {
            token.type = TOK_ERROR;
            free(value);
            return token;
        }
        
        /* Handle escape sequences */
        if (c == '\\' && !is_block) {
            lexer_advance(lexer);
            c = lexer_peek_char(lexer);
            switch (c) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                /* Unicode escape would go here */
            }
        }
        
        /* Grow buffer if needed */
        if (value_length >= value_capacity - 1) {
            value_capacity *= 2;
            value = realloc(value, value_capacity);
        }
        
        value[value_length++] = c;
        lexer_advance(lexer);
    }
    
    value[value_length] = '\0';
    token.value.string_value = value;
    token.length = lexer->current - token.start;
    
    return token;
}

static token_t lexer_next_token(lexer_t* lexer) {
    if (lexer->has_peek) {
        lexer->has_peek = false;
        lexer->current_token = lexer->peek_token;
        return lexer->current_token;
    }
    
    skip_whitespace(lexer);
    
    token_t token;
    token.line = lexer->line;
    token.column = lexer->column;
    token.start = lexer->current;
    token.length = 1;
    
    char c = lexer_peek_char(lexer);
    
    if (c == '\0') {
        token.type = TOK_EOF;
        return token;
    }
    
    if (isalpha(c) || c == '_') {
        token = scan_name(lexer);
    } else if (isdigit(c) || (c == '-' && isdigit(lexer->current[1]))) {
        token = scan_number(lexer);
    } else if (c == '"') {
        token = scan_string(lexer);
    } else {
        lexer_advance(lexer);
        switch (c) {
            case '!': token.type = TOK_BANG; break;
            case '$': token.type = TOK_DOLLAR; break;
            case '&': token.type = TOK_AMP; break;
            case '(': token.type = TOK_LPAREN; break;
            case ')': token.type = TOK_RPAREN; break;
            case ':': token.type = TOK_COLON; break;
            case '=': token.type = TOK_EQUALS; break;
            case '@': token.type = TOK_AT; break;
            case '[': token.type = TOK_LBRACKET; break;
            case ']': token.type = TOK_RBRACKET; break;
            case '{': token.type = TOK_LBRACE; break;
            case '}': token.type = TOK_RBRACE; break;
            case '|': token.type = TOK_PIPE; break;
            case '.':
                if (lexer->current[0] == '.' && lexer->current[1] == '.') {
                    lexer_advance(lexer);
                    lexer_advance(lexer);
                    token.type = TOK_SPREAD;
                    token.length = 3;
                } else {
                    token.type = TOK_ERROR;
                }
                break;
            default:
                token.type = TOK_ERROR;
                break;
        }
    }
    
    lexer->current_token = token;
    return token;
}

static token_t lexer_peek_token(lexer_t* lexer) {
    if (!lexer->has_peek) {
        token_t current = lexer->current_token;
        lexer->peek_token = lexer_next_token(lexer);
        lexer->current_token = current;
        lexer->has_peek = true;
    }
    return lexer->peek_token;
}

/*===========================================================================
 * AST Node Management
 *===========================================================================*/

static ast_node_t* ast_node_create(ast_node_type_t type, int line, int column) {
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    node->type = type;
    node->line = line;
    node->column = column;
    node->child_capacity = 8;
    node->children = calloc(node->child_capacity, sizeof(ast_node_t*));
    return node;
}

static void ast_node_add_child(ast_node_t* parent, ast_node_t* child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children, parent->child_capacity * sizeof(ast_node_t*));
    }
    parent->children[parent->child_count++] = child;
}

static void ast_node_destroy(ast_node_t* node) {
    if (!node) return;
    
    for (size_t i = 0; i < node->child_count; i++) {
        ast_node_destroy(node->children[i]);
    }
    free(node->children);
    
    /* Free type-specific data */
    switch (node->type) {
        case AST_OPERATION:
            free(node->data.operation.name);
            break;
        case AST_FIELD:
            free(node->data.field.alias);
            free(node->data.field.name);
            break;
        case AST_ARGUMENT:
            free(node->data.argument.name);
            break;
        case AST_VALUE_STRING:
            free(node->data.string_value.value);
            break;
        case AST_VALUE_ENUM:
            free(node->data.enum_value.name);
            break;
        case AST_VALUE_VARIABLE:
            free(node->data.variable.name);
            break;
        case AST_DIRECTIVE:
            free(node->data.directive.name);
            break;
        default:
            break;
    }
    
    free(node);
}

/*===========================================================================
 * Parser Implementation
 *===========================================================================*/

typedef struct {
    lexer_t lexer;
    char* error_message;
    int error_line;
    int error_column;
} parser_t;

static bool parser_match(parser_t* parser, token_type_t type) {
    return parser->lexer.current_token.type == type;
}

static bool parser_check(parser_t* parser, token_type_t type) {
    return lexer_peek_token(&parser->lexer).type == type;
}

static token_t parser_advance(parser_t* parser) {
    return lexer_next_token(&parser->lexer);
}

static bool parser_expect(parser_t* parser, token_type_t type, const char* message) {
    if (parser->lexer.current_token.type == type) {
        parser_advance(parser);
        return true;
    }
    
    parser->error_message = strdup(message);
    parser->error_line = parser->lexer.current_token.line;
    parser->error_column = parser->lexer.current_token.column;
    return false;
}

static bool token_is_name(token_t* token, const char* name) {
    return token->type == TOK_NAME && 
           strncmp(token->start, name, token->length) == 0 &&
           strlen(name) == token->length;
}

/* Forward declarations */
static ast_node_t* parse_selection_set(parser_t* parser);
static ast_node_t* parse_value(parser_t* parser, bool is_const);

static ast_node_t* parse_arguments(parser_t* parser, bool is_const) {
    if (!parser_match(parser, TOK_LPAREN)) return NULL;
    parser_advance(parser);
    
    ast_node_t* args = ast_node_create(AST_ARGUMENT, 
        parser->lexer.current_token.line, 
        parser->lexer.current_token.column);
    
    while (!parser_match(parser, TOK_RPAREN) && !parser_match(parser, TOK_EOF)) {
        ast_node_t* arg = ast_node_create(AST_ARGUMENT,
            parser->lexer.current_token.line,
            parser->lexer.current_token.column);
        
        /* Name */
        if (!parser_match(parser, TOK_NAME)) {
            ast_node_destroy(arg);
            ast_node_destroy(args);
            return NULL;
        }
        arg->data.argument.name = strndup(
            parser->lexer.current_token.start,
            parser->lexer.current_token.length);
        parser_advance(parser);
        
        /* Colon */
        if (!parser_expect(parser, TOK_COLON, "Expected ':'")) {
            ast_node_destroy(arg);
            ast_node_destroy(args);
            return NULL;
        }
        
        /* Value */
        ast_node_t* value = parse_value(parser, is_const);
        if (value) {
            ast_node_add_child(arg, value);
        }
        
        ast_node_add_child(args, arg);
    }
    
    parser_expect(parser, TOK_RPAREN, "Expected ')'");
    return args;
}

static ast_node_t* parse_directives(parser_t* parser, bool is_const) {
    ast_node_t* directives = NULL;
    
    while (parser_match(parser, TOK_AT)) {
        parser_advance(parser);
        
        if (!directives) {
            directives = ast_node_create(AST_DIRECTIVE,
                parser->lexer.current_token.line,
                parser->lexer.current_token.column);
        }
        
        ast_node_t* directive = ast_node_create(AST_DIRECTIVE,
            parser->lexer.current_token.line,
            parser->lexer.current_token.column);
        
        /* Name */
        if (!parser_match(parser, TOK_NAME)) {
            ast_node_destroy(directive);
            ast_node_destroy(directives);
            return NULL;
        }
        directive->data.directive.name = strndup(
            parser->lexer.current_token.start,
            parser->lexer.current_token.length);
        
        /* Map directive name to type */
        if (token_is_name(&parser->lexer.current_token, "atom")) {
            directive->data.directive.dir_type = HGQL_DIR_ATOM;
        } else if (token_is_name(&parser->lexer.current_token, "entity")) {
            directive->data.directive.dir_type = HGQL_DIR_ENTITY;
        } else if (token_is_name(&parser->lexer.current_token, "traverse")) {
            directive->data.directive.dir_type = HGQL_DIR_TRAVERSE;
        } else if (token_is_name(&parser->lexer.current_token, "pattern")) {
            directive->data.directive.dir_type = HGQL_DIR_PATTERN;
        } else if (token_is_name(&parser->lexer.current_token, "attention")) {
            directive->data.directive.dir_type = HGQL_DIR_ATTENTION;
        } else if (token_is_name(&parser->lexer.current_token, "infer")) {
            directive->data.directive.dir_type = HGQL_DIR_INFER;
        } else if (token_is_name(&parser->lexer.current_token, "similarity")) {
            directive->data.directive.dir_type = HGQL_DIR_SIMILARITY;
        }
        
        parser_advance(parser);
        
        /* Arguments */
        ast_node_t* args = parse_arguments(parser, is_const);
        if (args) {
            ast_node_add_child(directive, args);
        }
        
        ast_node_add_child(directives, directive);
    }
    
    return directives;
}

static ast_node_t* parse_value(parser_t* parser, bool is_const) {
    token_t token = parser->lexer.current_token;
    ast_node_t* node = NULL;
    
    switch (token.type) {
        case TOK_INT:
            node = ast_node_create(AST_VALUE_INT, token.line, token.column);
            node->data.int_value.value = token.value.int_value;
            parser_advance(parser);
            break;
            
        case TOK_FLOAT:
            node = ast_node_create(AST_VALUE_FLOAT, token.line, token.column);
            node->data.float_value.value = token.value.float_value;
            parser_advance(parser);
            break;
            
        case TOK_STRING:
            node = ast_node_create(AST_VALUE_STRING, token.line, token.column);
            node->data.string_value.value = token.value.string_value;
            parser_advance(parser);
            break;
            
        case TOK_NAME:
            if (token_is_name(&token, "true")) {
                node = ast_node_create(AST_VALUE_BOOLEAN, token.line, token.column);
                node->data.bool_value.value = true;
            } else if (token_is_name(&token, "false")) {
                node = ast_node_create(AST_VALUE_BOOLEAN, token.line, token.column);
                node->data.bool_value.value = false;
            } else if (token_is_name(&token, "null")) {
                node = ast_node_create(AST_VALUE_NULL, token.line, token.column);
            } else {
                node = ast_node_create(AST_VALUE_ENUM, token.line, token.column);
                node->data.enum_value.name = strndup(token.start, token.length);
            }
            parser_advance(parser);
            break;
            
        case TOK_DOLLAR:
            if (!is_const) {
                parser_advance(parser);
                token = parser->lexer.current_token;
                node = ast_node_create(AST_VALUE_VARIABLE, token.line, token.column);
                node->data.variable.name = strndup(token.start, token.length);
                parser_advance(parser);
            }
            break;
            
        case TOK_LBRACKET:
            node = ast_node_create(AST_VALUE_LIST, token.line, token.column);
            parser_advance(parser);
            while (!parser_match(parser, TOK_RBRACKET) && !parser_match(parser, TOK_EOF)) {
                ast_node_t* item = parse_value(parser, is_const);
                if (item) ast_node_add_child(node, item);
            }
            parser_expect(parser, TOK_RBRACKET, "Expected ']'");
            break;
            
        case TOK_LBRACE:
            node = ast_node_create(AST_VALUE_OBJECT, token.line, token.column);
            parser_advance(parser);
            while (!parser_match(parser, TOK_RBRACE) && !parser_match(parser, TOK_EOF)) {
                /* Object field: name: value */
                ast_node_t* field = ast_node_create(AST_ARGUMENT, 
                    parser->lexer.current_token.line,
                    parser->lexer.current_token.column);
                field->data.argument.name = strndup(
                    parser->lexer.current_token.start,
                    parser->lexer.current_token.length);
                parser_advance(parser);
                parser_expect(parser, TOK_COLON, "Expected ':'");
                ast_node_t* value = parse_value(parser, is_const);
                if (value) ast_node_add_child(field, value);
                ast_node_add_child(node, field);
            }
            parser_expect(parser, TOK_RBRACE, "Expected '}'");
            break;
            
        default:
            break;
    }
    
    return node;
}

static ast_node_t* parse_field(parser_t* parser) {
    ast_node_t* field = ast_node_create(AST_FIELD,
        parser->lexer.current_token.line,
        parser->lexer.current_token.column);
    
    /* Name or alias */
    char* first_name = strndup(
        parser->lexer.current_token.start,
        parser->lexer.current_token.length);
    parser_advance(parser);
    
    /* Check for alias */
    if (parser_match(parser, TOK_COLON)) {
        parser_advance(parser);
        field->data.field.alias = first_name;
        field->data.field.name = strndup(
            parser->lexer.current_token.start,
            parser->lexer.current_token.length);
        parser_advance(parser);
    } else {
        field->data.field.name = first_name;
    }
    
    /* Arguments */
    ast_node_t* args = parse_arguments(parser, false);
    if (args) ast_node_add_child(field, args);
    
    /* Directives */
    ast_node_t* directives = parse_directives(parser, false);
    if (directives) ast_node_add_child(field, directives);
    
    /* Selection set */
    if (parser_match(parser, TOK_LBRACE)) {
        ast_node_t* selection_set = parse_selection_set(parser);
        if (selection_set) ast_node_add_child(field, selection_set);
    }
    
    return field;
}

static ast_node_t* parse_selection_set(parser_t* parser) {
    if (!parser_match(parser, TOK_LBRACE)) return NULL;
    parser_advance(parser);
    
    ast_node_t* selection_set = ast_node_create(AST_SELECTION_SET,
        parser->lexer.current_token.line,
        parser->lexer.current_token.column);
    
    while (!parser_match(parser, TOK_RBRACE) && !parser_match(parser, TOK_EOF)) {
        if (parser_match(parser, TOK_SPREAD)) {
            parser_advance(parser);
            
            if (parser_match(parser, TOK_NAME) && 
                !token_is_name(&parser->lexer.current_token, "on")) {
                /* Fragment spread */
                ast_node_t* spread = ast_node_create(AST_FRAGMENT_SPREAD,
                    parser->lexer.current_token.line,
                    parser->lexer.current_token.column);
                spread->data.fragment_spread.name = strndup(
                    parser->lexer.current_token.start,
                    parser->lexer.current_token.length);
                parser_advance(parser);
                
                ast_node_t* directives = parse_directives(parser, false);
                if (directives) ast_node_add_child(spread, directives);
                
                ast_node_add_child(selection_set, spread);
            } else {
                /* Inline fragment */
                ast_node_t* inline_frag = ast_node_create(AST_INLINE_FRAGMENT,
                    parser->lexer.current_token.line,
                    parser->lexer.current_token.column);
                
                if (token_is_name(&parser->lexer.current_token, "on")) {
                    parser_advance(parser);
                    inline_frag->data.inline_fragment.type_condition = strndup(
                        parser->lexer.current_token.start,
                        parser->lexer.current_token.length);
                    parser_advance(parser);
                }
                
                ast_node_t* directives = parse_directives(parser, false);
                if (directives) ast_node_add_child(inline_frag, directives);
                
                ast_node_t* inner_selection = parse_selection_set(parser);
                if (inner_selection) ast_node_add_child(inline_frag, inner_selection);
                
                ast_node_add_child(selection_set, inline_frag);
            }
        } else if (parser_match(parser, TOK_NAME)) {
            ast_node_t* field = parse_field(parser);
            if (field) ast_node_add_child(selection_set, field);
        } else {
            parser_advance(parser);
        }
    }
    
    parser_expect(parser, TOK_RBRACE, "Expected '}'");
    return selection_set;
}

static ast_node_t* parse_operation(parser_t* parser) {
    ast_node_t* operation = ast_node_create(AST_OPERATION,
        parser->lexer.current_token.line,
        parser->lexer.current_token.column);
    
    /* Operation type */
    if (token_is_name(&parser->lexer.current_token, "query")) {
        operation->data.operation.op_type = HGQL_OP_QUERY;
        parser_advance(parser);
    } else if (token_is_name(&parser->lexer.current_token, "mutation")) {
        operation->data.operation.op_type = HGQL_OP_MUTATION;
        parser_advance(parser);
    } else if (token_is_name(&parser->lexer.current_token, "subscription")) {
        operation->data.operation.op_type = HGQL_OP_SUBSCRIPTION;
        parser_advance(parser);
    } else {
        /* Anonymous query */
        operation->data.operation.op_type = HGQL_OP_QUERY;
    }
    
    /* Optional name */
    if (parser_match(parser, TOK_NAME)) {
        operation->data.operation.name = strndup(
            parser->lexer.current_token.start,
            parser->lexer.current_token.length);
        parser_advance(parser);
    }
    
    /* Variable definitions */
    if (parser_match(parser, TOK_LPAREN)) {
        parser_advance(parser);
        while (!parser_match(parser, TOK_RPAREN) && !parser_match(parser, TOK_EOF)) {
            ast_node_t* var_def = ast_node_create(AST_VARIABLE_DEF,
                parser->lexer.current_token.line,
                parser->lexer.current_token.column);
            
            parser_expect(parser, TOK_DOLLAR, "Expected '$'");
            var_def->data.variable_def.name = strndup(
                parser->lexer.current_token.start,
                parser->lexer.current_token.length);
            parser_advance(parser);
            parser_expect(parser, TOK_COLON, "Expected ':'");
            
            /* Type (simplified - just capture name) */
            parser_advance(parser);
            
            /* Default value */
            if (parser_match(parser, TOK_EQUALS)) {
                parser_advance(parser);
                ast_node_t* default_val = parse_value(parser, true);
                if (default_val) ast_node_add_child(var_def, default_val);
            }
            
            ast_node_add_child(operation, var_def);
        }
        parser_expect(parser, TOK_RPAREN, "Expected ')'");
    }
    
    /* Directives */
    ast_node_t* directives = parse_directives(parser, false);
    if (directives) ast_node_add_child(operation, directives);
    
    /* Selection set */
    ast_node_t* selection_set = parse_selection_set(parser);
    if (selection_set) ast_node_add_child(operation, selection_set);
    
    return operation;
}

static ast_node_t* parse_document(parser_t* parser) {
    ast_node_t* document = ast_node_create(AST_DOCUMENT, 1, 1);
    
    parser_advance(parser);
    
    while (!parser_match(parser, TOK_EOF)) {
        if (token_is_name(&parser->lexer.current_token, "query") ||
            token_is_name(&parser->lexer.current_token, "mutation") ||
            token_is_name(&parser->lexer.current_token, "subscription") ||
            parser_match(parser, TOK_LBRACE)) {
            ast_node_t* operation = parse_operation(parser);
            if (operation) ast_node_add_child(document, operation);
        } else if (token_is_name(&parser->lexer.current_token, "fragment")) {
            /* Fragment definition - skip for now */
            parser_advance(parser);
        } else {
            parser_advance(parser);
        }
    }
    
    return document;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

COGUTIL_API cog_result_t hgql_parse(
    const char* query,
    hgql_document_t* document
) {
    if (!query || !document) return COG_ERROR_INVALID_PARAM;
    
    parser_t parser = {0};
    lexer_init(&parser.lexer, query);
    
    ast_node_t* root = parse_document(&parser);
    
    if (parser.error_message) {
        ast_node_destroy(root);
        lexer_destroy(&parser.lexer);
        return COG_ERROR_PARSE;
    }
    
    hgql_document_t doc = calloc(1, sizeof(struct hgql_document));
    doc->root = root;
    doc->source = strdup(query);
    
    /* Extract operations and fragments */
    for (size_t i = 0; i < root->child_count; i++) {
        if (root->children[i]->type == AST_OPERATION) {
            doc->operation_count++;
        } else if (root->children[i]->type == AST_FRAGMENT_DEF) {
            doc->fragment_count++;
        }
    }
    
    doc->operations = calloc(doc->operation_count, sizeof(ast_node_t*));
    doc->fragments = calloc(doc->fragment_count, sizeof(ast_node_t*));
    
    size_t op_idx = 0, frag_idx = 0;
    for (size_t i = 0; i < root->child_count; i++) {
        if (root->children[i]->type == AST_OPERATION) {
            doc->operations[op_idx++] = root->children[i];
        } else if (root->children[i]->type == AST_FRAGMENT_DEF) {
            doc->fragments[frag_idx++] = root->children[i];
        }
    }
    
    lexer_destroy(&parser.lexer);
    *document = doc;
    
    return COG_OK;
}

COGUTIL_API void hgql_document_free(hgql_document_t document) {
    if (!document) return;
    
    ast_node_destroy(document->root);
    free(document->operations);
    free(document->fragments);
    free(document->source);
    free(document);
}
