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

#include "meglang/ast.h"

#include <stdlib.h>

static void expr_destroy(Expr *expr)
{
    if (!expr)
        return;
    if (expr->kind == EXPR_UNARY)
        expr_destroy(expr->as.unary.operand);
    if (expr->kind == EXPR_BINARY)
    {
        expr_destroy(expr->as.binary.left);
        expr_destroy(expr->as.binary.right);
    }
    free(expr);
}

static void statements_destroy(Statement *statement)
{
    while (statement)
    {
        Statement *next = statement->next;
        switch (statement->kind)
        {
        case STMT_LET:
            expr_destroy(statement->as.let.value);
            break;
        case STMT_RETURN:
        case STMT_EXPR:
            expr_destroy(statement->as.expression);
            break;
        case STMT_BLOCK:
            statements_destroy(statement->as.block.items);
            break;
        case STMT_IF:
            expr_destroy(statement->as.branch.condition);
            statements_destroy(statement->as.branch.then_branch);
            statements_destroy(statement->as.branch.else_branch);
            break;
        case STMT_WHILE:
            expr_destroy(statement->as.loop.condition);
            statements_destroy(statement->as.loop.body);
            break;

        // for loop
        case STMT_FOR:
            statements_destroy(statement->as.iteration.initializer);
            expr_destroy(statement->as.iteration.condition);
            expr_destroy(statement->as.iteration.step);
            statements_destroy(statement->as.iteration.body);
            break;

        } // switch

        free(statement);
        statement = next;
    }
}

void program_destroy(Program *program)
{
    if (!program)
        return;
    statements_destroy(program->function.body);
    free(program);
}

static bool put(FILE *out, const char *text) { return fputs(text, out) >= 0; }
static bool indent(FILE *out, unsigned depth)
{
    while (depth--)
        if (!put(out, "  "))
            return false;
    return true;
}

static bool print_expr(FILE *out, const Expr *expr)
{
    if (!expr)
        return put(out, "<error>");
    switch (expr->kind)
    {
    case EXPR_INT:
        return fprintf(out, "%llu", (unsigned long long)expr->as.integer) >= 0;
    case EXPR_BOOL:
        return put(out, expr->as.boolean ? "true" : "false");
    case EXPR_NAME:
        return span_write(out, expr->as.name);
    case EXPR_UNARY:
        return put(out, "(") && put(out, token_name(expr->as.unary.op)) &&
               print_expr(out, expr->as.unary.operand) && put(out, ")");
    case EXPR_BINARY:
        return put(out, "(") && print_expr(out, expr->as.binary.left) &&
               put(out, " ") && put(out, token_name(expr->as.binary.op)) &&
               put(out, " ") && print_expr(out, expr->as.binary.right) && put(out, ")");
    }
    return false;
}

static bool print_statements(FILE *out, const Statement *statement, unsigned depth)
{
    for (; statement; statement = statement->next)
    {
        if (!indent(out, depth))
            return false;
        switch (statement->kind)
        {
        case STMT_LET:
            if (!put(out, "let ") || !span_write(out, statement->as.let.name) ||
                !put(out, ": ") || !span_write(out, statement->as.let.type_name) ||
                !put(out, " = ") || !print_expr(out, statement->as.let.value) ||
                !put(out, "\n"))
                return false;
            break;
        case STMT_RETURN:
            if (!put(out, "return ") || !print_expr(out, statement->as.expression) || !put(out, "\n"))
                return false;
            break;
        case STMT_EXPR:
            if (!print_expr(out, statement->as.expression) || !put(out, "\n"))
                return false;
            break;
        case STMT_BLOCK:
            if (!put(out, "block\n") || !print_statements(out, statement->as.block.items, depth + 1))
                return false;
            break;
        case STMT_IF:
            if (!put(out, "if ") || !print_expr(out, statement->as.branch.condition) || !put(out, "\n") ||
                !print_statements(out, statement->as.branch.then_branch, depth + 1))
                return false;
            if (statement->as.branch.else_branch &&
                (!indent(out, depth) || !put(out, "else\n") ||
                 !print_statements(out, statement->as.branch.else_branch, depth + 1)))
                return false;
            break;
        case STMT_WHILE:
            if (!put(out, "while ") || !print_expr(out, statement->as.loop.condition) || !put(out, "\n") ||
                !print_statements(out, statement->as.loop.body, depth + 1))
                return false;
            break;
        case STMT_FOR:
            if (!put(out, "for\n") ||
                !print_statements(out, statement->as.iteration.initializer, depth + 1) ||
                !indent(out, depth + 1) || !put(out, "condition ") ||
                !print_expr(out, statement->as.iteration.condition) || !put(out, "\n") ||
                !indent(out, depth + 1) || !put(out, "step ") ||
                !print_expr(out, statement->as.iteration.step) || !put(out, "\n") ||
                !print_statements(out, statement->as.iteration.body, depth + 1))
                return false;
            break;
        }
    }
    return true;
}

bool ast_print(FILE *out, const Program *program)
{
    return out && program && put(out, "function main\n") &&
           print_statements(out, program->function.body, 1);
}
