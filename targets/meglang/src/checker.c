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

#include <meglang/checker.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Scope { Scope *parent; Symbol *symbols; };

const char *value_type_name(ValueType type)
{
    switch (type) {
    case TYPE_I64: return "i64";
    case TYPE_BOOL: return "bool";
    default: return "<error>";
    }
}

static void report(Checker *checker, SourceSpan span, const char *message)
{
    diagnostic_emit(checker->diagnostics, DIAGNOSTIC_ERROR, span, message);
    ++checker->errors;
}

static bool same_name(SourceSpan left, SourceSpan right)
{
    return span_valid(left) && span_valid(right) && left.length == right.length &&
           memcmp(left.source->text + left.start,
                  right.source->text + right.start, left.length) == 0;
}

static Symbol *find(Checker *checker, SourceSpan name)
{
    Scope *scope;
    for (scope = checker->scope; scope; scope = scope->parent) {
        Symbol *symbol;
        for (symbol = scope->symbols; symbol; symbol = symbol->next_in_scope)
            if (same_name(symbol->name, name)) return symbol;
    }
    return NULL;
}

static Symbol *define(Checker *checker, SourceSpan name, ValueType type)
{
    Symbol *symbol;
    for (symbol = checker->scope->symbols; symbol; symbol = symbol->next_in_scope) {
        if (same_name(symbol->name, name)) {
            report(checker, name, "duplicate variable in this block");
            return symbol;
        }
    }
    symbol = calloc(1, sizeof *symbol);
    if (!symbol) { fputs("meg: out of memory\n", stderr); exit(2); }
    symbol->name = name; symbol->type = type; symbol->id = checker->next_id++;
    symbol->next_in_scope = checker->scope->symbols;
    checker->scope->symbols = symbol;
    symbol->next_allocated = checker->allocated;
    checker->allocated = symbol;
    return symbol;
}

static ValueType check_expr(Checker *checker, Expr *expr, bool allow_assignment)
{
    ValueType left, right;
    Symbol *symbol;
    if (!expr) return TYPE_ERROR;
    switch (expr->kind) {
    case EXPR_INT: return expr->type = TYPE_I64;
    case EXPR_BOOL: return expr->type = TYPE_BOOL;
    case EXPR_NAME:
        symbol = find(checker, expr->as.name);
        if (!symbol) { report(checker, expr->span, "unknown variable"); return expr->type = TYPE_ERROR; }
        expr->symbol = symbol;
        return expr->type = symbol->type;
    case EXPR_UNARY:
        left = check_expr(checker, expr->as.unary.operand, false);
        if (left == TYPE_ERROR) return expr->type = TYPE_ERROR;
        if (expr->as.unary.op == TOKEN_MINUS) {
            if (left != TYPE_I64) { report(checker, expr->span, "unary '-' requires i64"); return expr->type = TYPE_ERROR; }
            return expr->type = TYPE_I64;
        }
        if (left != TYPE_BOOL) { report(checker, expr->span, "unary '!' requires bool"); return expr->type = TYPE_ERROR; }
        return expr->type = TYPE_BOOL;
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_ASSIGN) {
            if (!allow_assignment) {
                report(checker, expr->span, "assignment is only allowed as a statement");
                return expr->type = TYPE_ERROR;
            }
            right = check_expr(checker, expr->as.binary.right, false);
            if (expr->as.binary.left->kind != EXPR_NAME) {
                check_expr(checker, expr->as.binary.left, false);
                report(checker, expr->as.binary.left->span, "assignment target must be a variable");
                return expr->type = TYPE_ERROR;
            }
            left = check_expr(checker, expr->as.binary.left, false);
            if (left == TYPE_ERROR || right == TYPE_ERROR) return expr->type = TYPE_ERROR;
            if (left != right) { report(checker, expr->span, "assignment types do not match"); return expr->type = TYPE_ERROR; }
            return expr->type = left;
        }
        left = check_expr(checker, expr->as.binary.left, false);
        right = check_expr(checker, expr->as.binary.right, false);
        if (left == TYPE_ERROR || right == TYPE_ERROR) return expr->type = TYPE_ERROR;
        switch (expr->as.binary.op) {
        case TOKEN_PLUS: case TOKEN_MINUS: case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT:
            if (left != TYPE_I64 || right != TYPE_I64) { report(checker, expr->span, "arithmetic requires i64 operands"); return expr->type = TYPE_ERROR; }
            return expr->type = TYPE_I64;
        case TOKEN_LESS: case TOKEN_LESS_EQUAL: case TOKEN_GREATER: case TOKEN_GREATER_EQUAL:
            if (left != TYPE_I64 || right != TYPE_I64) { report(checker, expr->span, "comparison requires i64 operands"); return expr->type = TYPE_ERROR; }
            return expr->type = TYPE_BOOL;
        case TOKEN_EQUAL: case TOKEN_NOT_EQUAL:
            if (left != right) { report(checker, expr->span, "equality operands must have the same type"); return expr->type = TYPE_ERROR; }
            return expr->type = TYPE_BOOL;
        default: report(checker, expr->span, "invalid binary operator"); return expr->type = TYPE_ERROR;
        }
    }
    return TYPE_ERROR;
}

