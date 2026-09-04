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
#include "meglang/lexer.h"
#include <stdbool.h>
#include <string.h>

static const char *const token_names[TOKEN_KIND_COUNT] = {
    [TOKEN_ERROR] = "error", 
    [TOKEN_EOF] = "end of file", 
    [TOKEN_IDENTIFIER] = "identifier", 
    [TOKEN_INTEGER] = "integer", 
    [TOKEN_FN] = "fn", 
    [TOKEN_LET] = "let", 
    [TOKEN_RETURN] = "return", 
    [TOKEN_IF] = "if", 
    [TOKEN_ELSE] = "else", 
    [TOKEN_WHILE] = "while", 
    [TOKEN_FOR] = "for",
    [TOKEN_TRUE] = "true", 
    [TOKEN_FALSE] = "false", 
    [TOKEN_I8] = "i8",
    [TOKEN_I16] = "i16",
    [TOKEN_I32] = "i32",
    [TOKEN_I64] = "i64", 
    [TOKEN_U8] = "u8",
    [TOKEN_U16] = "u16",
    [TOKEN_U32] = "u32",
    [TOKEN_U64] = "u64",
    [TOKEN_BOOL] = "bool", 
    [TOKEN_LPAREN] = "(", 
    [TOKEN_RPAREN] = ")", 
    [TOKEN_LBRACE] = "{", 
    [TOKEN_RBRACE] = "}", 
    [TOKEN_COLON] = ":", 
    [TOKEN_COMMA] = ",",
    [TOKEN_SEMICOLON] = ";", 
    [TOKEN_PLUS] = "+", 
    [TOKEN_MINUS] = "-", 
    [TOKEN_STAR] = "*", 
    [TOKEN_SLASH] = "/", 
    [TOKEN_PERCENT] = "%", 
    [TOKEN_BANG] = "!", 
    [TOKEN_ASSIGN] = "=", 
    [TOKEN_EQUAL] = "==", 
    [TOKEN_NOT_EQUAL] = "!=", 
    [TOKEN_LESS] = "<", 
    [TOKEN_LESS_EQUAL] = "<=", 
    [TOKEN_GREATER] = ">", 
    [TOKEN_GREATER_EQUAL] = ">=", 
    [TOKEN_ARROW] = "->",
    [TOKEN_REF] = "ref",
    [TOKEN_AMPERSAND] = "&",
};

const char *token_name(TokenKind kind)
{
    if ((unsigned)kind >= TOKEN_KIND_COUNT || !token_names[kind])
        return "unknown token";
    return token_names[kind];
}

static char peek(const Lexer *lexer)
{
    if (lexer->current >= lexer->source->length)
        return '\0';
    return lexer->source->text[lexer->current];
}

static char peek_next(const Lexer *lexer)
{
    if (lexer->current + 1 >= lexer->source->length)
        return '\0';
    return lexer->source->text[lexer->current + 1];
}

static char advance(Lexer *lexer)
{
    char c;
    if (lexer->current >= lexer->source->length)
        return '\0';
    c = lexer->source->text[lexer->current++];
    if (c == '\r')
    {
        if (peek(lexer) == '\n')
            ++lexer->current;
        ++lexer->line;
        lexer->column = 1;
        return '\n';
    }
    if (c == '\n')
    {
        ++lexer->line;
        lexer->column = 1;
    }
    else
    {
        ++lexer->column;
    }
    return c;
}

static SourceSpan make_span(const Lexer *lexer, size_t start,
                            unsigned line, unsigned column)
{
    SourceSpan span = {lexer->source, start, lexer->current - start,
                       line, column};
    return span;
}

static Token make_token(const Lexer *lexer, TokenKind kind, size_t start,
                        unsigned line, unsigned column)
{
    Token token = {kind, make_span(lexer, start, line, column)};
    return token;
}

static void report(Lexer *lexer, SourceSpan span, const char *message)
{
    diagnostic_emit(lexer->diagnostics, DIAGNOSTIC_ERROR, span, message);
    ++lexer->errors;
}

static bool identifier_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool identifier_continue(char c)
{
    return identifier_start(c) || (c >= '0' && c <= '9');
}

typedef struct Keyword
{
    const char *text;
    TokenKind kind;
} Keyword;

static TokenKind keyword_kind(SourceSpan span)
{
    static const Keyword keywords[] = {
        {"fn", TOKEN_FN}, 
        {"let", TOKEN_LET}, 
        {"return", TOKEN_RETURN}, 
        {"if", TOKEN_IF}, 
        {"else", TOKEN_ELSE}, 
        {"while", TOKEN_WHILE}, 
        {"for", TOKEN_FOR}, 
        {"true", TOKEN_TRUE}, 
        {"false", TOKEN_FALSE}, 
        {"i8", TOKEN_I8},
        {"i16", TOKEN_I16},
        {"i32", TOKEN_I32},
        {"i64", TOKEN_I64}, 
        {"u8", TOKEN_U8},
        {"u16", TOKEN_U16},
        {"u32", TOKEN_U32},
        {"u64", TOKEN_U64},
        {"ref", TOKEN_REF},
        {"bool", TOKEN_BOOL}};
    size_t i;
    for (i = 0; i < sizeof keywords / sizeof keywords[0]; ++i)
        if (span_equals(span, keywords[i].text))
            return keywords[i].kind;
    return TOKEN_IDENTIFIER;
}

