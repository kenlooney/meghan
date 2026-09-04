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

#ifndef MEGLANG_AST_H
#define MEGLANG_AST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "source.h"
#include "token.h"

typedef enum ValueType{
    TYPE_ERROR,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_BOOL
} ValueType;

typedef struct Symbol Symbol;
typedef struct Expr Expr;
typedef struct Statement Statement;

typedef enum ExprKind {
    EXPR_INT,
    EXPR_BOOL,
    EXPR_NAME,
    EXPR_UNARY,
    EXPR_BINARY
} ExprKind;

struct Expr {
    ExprKind kind;
    SourceSpan span;
    ValueType type;
    const Symbol *symbol;
    union {
        uint64_t integer;
        bool boolean;
        SourceSpan name;
        struct {TokenKind op; Expr *operand;} unary;
        struct {TokenKind op; Expr *left; Expr *right;} binary;
    } as;
};

typedef enum StatementKind {
    STMT_LET,
    STMT_RETURN,
    STMT_EXPR,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR
} StatementKind;

struct Statement {
    StatementKind kind;
    SourceSpan span;
    Statement *next;
    union {
        struct {SourceSpan name; SourceSpan type_name; ValueType type; const Symbol *symbol; Expr *value; } let;
        Expr *expression;
        struct { Statement *items; } block;
        struct { Expr *condition; Statement *then_branch; Statement *else_branch; } branch;
        struct { Expr *condition; Statement *body; } loop;
        struct {
            Statement *initializer;
            Expr *condition;
            Expr *step;
            Statement *body;
        } iteration;
    } as;
};

typedef struct Function {
    SourceSpan span;
    SourceSpan name;
    SourceSpan return_type_name;
    ValueType return_type;
    Statement *body;
} Function;

typedef struct Program {
    const Source *source;
    Function function;
} Program;

void program_destroy(Program *program);
bool ast_print(FILE *out, const Program *program);

#endif // MEGLANG_AST_H