static ValueType written_type(SourceSpan span)
{
    if (span_equals(span, "i64")) return TYPE_I64;
    if (span_equals(span, "bool")) return TYPE_BOOL;
    return TYPE_ERROR;
}

static void check_statements(Checker *checker, Statement *statement);
static void check_block(Checker *checker, Statement *block);

static void check_for(Checker *checker, Statement *statement)
{
    Scope loop_scope = {checker->scope, NULL};
    ValueType condition;
    checker->scope = &loop_scope;
    check_statements(checker, statement->as.iteration.initializer);
    condition = check_expr(checker, statement->as.iteration.condition, false);
    if (condition != TYPE_ERROR && condition != TYPE_BOOL)
        report(checker, statement->as.iteration.condition->span,
               "for condition must be bool");
    (void)check_expr(checker, statement->as.iteration.step, true);
    check_block(checker, statement->as.iteration.body);
    checker->scope = loop_scope.parent;
}

static void check_block(Checker *checker, Statement *block)
{
    Scope nested = {checker->scope, NULL};
    checker->scope = &nested;
    check_statements(checker, block->as.block.items);
    checker->scope = nested.parent;
}

static void check_statements(Checker *checker, Statement *statement)
{
    for (; statement; statement = statement->next) {
        ValueType got;
        switch (statement->kind) {
        case STMT_LET:
            statement->as.let.type = written_type(statement->as.let.type_name);
            got = check_expr(checker, statement->as.let.value, false);
            if (got != TYPE_ERROR && got != statement->as.let.type)
                report(checker, statement->span, "initializer type does not match variable type");
            statement->as.let.symbol = define(checker, statement->as.let.name, statement->as.let.type);
            break;
        case STMT_RETURN:
            got = check_expr(checker, statement->as.expression, false);
            if (got != TYPE_ERROR && got != TYPE_I64)
                report(checker, statement->span, "main must return i64");
            break;
        case STMT_EXPR: (void)check_expr(checker, statement->as.expression, true); break;
        case STMT_BLOCK: check_block(checker, statement); break;
        case STMT_FOR: check_for(checker, statement); break;
        case STMT_IF:
            got = check_expr(checker, statement->as.branch.condition, false);
            if (got != TYPE_ERROR && got != TYPE_BOOL) report(checker, statement->as.branch.condition->span, "if condition must be bool");
            check_block(checker, statement->as.branch.then_branch);
            if (statement->as.branch.else_branch) check_block(checker, statement->as.branch.else_branch);
            break;
        case STMT_WHILE:
            got = check_expr(checker, statement->as.loop.condition, false);
            if (got != TYPE_ERROR && got != TYPE_BOOL) report(checker, statement->as.loop.condition->span, "while condition must be bool");
            check_block(checker, statement->as.loop.body);
            break;
        }
    }
}

static bool guarantees_return(const Statement *statement)
{
    for (; statement; statement = statement->next) {
        if (statement->kind == STMT_RETURN) return true;
        if (statement->kind == STMT_BLOCK && guarantees_return(statement->as.block.items)) return true;
        if (statement->kind == STMT_IF && statement->as.branch.else_branch &&
            guarantees_return(statement->as.branch.then_branch) &&
            guarantees_return(statement->as.branch.else_branch)) return true;
    }
    return false;
}

void checker_init(Checker *checker, DiagnosticSink diagnostics)
{
    *checker = (Checker){0};
    checker->diagnostics = diagnostics;
}

bool checker_check(Checker *checker, Program *program)
{
    Scope root = {NULL, NULL};
    checker->scope = &root;
    check_block(checker, program->function.body);
    if (!guarantees_return(program->function.body))
        report(checker, program->function.span, "main may reach the end without returning");
    checker->scope = NULL;
    return checker->errors == 0;
}

void checker_destroy(Checker *checker)
{
    Symbol *symbol = checker->allocated;
    while (symbol) { Symbol *next = symbol->next_allocated; free(symbol); symbol = next; }
    *checker = (Checker){0};
}
