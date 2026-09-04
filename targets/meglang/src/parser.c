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
        error_at(parser, token.span,
                 "expected integer type or 'bool'");
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

static Expr *parse_expression(Parser *parser, unsigned minimum);

static Expr *parse_primary(Parser *parser)
{
    Token token = parser->current;
    Expr *expr;
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
        Token end;
        advance(parser);
        if (parser->current.kind == TOKEN_LPAREN)
        {
            advance(parser);
            end = expect(parser, TOKEN_RPAREN, "expected ')' after function name");
            expr = new_expr(EXPR_CALL, joined(token.span, end.span));
            expr->as.call.name = token.span;
            return expr;
        }
        expr = new_expr(EXPR_NAME, token.span);
        expr->as.name = token.span;
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
    lexer_init(&parser.lexer, source, diagnostics);
    parser.current = (Token){0};
    parser.previous = (Token){0};
    parser.diagnostics = diagnostics;
    parser.errors = 0;
    program->source = source;
    advance(&parser);
    while (parser.current.kind != TOKEN_EOF)
    {
        Token start = expect(&parser, TOKEN_FN, "expected 'fn'");
        Token name = expect(&parser, TOKEN_IDENTIFIER, "expected function name");
        Token return_type;
        Function *function = allocate(sizeof *function);
        function->name = name.span;
        expect(&parser, TOKEN_LPAREN, "expected '('");
        expect(&parser, TOKEN_RPAREN, "expected ')'");
        expect(&parser, TOKEN_ARROW, "expected '->'");
        return_type = parse_type_name(&parser);
        function->return_type_name = return_type.span;
        function->body = parse_block(&parser);
        function->span = joined(start.span, function->body->span);
        *tail = function;
        tail = &function->next;
    }
    {
        ParseResult result = {program, parser.errors + parser.lexer.errors};
        return result;
    }
}
