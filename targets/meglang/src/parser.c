// Copyright 2026 Kenneth Looney
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <meglang/parser.h>
#include <meglang/lexer.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Parser
{
    Lexer lexer;
    Token current;
    Token previous;
    DiagnosticSink diagnostics;
    unsigned errors;
} Parser;

static void *allocate(size_t size)
{
    void *memory = calloc(1, size);
    if (!memory)
    {
        fputs("meg: out of memory\n", stderr);
        exit(2);
    }
    return memory;
}

static void advance(Parser *parser)
{
    parser->previous = parser->current;
    do
        parser->current = lexer_next(&parser->lexer);
    while (parser->current.kind == TOKEN_ERROR);
}

static void error_at(Parser *parser, SourceSpan span, const char *message)
{
    diagnostic_emit(parser->diagnostics, DIAGNOSTIC_ERROR, span, message);
    ++parser->errors;
}

static Token expect(Parser *parser, TokenKind kind, const char *message)
{
    Token token = parser->current;
    if (token.kind == kind)
        advance(parser);
    else
        error_at(parser, token.span, message);
    return token;
}

static Token parse_type_name(Parser *parser)
{
    Token token = parser->current;
    switch (token.kind)
    {
    case TOKEN_I8:
    case TOKEN_I16:
    case TOKEN_I32:
    case TOKEN_I64:
    case TOKEN_U8:
    case TOKEN_U16:
    case TOKEN_U32:
    case TOKEN_U64:
    case TOKEN_BOOL:
        advance(parser);
        break;
    default:
        if (token.kind == TOKEN_IDENTIFIER &&
            (span_equals(token.span, "string") ||
             span_equals(token.span, "ustring") ||
             span_equals(token.span, "char") ||
             span_equals(token.span, "utf8_char") ||
             span_equals(token.span, "uchar")))
            advance(parser);
        else
            error_at(parser, token.span, "expected type name");
        break;
    }
    return token;
}

static SourceSpan joined(SourceSpan first, SourceSpan last)
{
    SourceSpan result = first;
    size_t end = last.start + last.length;
    if (end >= first.start)
        result.length = end - first.start;
    return result;
}

static Expr *new_expr(ExprKind kind, SourceSpan span)
{
    Expr *expr = allocate(sizeof *expr);
    expr->kind = kind;
    expr->span = span;
    expr->type = (Type){TYPE_ERROR, TYPE_VALUE};
    return expr;
}

static Statement *new_statement(StatementKind kind, SourceSpan span)
{
    Statement *statement = allocate(sizeof *statement);
    statement->kind = kind;
    statement->span = span;
    if (kind == STMT_LET)
        statement->as.let.type = (Type){TYPE_ERROR, TYPE_VALUE};
    return statement;
}

static int digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