static bool skip_trivia(Lexer *lexer)
{
    for (;;)
    {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            advance(lexer);
        }
        else if (c == '/' && peek_next(lexer) == '/')
        {
            while (peek(lexer) && peek(lexer) != '\n' && peek(lexer) != '\r')
                advance(lexer);
        }
        else if (c == '/' && peek_next(lexer) == '*')
        {
            size_t start = lexer->current;
            unsigned line = lexer->line, column = lexer->column;
            advance(lexer);
            advance(lexer);
            while (peek(lexer) && !(peek(lexer) == '*' && peek_next(lexer) == '/'))
                advance(lexer);
            if (!peek(lexer))
            {
                report(lexer, make_span(lexer, start, line, column),
                       "unterminated block comment");
                return false;
            }
            advance(lexer);
            advance(lexer);
        }
        else
        {
            return true;
        }
    }
}

static int digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static Token scan_number(Lexer *lexer, size_t start,
                         unsigned line, unsigned column)
{
    unsigned base = 10;
    bool saw_digit = false;
    bool separator = false;
    bool invalid = false;
    if (peek(lexer) == '0' && (peek_next(lexer) == 'x' || peek_next(lexer) == 'X'))
    {
        advance(lexer);
        advance(lexer);
        base = 16;
    }
    else if (peek(lexer) == '0' && (peek_next(lexer) == 'b' || peek_next(lexer) == 'B'))
    {
        advance(lexer);
        advance(lexer);
        base = 2;
    }
    while (identifier_continue(peek(lexer)))
    {
        char c = advance(lexer);
        int digit;
        if (c == '_')
        {
            if (!saw_digit || separator)
                invalid = true;
            separator = true;
            continue;
        }
        digit = digit_value(c);
        if (digit < 0 || (unsigned)digit >= base)
            invalid = true;
        saw_digit = true;
        separator = false;
    }
    if (!saw_digit || separator)
        invalid = true;
    if (invalid)
    {
        Token token = make_token(lexer, TOKEN_ERROR, start, line, column);
        report(lexer, token.span, "invalid integer literal");
        return token;
    }
    return make_token(lexer, TOKEN_INTEGER, start, line, column);
}

void lexer_init(Lexer *lexer, const Source *source,
                DiagnosticSink diagnostics)
{
    *lexer = (Lexer){source, diagnostics, 0, 1, 1, 0};
}

Token lexer_next(Lexer *lexer)
{
    size_t start;
    unsigned line, column;
    char c;
    if (!skip_trivia(lexer))
        return make_token(lexer, TOKEN_EOF, lexer->current,
                          lexer->line, lexer->column);
    start = lexer->current;
    line = lexer->line;
    column = lexer->column;
    c = peek(lexer);
    if (!c)
        return make_token(lexer, TOKEN_EOF, start, line, column);
    if (identifier_start(c))
    {
        Token token;
        do
            advance(lexer);
        while (identifier_continue(peek(lexer)));
        token = make_token(lexer, TOKEN_IDENTIFIER, start, line, column);
        token.kind = keyword_kind(token.span);
        return token;
    }
    if (c >= '0' && c <= '9')
        return scan_number(lexer, start, line, column);
    advance(lexer);
#define ONE(character, kind) \
    case character:          \
        return make_token(lexer, kind, start, line, column)
    switch (c)
    {
        ONE('(', TOKEN_LPAREN);
        ONE(')', TOKEN_RPAREN);
        ONE('{', TOKEN_LBRACE);
        ONE('}', TOKEN_RBRACE);
        ONE(':', TOKEN_COLON);
        ONE(',', TOKEN_COMMA);
        ONE(';', TOKEN_SEMICOLON);
        ONE('+', TOKEN_PLUS);
        ONE('*', TOKEN_STAR);
        ONE('/', TOKEN_SLASH);
        ONE('%', TOKEN_PERCENT);
        ONE('&', TOKEN_AMPERSAND);
    case '-':
        if (peek(lexer) == '>')
        {
            advance(lexer);
            return make_token(lexer, TOKEN_ARROW, start, line, column);
        }
        return make_token(lexer, TOKEN_MINUS, start, line, column);
    case '!':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(lexer, TOKEN_NOT_EQUAL, start, line, column);
        }
        return make_token(lexer, TOKEN_BANG, start, line, column);
    case '=':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(lexer, TOKEN_EQUAL, start, line, column);
        }
        return make_token(lexer, TOKEN_ASSIGN, start, line, column);
    case '<':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(lexer, TOKEN_LESS_EQUAL, start, line, column);
        }
        return make_token(lexer, TOKEN_LESS, start, line, column);
    case '>':
        if (peek(lexer) == '=')
        {
            advance(lexer);
            return make_token(lexer, TOKEN_GREATER_EQUAL, start, line, column);
        }
        return make_token(lexer, TOKEN_GREATER, start, line, column);
    default:
    {
        Token token = make_token(lexer, TOKEN_ERROR, start, line, column);
        report(lexer, token.span, "unexpected character");
        return token;
    }
    }
#undef ONE
}
