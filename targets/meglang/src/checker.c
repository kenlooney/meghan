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
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct Scope
{
    Scope *parent;
    Symbol *symbols;
};

static Type make_type(ValueType value, TypeForm form)
{
    Type type = {value, form};
    return type;
}

static Type value_type(ValueType value) { return make_type(value, TYPE_VALUE); }
static Type error_type(void) { return value_type(TYPE_ERROR); }
static bool is_error(Type type) { return type.value == TYPE_ERROR; }
static bool same_type(Type left, Type right)
{
    return left.value == right.value && left.form == right.form;
}

const char *value_type_name(ValueType type)
{
    switch (type)
    {
    case TYPE_I8:
        return "i8";
    case TYPE_I16:
        return "i16";
    case TYPE_I32:
        return "i32";
    case TYPE_I64:
        return "i64";
    case TYPE_U8:
        return "u8";
    case TYPE_U16:
        return "u16";
    case TYPE_U32:
        return "u32";
    case TYPE_U64:
        return "u64";
    case TYPE_BOOL:
        return "bool";
    case TYPE_STRING:
        return "string";
    case TYPE_USTRING:
        return "ustring";
    case TYPE_CHAR:
        return "char";
    case TYPE_UTF8_CHAR:
        return "utf8_char";
    case TYPE_UCHAR:
        return "uchar";
    default:
        return "<error>";
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
static FunctionSymbol *find_function_in_source(Checker *checker, SourceSpan name,
                                               const Source *source)
{
    FunctionSymbol *symbol;
    for (symbol = checker->functions; symbol; symbol = symbol->next)
        if (symbol->name.source == source && same_name(symbol->name, name))
            return symbol;
    return NULL;
}

static FunctionSymbol *find_function(Checker *checker, SourceSpan name)
{
    return find_function_in_source(checker, name, name.source);
}

static const Import *find_import(Checker *checker, SourceSpan alias)
{
    const Import *import;
    for (import = checker->program->imports; import; import = import->next)
        if (import->span.source == alias.source &&
            same_name(import->alias, alias))
            return import;
    return NULL;
}

static FunctionSymbol *define_function(Checker *checker, Function *function)
{
    FunctionSymbol *symbol = find_function(checker, function->name);
    if (symbol)
    {
        report(checker, function->name, "duplicate function");
        return symbol;
    }
    symbol = calloc(1, sizeof *symbol);
    if (!symbol)
    {
        fputs("meg: out of memory\n", stderr);
        exit(2);
    }
    symbol->name = function->name;
    symbol->return_type = value_type(function->return_type);
    symbol->declaration = function;
    symbol->id = checker->next_function_id++;
    symbol->next = checker->functions;
    checker->functions = symbol;
    return symbol;
}
static Symbol *find(Checker *checker, SourceSpan name)
{
    Scope *scope;
    for (scope = checker->scope; scope; scope = scope->parent)
    {
        Symbol *symbol;
        for (symbol = scope->symbols; symbol; symbol = symbol->next_in_scope)
            if (same_name(symbol->name, name))
                return symbol;
    }
    return NULL;
}

static Symbol *define(Checker *checker, SourceSpan name, Type type)
{
    Symbol *symbol;
    for (symbol = checker->scope->symbols; symbol; symbol = symbol->next_in_scope)
    {
        if (same_name(symbol->name, name))
        {
            report(checker, name, "duplicate variable in this block");
            return symbol;
        }
    }
    symbol = calloc(1, sizeof *symbol);
    if (!symbol)
    {
        fputs("meg: out of memory\n", stderr);
        exit(2);
    }
    symbol->name = name;
    symbol->type = type;
    symbol->id = checker->next_id++;
    symbol->next_in_scope = checker->scope->symbols;
    checker->scope->symbols = symbol;
    symbol->next_allocated = checker->allocated;
    checker->allocated = symbol;
    return symbol;
}

static ValueType written_type(SourceSpan span)
{
    if (span_equals(span, "i8"))
        return TYPE_I8;
    if (span_equals(span, "i16"))
        return TYPE_I16;
    if (span_equals(span, "i32"))
        return TYPE_I32;
    if (span_equals(span, "i64"))
        return TYPE_I64;
    if (span_equals(span, "u8"))
        return TYPE_U8;
    if (span_equals(span, "u16"))
        return TYPE_U16;
    if (span_equals(span, "u32"))
        return TYPE_U32;
    if (span_equals(span, "u64"))
        return TYPE_U64;
    if (span_equals(span, "bool"))
        return TYPE_BOOL;
    if (span_equals(span, "string"))
        return TYPE_STRING;
    if (span_equals(span, "char"))
        return TYPE_CHAR;
    if (span_equals(span, "utf8_char"))
        return TYPE_UTF8_CHAR;
    if (span_equals(span, "uchar"))
        return TYPE_UCHAR;
    return TYPE_ERROR;
}

static bool is_signed_integer_value(ValueType type)
{
    return type == TYPE_I8 || type == TYPE_I16 ||
           type == TYPE_I32 || type == TYPE_I64;
}

static bool is_unsigned_integer_value(ValueType type)
{
    return type == TYPE_U8 || type == TYPE_U16 ||
           type == TYPE_U32 || type == TYPE_U64;
}

static bool is_integer_type(Type type)
{
    return type.form == TYPE_VALUE &&
           (is_signed_integer_value(type.value) || is_unsigned_integer_value(type.value));
}
static bool is_character_type(Type type)
{
    return type.form == TYPE_VALUE &&
           (type.value == TYPE_CHAR || type.value == TYPE_UTF8_CHAR || type.value == TYPE_UCHAR);
}
static bool is_string_type(Type type)
{
    return type.form == TYPE_VALUE && type.value == TYPE_STRING;
}

static bool integer_fits_type(uint64_t value, ValueType type)
{
    switch (type)
    {
    case TYPE_I8:
        return value <= INT8_MAX;
    case TYPE_I16:
        return value <= INT16_MAX;
    case TYPE_I32:
        return value <= INT32_MAX;
    case TYPE_I64:
        return value <= INT64_MAX;
    case TYPE_U8:
        return value <= UINT8_MAX;
    case TYPE_U16:
        return value <= UINT16_MAX;
    case TYPE_U32:
        return value <= UINT32_MAX;
    case TYPE_U64:
        return true;
    default:
        return false;
    }
}

static bool negative_integer_fits_type(uint64_t magnitude, ValueType type)
{
    switch (type)
    {
    case TYPE_I8:
        return magnitude <= (uint64_t)INT8_MAX + 1;
    case TYPE_I16:
        return magnitude <= (uint64_t)INT16_MAX + 1;
    case TYPE_I32:
        return magnitude <= (uint64_t)INT32_MAX + 1;
    case TYPE_I64:
        return magnitude <= (uint64_t)INT64_MAX + 1;
    default:
        return false;
    }
}

static bool is_lvalue(const Expr *expr)
{
    return expr && (expr->kind == EXPR_NAME ||
                    (expr->kind == EXPR_UNARY && expr->as.unary.op == TOKEN_STAR));
}

static bool is_integer_literal_expr(const Expr *expr)
{
    return expr &&
           (expr->kind == EXPR_INT ||
            (expr->kind == EXPR_UNARY &&
             expr->as.unary.op == TOKEN_MINUS &&
             expr->as.unary.operand->kind == EXPR_INT));
}

static Type check_expr(Checker *checker, Expr *expr, Type expected, bool allow_assignment)
{
    Type left, right;
    Symbol *symbol;
    if (!expr)
        return error_type();
    switch (expr->kind)
    {
    case EXPR_CHAR:
        return expr->type = value_type(TYPE_CHAR);
    case EXPR_UTF8_CHAR:
        return expr->type = value_type(TYPE_UTF8_CHAR);
    case EXPR_UCHAR:
        return expr->type = value_type(TYPE_UCHAR);
    case EXPR_STRING:
    return expr->type = value_type(
        expr->as.string.encoding == STRING_UTF16
        ? TYPE_USTRING
        : TYPE_STRING);
    break;
    case EXPR_INT:
    {
        Type type = is_integer_type(expected)
                        ? expected
                        : value_type(expr->as.integer <= INT64_MAX ? TYPE_I64 : TYPE_U64);
        if (!integer_fits_type(expr->as.integer, type.value))
        {
            report(checker, expr->span, "integer literal does not fit in the expected type");
            return expr->type = error_type();
        }
        return expr->type = type;
    }
    case EXPR_BOOL:
        return expr->type = value_type(TYPE_BOOL);
    case EXPR_NAME:
        symbol = find(checker, expr->as.name);
        if (!symbol)
        {
            report(checker, expr->span, "unknown variable");
            return expr->type = error_type();
        }
        expr->symbol = symbol;
        return expr->type = symbol->type;
    case EXPR_CALL:
    {
        FunctionSymbol *function;
        Argument *argument = expr->as.call.arguments;
        const Parameter *parameter;
        if (span_valid(expr->as.call.qualifier))
        {
            const Import *import = find_import(checker, expr->as.call.qualifier);
            if (!import)
            {
                for (; argument; argument = argument->next)
                    (void)check_expr(checker, argument->value, error_type(), false);
                report(checker, expr->as.call.qualifier, "unknown module alias");
                return expr->type = error_type();
            }
            function = find_function_in_source(checker, expr->as.call.name,
                                               import->target);
        }
        else
        {
            function = find_function(checker, expr->as.call.name);
        }
        if (!function)
        {
            for (; argument; argument = argument->next)
                (void)check_expr(checker, argument->value, error_type(), false);
            report(checker, expr->span, "unknown function");
            return expr->type = error_type();
        }
        expr->as.call.symbol = function;
        parameter = function->declaration->parameters;
        while (argument && parameter)
        {
            Type got = check_expr(checker, argument->value, parameter->type, false);
            if (!is_error(got) && !same_type(got, parameter->type))
                report(checker, argument->value->span,
                       "argument type does not match parameter type");
            argument = argument->next;
            parameter = parameter->next;
        }
        if (argument)
        {
            report(checker, expr->span, "too many arguments in function call");
            for (; argument; argument = argument->next)
                (void)check_expr(checker, argument->value, error_type(), false);
        }
        else if (parameter)
        {
            report(checker, expr->span, "too few arguments in function call");
        }
        return expr->type = function->return_type;
    }
    case EXPR_UNARY:
        if (expr->as.unary.op == TOKEN_AMPERSAND || expr->as.unary.op == TOKEN_REF)
        {
            TypeForm form = expr->as.unary.op == TOKEN_AMPERSAND
                                ? TYPE_POINTER
                                : TYPE_REFERENCE;
            if (!is_lvalue(expr->as.unary.operand))
            {
                (void)check_expr(checker, expr->as.unary.operand, error_type(), false);
                report(checker, expr->as.unary.operand->span,
                       "address and reference operators require an assignable value");
                return expr->type = error_type();
            }
            left = check_expr(checker, expr->as.unary.operand, error_type(), false);
            if (is_error(left))
                return expr->type = error_type();
            if (left.form != TYPE_VALUE)
            {
                report(checker, expr->span, "cannot take the address or reference of this value");
                return expr->type = error_type();
            }
            return expr->type = make_type(left.value, form);
        }
        if (expr->as.unary.op == TOKEN_STAR)
        {
            left = check_expr(checker, expr->as.unary.operand, error_type(), false);
            if (is_error(left))
                return expr->type = error_type();
            if (left.form != TYPE_POINTER && left.form != TYPE_REFERENCE)
            {
                report(checker, expr->span, "unary '*' requires a pointer or reference");
                return expr->type = error_type();
            }
            return expr->type = value_type(left.value);
        }
        if (expr->as.unary.op == TOKEN_MINUS)
        {
            if (expr->as.unary.operand->kind == EXPR_INT &&
                expected.form == TYPE_VALUE && is_signed_integer_value(expected.value))
            {
                if (!negative_integer_fits_type(expr->as.unary.operand->as.integer,
                                                expected.value))
                {
                    report(checker, expr->span,
                           "negative integer literal does not fit in the expected type");
                    return expr->type = error_type();
                }
                expr->as.unary.operand->type = expected;
                return expr->type = expected;
            }
            left = check_expr(checker, expr->as.unary.operand, expected, false);
            if (is_error(left))
                return expr->type = error_type();
            if (!is_integer_type(left) || !is_signed_integer_value(left.value))
            {
                report(checker, expr->span, "unary '-' requires a signed integer");
                return expr->type = error_type();
            }
            return expr->type = left;
        }
        left = check_expr(checker, expr->as.unary.operand, value_type(TYPE_BOOL), false);
        if (is_error(left))
            return expr->type = error_type();
        if (!same_type(left, value_type(TYPE_BOOL)))
        {
            report(checker, expr->span, "unary '!' requires bool");
            return expr->type = error_type();
        }
        return expr->type = value_type(TYPE_BOOL);
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_ASSIGN)
        {
            if (!allow_assignment)
            {
                report(checker, expr->span, "assignment is only allowed as a statement");
                return expr->type = error_type();
            }
            if (!is_lvalue(expr->as.binary.left))
            {
                (void)check_expr(checker, expr->as.binary.left, error_type(), false);
                report(checker, expr->as.binary.left->span,
                       "assignment target must be a variable or dereferenced pointer");
                return expr->type = error_type();
            }
            left = check_expr(checker, expr->as.binary.left, error_type(), false);
            right = check_expr(checker, expr->as.binary.right, left, false);
            if (is_error(left) || is_error(right))
                return expr->type = error_type();
            if (!same_type(left, right))
            {
                report(checker, expr->span, "assignment types do not match");
                return expr->type = error_type();
            }
            return expr->type = left;
        }
        switch (expr->as.binary.op)
        {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            if (!is_integer_type(expected) &&
                is_integer_literal_expr(expr->as.binary.left) &&
                !is_integer_literal_expr(expr->as.binary.right))
            {
                right = check_expr(checker, expr->as.binary.right, error_type(), false);
                left = check_expr(checker, expr->as.binary.left, right, false);
            }
            else
            {
                left = check_expr(checker, expr->as.binary.left, expected, false);
                right = check_expr(checker, expr->as.binary.right,
                                   is_integer_type(expected) ? expected : left, false);
            }
            if (is_error(left) || is_error(right))
                return expr->type = error_type();
            if (!is_integer_type(left) || !is_integer_type(right))
            {
                report(checker, expr->span, "arithmetic operators require integer operands");
                return expr->type = error_type();
            }
            if (!same_type(left, right))
            {
                report(checker, expr->span, "arithmetic operands must have the same type");
                return expr->type = error_type();
            }
            return expr->type = left;
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
            if (is_integer_literal_expr(expr->as.binary.left) &&
                !is_integer_literal_expr(expr->as.binary.right))
            {
                right = check_expr(checker, expr->as.binary.right, error_type(), false);
                left = check_expr(checker, expr->as.binary.left, right, false);
            }
            else
            {
                left = check_expr(checker, expr->as.binary.left, error_type(), false);
                right = check_expr(checker, expr->as.binary.right, left, false);
            }
            if (is_error(left) || is_error(right))
                return expr->type = error_type();
            if (!is_integer_type(left) || !same_type(left, right))
            {
                report(checker, expr->span, "comparison requires matching integer operands");
                return expr->type = error_type();
            }
            return expr->type = value_type(TYPE_BOOL);
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
            if (is_integer_literal_expr(expr->as.binary.left) &&
                !is_integer_literal_expr(expr->as.binary.right))
            {
                right = check_expr(checker, expr->as.binary.right, error_type(), false);
                left = check_expr(checker, expr->as.binary.left, right, false);
            }
            else
            {
                left = check_expr(checker, expr->as.binary.left, error_type(), false);
                right = check_expr(checker, expr->as.binary.right, left, false);
            }
            if (is_error(left) || is_error(right))
                return expr->type = error_type();
            if (!same_type(left, right))
            {
                report(checker, expr->span, "equality operands must have the same type");
                return expr->type = error_type();
            }
            return expr->type = value_type(TYPE_BOOL);
        default:
            report(checker, expr->span, "invalid binary operator");
            return expr->type = error_type();
        }
    }
    return error_type();
}

static void check_statements(Checker *checker, Statement *statement);
static void check_block(Checker *checker, Statement *block);

static void check_for(Checker *checker, Statement *statement)
{
    Scope loop_scope = {checker->scope, NULL};
    Type condition;
    checker->scope = &loop_scope;
    check_statements(checker, statement->as.iteration.initializer);
    condition = check_expr(checker, statement->as.iteration.condition,
                           value_type(TYPE_BOOL), false);
    if (!is_error(condition) && !same_type(condition, value_type(TYPE_BOOL)))
        report(checker, statement->as.iteration.condition->span,
               "for condition must be bool");
    (void)check_expr(checker, statement->as.iteration.step, error_type(), true);
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
    for (; statement; statement = statement->next)
    {
        Type got;
        switch (statement->kind)
        {
        case STMT_LET:
        {
            TypeForm form = TYPE_VALUE;
            if (statement->as.let.type_modifier == TOKEN_STAR)
                form = TYPE_POINTER;
            if (statement->as.let.type_modifier == TOKEN_REF)
                form = TYPE_REFERENCE;
            statement->as.let.type = make_type(
                written_type(statement->as.let.type_name), form);
            got = check_expr(checker, statement->as.let.value,
                             statement->as.let.type, false);
            if (!is_error(got) && !same_type(got, statement->as.let.type))
                report(checker, statement->span,
                       "initializer type does not match variable type");
            statement->as.let.symbol = define(checker, statement->as.let.name,
                                              statement->as.let.type);
            break;
        }
        case STMT_RETURN:
            got = check_expr(checker, statement->as.expression,
                             value_type(checker->return_type), false);
            if (!is_error(got) && !same_type(got, value_type(checker->return_type)))
                report(checker, statement->span,
                       "return type does not match function return type");
            break;
        case STMT_EXPR:
            (void)check_expr(checker, statement->as.expression, error_type(), true);
            break;
        case STMT_BLOCK:
            check_block(checker, statement);
            break;
        case STMT_FOR:
            check_for(checker, statement);
            break;
        case STMT_IF:
            got = check_expr(checker, statement->as.branch.condition,
                             value_type(TYPE_BOOL), false);
            if (!is_error(got) && !same_type(got, value_type(TYPE_BOOL)))
                report(checker, statement->as.branch.condition->span,
                       "if condition must be bool");
            check_block(checker, statement->as.branch.then_branch);
            if (statement->as.branch.else_branch)
                check_block(checker, statement->as.branch.else_branch);
            break;
        case STMT_WHILE:
            got = check_expr(checker, statement->as.loop.condition,
                             value_type(TYPE_BOOL), false);
            if (!is_error(got) && !same_type(got, value_type(TYPE_BOOL)))
                report(checker, statement->as.loop.condition->span,
                       "while condition must be bool");
            check_block(checker, statement->as.loop.body);
            break;
        }
    }
}

static bool guarantees_return(const Statement *statement)
{
    for (; statement; statement = statement->next)
    {
        if (statement->kind == STMT_RETURN)
            return true;
        if (statement->kind == STMT_BLOCK &&
            guarantees_return(statement->as.block.items))
            return true;
        if (statement->kind == STMT_IF && statement->as.branch.else_branch &&
            guarantees_return(statement->as.branch.then_branch) &&
            guarantees_return(statement->as.branch.else_branch))
            return true;
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
    const Import *import;
    Function *function;
    FunctionSymbol *main_symbol = NULL;
    SourceSpan program_span = {program->source, 0, 0, 1, 1};
    checker->program = program;
    for (import = program->imports; import; import = import->next)
    {
        const Import *previous;
        if (!span_valid(import->alias))
            continue;
        for (previous = program->imports; previous != import;
             previous = previous->next)
        {
            if (previous->span.source == import->span.source &&
                same_name(previous->alias, import->alias))
            {
                report(checker, import->alias, "duplicate module alias");
                break;
            }
        }
    }
    for (function = program->functions; function; function = function->next)
    {
        Parameter *parameter;
        function->return_type = written_type(function->return_type_name);
        for (parameter = function->parameters; parameter; parameter = parameter->next)
        {
            TypeForm form = TYPE_VALUE;
            if (parameter->type_modifier == TOKEN_STAR)
                form = TYPE_POINTER;
            if (parameter->type_modifier == TOKEN_REF)
                form = TYPE_REFERENCE;
            parameter->type = make_type(written_type(parameter->type_name), form);
        }
        function->symbol = define_function(checker, function);
        if (span_equals(function->name, "main"))
        {
            if (function->name.source != program->source)
                report(checker, function->name,
                       "main must be declared in the entry file");
            else
            {
                if (main_symbol)
                    report(checker, function->name, "duplicate main function");
                else
                    main_symbol = (FunctionSymbol *)function->symbol;
                if (function->parameters)
                    report(checker, function->parameters->span,
                           "main must not declare parameters");
            }
        }
    }
    if (!main_symbol)
        report(checker, program_span, "program must define main");
    for (function = program->functions; function; function = function->next)
    {
        Scope function_scope = {NULL, NULL};
        Parameter *parameter;
        checker->return_type = function->return_type;
        checker->scope = &function_scope;
        for (parameter = function->parameters; parameter; parameter = parameter->next)
            parameter->symbol = define(checker, parameter->name, parameter->type);
        check_statements(checker, function->body->as.block.items);
        if (!guarantees_return(function->body))
            report(checker, function->span,
                   "function may reach the end without returning");
        checker->scope = NULL;
    }
    checker->scope = NULL;
    checker->program = NULL;
    checker->return_type = TYPE_ERROR;
    return checker->errors == 0;
}

void checker_destroy(Checker *checker)
{
     Symbol *symbol = checker->allocated;
    FunctionSymbol *function = checker->functions;
    while (symbol) { Symbol *next = symbol->next_allocated; free(symbol); symbol = next; }
    while (function) { FunctionSymbol *next = function->next; free(function); function = next; }
    *checker = (Checker){0};
}