static bool integer_value(Token token, uint64_t *out)
{
    const char *text = token.span.source->text + token.span.start;
    size_t i = 0;
    unsigned base = 10;
    uint64_t value = 0;
    if (token.span.length >= 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X'))
    {
        base = 16;
        i = 2;
    }
    else if (token.span.length >= 2 && text[0] == '0' &&
             (text[1] == 'b' || text[1] == 'B'))
    {
        base = 2;
        i = 2;
    }
    for (; i < token.span.length; ++i)
    {
        int digit;
        if (text[i] == '_')
            continue;
        digit = digit_value(text[i]);
        if (value > (UINT64_MAX - (unsigned)digit) / base)
            return false;
        value = value * base + (unsigned)digit;
    }
    *out = value;
    return true;
}

static uint32_t character_value(Token token)
{
    const unsigned char *text =
        (const unsigned char *)token.span.source->text + token.span.start;
    size_t i = token.kind == TOKEN_CHAR ? 1 :
               token.kind == TOKEN_UTF8_CHAR ? 3 : 2;
    unsigned char c = text[i];

    if (c == '\\')
    {
        switch (text[i + 1])
        {
        case '0': return 0;
        case 'a': return '\a';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        default: return text[i + 1];
        }
    }
    if (c < 0x80)
        return c;
    if (c < 0xe0)
        return ((uint32_t)(c & 0x1f) << 6) |
               (uint32_t)(text[i + 1] & 0x3f);
    if (c < 0xf0)
        return ((uint32_t)(c & 0x0f) << 12) |
               ((uint32_t)(text[i + 1] & 0x3f) << 6) |
               (uint32_t)(text[i + 2] & 0x3f);
    return ((uint32_t)(c & 0x07) << 18) |
           ((uint32_t)(text[i + 1] & 0x3f) << 12) |
           ((uint32_t)(text[i + 2] & 0x3f) << 6) |
           (uint32_t)(text[i + 3] & 0x3f);
}

static Expr *parse_expression(Parser *parser, unsigned minimum);

static Expr *parse_primary(Parser *parser)
{
    Token token = parser->current;
    Expr *expr;
    if (token.kind == TOKEN_STRING || token.kind == TOKEN_USTRING)
    {
        expr = new_expr(EXPR_STRING, token.span);
        expr->as.string.encoding = token.kind == TOKEN_STRING ? STRING_UTF8 : STRING_UTF16;
        expr->as.string.literal = token.span;
        advance(parser);
        return expr;
    }
    if (token.kind == TOKEN_CHAR || token.kind == TOKEN_UTF8_CHAR || token.kind == TOKEN_UCHAR)
    {
        expr = new_expr(token.kind == TOKEN_CHAR ? EXPR_CHAR :
                        token.kind == TOKEN_UTF8_CHAR ? EXPR_UTF8_CHAR : EXPR_UCHAR,
                        token.span);
        if (token.kind == TOKEN_CHAR)
        {
            expr->as.character.literal = token.span;
            expr->as.character.value = character_value(token);
        }
        else if (token.kind == TOKEN_UTF8_CHAR)
        {
            expr->as.utf8_character.literal = token.span;
            expr->as.utf8_character.value = character_value(token);
        }
        else
        {
            expr->as.uchar.literal = token.span;
            expr->as.uchar.value = character_value(token);
        }
        advance(parser);
        return expr;
    }
    if (token.kind == TOKEN_INTEGER)
    {
        uint64_t value = 0;
        advance(parser);
        expr = new_expr(EXPR_INT, token.span);
        if (!integer_value(token, &value))
            error_at(parser, token.span, "integer literal is out of range");
        expr->as.integer = value;
        return expr;
    }
    if (token.kind == TOKEN_TRUE || token.kind == TOKEN_FALSE)
    {
        advance(parser);
        expr = new_expr(EXPR_BOOL, token.span);
        expr->as.boolean = token.kind == TOKEN_TRUE;
        return expr;
    }
    if (token.kind == TOKEN_IDENTIFIER)
    {
        Token qualifier = {0};
        Token name = token;
        Token end;
        advance(parser);
        if (parser->current.kind == TOKEN_DOT)
        {
            qualifier = token;
            advance(parser);
            name = expect(parser, TOKEN_IDENTIFIER,
                          "expected function name after module alias");
        }
        if (parser->current.kind == TOKEN_LPAREN)
        {
            Argument *arguments = NULL;
            Argument **tail = &arguments;
            advance(parser);
            while (parser->current.kind != TOKEN_RPAREN &&
                   parser->current.kind != TOKEN_EOF)
            {
                Argument *argument = allocate(sizeof *argument);
                argument->value = parse_expression(parser, 1);
                *tail = argument;
                tail = &argument->next;
                if (parser->current.kind != TOKEN_COMMA)
                    break;
                advance(parser);
                if (parser->current.kind == TOKEN_RPAREN)
                {
                    error_at(parser, parser->current.span,
                             "expected argument after ','");
                    break;
                }
            }
            end = expect(parser, TOKEN_RPAREN, "expected ')' after function name");
            expr = new_expr(EXPR_CALL,
                            joined(span_valid(qualifier.span)
                                       ? qualifier.span : name.span,
                                   end.span));
            expr->as.call.qualifier = qualifier.span;
            expr->as.call.name = name.span;
            expr->as.call.arguments = arguments;
            return expr;
        }
        if (span_valid(qualifier.span))
            error_at(parser, joined(qualifier.span, name.span),
                     "qualified names must be function calls");
        expr = new_expr(EXPR_NAME, name.span);
        expr->as.name = name.span;
        return expr;
    }
    if (token.kind == TOKEN_LPAREN)
    {
        SourceSpan start = token.span;
        advance(parser);
        expr = parse_expression(parser, 1);
        token = expect(parser, TOKEN_RPAREN, "expected ')' after expression");
        expr->span = joined(start, token.span);
        return expr;
    }
    error_at(parser, token.span, "expected expression");
    if (token.kind != TOKEN_EOF)
        advance(parser);
    return new_expr(EXPR_INT, token.span);
}

static Expr *parse_prefix(Parser *parser)
{
    Token token = parser->current;
    if (token.kind == TOKEN_MINUS || token.kind == TOKEN_BANG ||
        token.kind == TOKEN_STAR || token.kind == TOKEN_AMPERSAND ||
        token.kind == TOKEN_REF)
    {
        Expr *expr;
        Expr *operand;
        advance(parser);
        operand = parse_expression(parser, 6);
        expr = new_expr(EXPR_UNARY, joined(token.span, operand->span));
        expr->as.unary.op = token.kind;
        expr->as.unary.operand = operand;
        return expr;
    }
    return parse_primary(parser);
}

static bool infix_power(TokenKind kind, unsigned *left, unsigned *right)
{
    switch (kind)
    {
    case TOKEN_ASSIGN:
        *left = 1;
        *right = 1;
        return true;
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        *left = 2;
        *right = 3;
        return true;
    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
        *left = 3;
        *right = 4;
        return true;
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        *left = 4;
        *right = 5;
        return true;
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        *left = 5;
        *right = 6;
        return true;
    default:
        return false;
    }
}

static Expr *parse_expression(Parser *parser, unsigned minimum)
{
    Expr *left = parse_prefix(parser);
    unsigned left_power, right_power;
    while (infix_power(parser->current.kind, &left_power, &right_power) &&
           left_power >= minimum)
    {
        Token operator = parser->current;
        Expr *right, *combined;
        advance(parser);
        right = parse_expression(parser, right_power);
        combined = new_expr(EXPR_BINARY, joined(left->span, right->span));
        combined->as.binary.op = operator.kind;
        combined->as.binary.left = left;
        combined->as.binary.right = right;
        left = combined;
    }
    return left;
}

static Statement *parse_block(Parser *parser);

static Statement *parse_statement(Parser *parser)
{
    Token start = parser->current;
    Statement *statement;
    if (start.kind == TOKEN_LET)
    {
        Token name, type, end;
        advance(parser);
        statement = new_statement(STMT_LET, start.span);
        name = expect(parser, TOKEN_IDENTIFIER, "expected variable name");
        expect(parser, TOKEN_COLON, "expected ':' after variable name");
        if (parser->current.kind == TOKEN_STAR || parser->current.kind == TOKEN_REF)
        {
            statement->as.let.type_modifier = parser->current.kind;
            advance(parser);
        }
        type = parse_type_name(parser);
        expect(parser, TOKEN_ASSIGN, "expected '=' after variable type");
        statement->as.let.name = name.span;
        statement->as.let.type_name = type.span;
        statement->as.let.value = parse_expression(parser, 1);
        end = expect(parser, TOKEN_SEMICOLON, "expected ';' after declaration");
        statement->span = joined(start.span, end.span);
        return statement;
    }
    if (start.kind == TOKEN_RETURN)
    {
        Token end;
        advance(parser);
        statement = new_statement(STMT_RETURN, start.span);
        statement->as.expression = parse_expression(parser, 1);
        end = expect(parser, TOKEN_SEMICOLON, "expected ';' after return value");
        statement->span = joined(start.span, end.span);
        return statement;
    }
    if (start.kind == TOKEN_IF)
    {
        advance(parser);
        statement = new_statement(STMT_IF, start.span);
        statement->as.branch.condition = parse_expression(parser, 1);
        statement->as.branch.then_branch = parse_block(parser);
        if (parser->current.kind == TOKEN_ELSE)
        {
            advance(parser);
            statement->as.branch.else_branch = parse_block(parser);
        }
        return statement;
    }
    if (start.kind == TOKEN_WHILE)
    {
        advance(parser);
        statement = new_statement(STMT_WHILE, start.span);
        statement->as.loop.condition = parse_expression(parser, 1);
        statement->as.loop.body = parse_block(parser);
        return statement;
    }
    if (start.kind == TOKEN_FOR)
    {
        Token end;
        advance(parser);
        statement = new_statement(STMT_FOR, start.span);
        expect(parser, TOKEN_LPAREN, "expected '(' after 'for'");
        if (parser->current.kind != TOKEN_LET)
            error_at(parser, parser->current.span, "expected 'let' in for initializer");
        statement->as.iteration.initializer = parse_statement(parser);
        statement->as.iteration.condition = parse_expression(parser, 1);
        expect(parser, TOKEN_SEMICOLON, "expected ';' after for condition");
        statement->as.iteration.step = parse_expression(parser, 1);
        expect(parser, TOKEN_RPAREN, "expected ')' after for clauses");
        statement->as.iteration.body = parse_block(parser);
        end = (Token){TOKEN_RBRACE, statement->as.iteration.body->span};
        statement->span = joined(start.span, end.span);
        return statement;
    }

    if (start.kind == TOKEN_LBRACE)
        return parse_block(parser);
    statement = new_statement(STMT_EXPR, start.span);
    statement->as.expression = parse_expression(parser, 1);
    expect(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    return statement;
}

static Statement *parse_block(Parser *parser)
{
    Token start = expect(parser, TOKEN_LBRACE, "expected '{'");
    Statement *block = new_statement(STMT_BLOCK, start.span);
    Statement **tail = &block->as.block.items;
    while (parser->current.kind != TOKEN_RBRACE && parser->current.kind != TOKEN_EOF)
    {
        Token before = parser->current;
        *tail = parse_statement(parser);
        tail = &(*tail)->next;
        if (parser->current.kind == before.kind && parser->current.span.start == before.span.start)
            advance(parser);
    }
    block->span = joined(start.span, expect(parser, TOKEN_RBRACE, "expected '}' after block").span);
    return block;
}

ParseResult parse_source(const Source *source, DiagnosticSink diagnostics)
{
     Parser parser;
    Program *program = allocate(sizeof *program);
    Function **tail = &program->functions;
    Import **import_tail = &program->imports;
    lexer_init(&parser.lexer, source, diagnostics);
    parser.current = (Token){0}; parser.previous = (Token){0};
    parser.diagnostics = diagnostics; parser.errors = 0;
    program->source = source;
    advance(&parser);
    while (parser.current.kind != TOKEN_EOF) {
        if (parser.current.kind == TOKEN_IMPORT) {
            Token start = parser.current;
            Token path, end;
            Import *import = allocate(sizeof *import);
            advance(&parser);
            path = expect(&parser, TOKEN_STRING,
                          "expected quoted source path after 'import'");
            if (parser.current.kind == TOKEN_AS) {
                advance(&parser);
                import->alias = expect(&parser, TOKEN_IDENTIFIER,
                                       "expected alias after 'as'").span;
            }

            end = expect(&parser, TOKEN_SEMICOLON,
                         "expected ';' after import path");
            import->span = joined(start.span, end.span);
            import->path = path.span;
            if (path.kind == TOKEN_STRING && path.span.length >= 2) {
                ++import->path.start;
                import->path.length -= 2;
                ++import->path.column;
                if (import->path.length == 0)
                    error_at(&parser, path.span, "import path must not be empty");
            }
            *import_tail = import;
            import_tail = &import->next;
            continue;
        }
        Token start = expect(&parser, TOKEN_FN, "expected 'fn'");
        Token name = expect(&parser, TOKEN_IDENTIFIER, "expected function name");
        Token return_type;
        Function *function = allocate(sizeof *function);
        Parameter **parameter_tail = &function->parameters;
        function->name = name.span;
        expect(&parser, TOKEN_LPAREN, "expected '('");
        while (parser.current.kind != TOKEN_RPAREN &&
               parser.current.kind != TOKEN_EOF) {
            Token parameter_name = expect(&parser, TOKEN_IDENTIFIER,
                                          "expected parameter name");
            Token parameter_type;
            Parameter *parameter = allocate(sizeof *parameter);
            expect(&parser, TOKEN_COLON, "expected ':' after parameter name");
            parameter->name = parameter_name.span;
            if (parser.current.kind == TOKEN_STAR || parser.current.kind == TOKEN_REF)
            {
                parameter->type_modifier = parser.current.kind;
                advance(&parser);
            }
            parameter_type = parse_type_name(&parser);
            parameter->type_name = parameter_type.span;
            parameter->span = joined(parameter_name.span, parameter_type.span);
            *parameter_tail = parameter;
            parameter_tail = &parameter->next;
            if (parser.current.kind != TOKEN_COMMA) break;
            advance(&parser);
            if (parser.current.kind == TOKEN_RPAREN) {
                error_at(&parser, parser.current.span,
                         "expected parameter after ','");
                break;
            }
        }
        expect(&parser, TOKEN_RPAREN, "expected ')'");
        expect(&parser, TOKEN_ARROW, "expected '->'");
        return_type = parse_type_name(&parser);
        function->return_type_name = return_type.span;
        function->body = parse_block(&parser);
        function->span = joined(start.span, function->body->span);
        *tail = function;
        tail = &function->next;
    }
    { ParseResult result = {program, parser.errors + parser.lexer.errors}; return result; }
}
