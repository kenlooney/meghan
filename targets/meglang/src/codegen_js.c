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

#include <meglang/codegen_js.h>

#include <meglang/checker.h>

#include <stdarg.h>

typedef struct JsEmitter {
    FILE *out;
    DiagnosticSink diagnostics;
    ValueType return_type;
    bool failed;
} JsEmitter;

static bool emit(JsEmitter *emitter, const char *format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfprintf(emitter->out, format, arguments);
    va_end(arguments);
    if (result < 0) emitter->failed = true;
    return result >= 0;
}

static bool emit_indent(JsEmitter *emitter, unsigned depth)
{
    while (depth--) if (!emit(emitter, "    ")) return false;
    return true;
}

static const char *js_operator(TokenKind kind)
{
    if (kind == TOKEN_EQUAL) return "===";
    if (kind == TOKEN_NOT_EQUAL) return "!==";
    return token_name(kind);
}

static bool is_unsigned_integer_type(ValueType type)
{
    return type == TYPE_U8 || type == TYPE_U16 ||
           type == TYPE_U32 || type == TYPE_U64;
}

static bool is_signed_integer_type(ValueType type)
{
    return type == TYPE_I8 || type == TYPE_I16 ||
           type == TYPE_I32 || type == TYPE_I64;
}

static unsigned integer_bits(ValueType type)
{
    switch (type) {
    case TYPE_I8: case TYPE_U8: return 8;
    case TYPE_I16: case TYPE_U16: return 16;
    case TYPE_I32: case TYPE_U32: return 32;
    case TYPE_I64: case TYPE_U64: return 64;
    default: return 0;
    }
}

static bool emit_expr(JsEmitter *emitter, const Expr *expr);

static bool emit_function_name(JsEmitter *emitter, const FunctionSymbol *function)
{
    if (span_equals(function->name, "main"))
        return emit(emitter, "meg_main");
    return emit(emitter, "meg_f_%u", function->id);
}

static bool emit_arguments(JsEmitter *emitter, const Argument *argument)
{
    bool first = true;
    for (; argument; argument = argument->next) {
        if ((!first && !emit(emitter, ", ")) ||
            !emit_expr(emitter, argument->value)) return false;
        first = false;
    }
    return true;
}

static bool emit_parameters(JsEmitter *emitter, const Parameter *parameter)
{
    bool first = true;
    for (; parameter; parameter = parameter->next) {
        if ((!first && !emit(emitter, ", ")) ||
            !emit(emitter, "meg_v_%u", parameter->symbol->id)) return false;
        first = false;
    }
    return true;
}

static bool emit_integer_conversion_start(JsEmitter *emitter, Type type)
{
    if (type.form != TYPE_VALUE) return true;
    if (is_signed_integer_type(type.value))
        return emit(emitter, "meg_signed(%u, ", integer_bits(type.value));
    if (is_unsigned_integer_type(type.value))
        return emit(emitter, "meg_unsigned(%u, ", integer_bits(type.value));
    return true;
}

static bool emit_integer_conversion_end(JsEmitter *emitter, Type type)
{
    if (type.form == TYPE_VALUE &&
        (is_signed_integer_type(type.value) || is_unsigned_integer_type(type.value)))
        return emit(emitter, ")");
    return true;
}

static bool emit_typed_expr(JsEmitter *emitter, const Expr *expr, Type type)
{
    return emit_integer_conversion_start(emitter, type) &&
           emit_expr(emitter, expr) &&
           emit_integer_conversion_end(emitter, type);
}

static bool emit_reference(JsEmitter *emitter, const Expr *operand)
{
    if (operand->kind == EXPR_UNARY && operand->as.unary.op == TOKEN_STAR)
        return emit_expr(emitter, operand->as.unary.operand);
    return emit(emitter, "({ get: () => ") && emit_expr(emitter, operand) &&
           emit(emitter, ", set: meg_value => { ") && emit_expr(emitter, operand) &&
           emit(emitter, " = ") &&
           emit_integer_conversion_start(emitter, operand->type) &&
           emit(emitter, "meg_value") &&
           emit_integer_conversion_end(emitter, operand->type) &&
           emit(emitter, "; } })");
}

static bool emit_expr(JsEmitter *emitter, const Expr *expr)
{
    switch (expr->kind) {
    case EXPR_INT: return emit(emitter, "%llun", (unsigned long long)expr->as.integer);
    case EXPR_BOOL: return emit(emitter, "%s", expr->as.boolean ? "true" : "false");
    case EXPR_CHAR: return emit(emitter, "%u", (unsigned)expr->as.character.value);
    case EXPR_UTF8_CHAR: return emit(emitter, "%u", (unsigned)expr->as.utf8_character.value);
    case EXPR_UCHAR: return emit(emitter, "%u", (unsigned)expr->as.uchar.value);
    case EXPR_NAME: return emit(emitter, "meg_v_%u", expr->symbol->id);
    case EXPR_CALL:
        return emit_function_name(emitter, expr->as.call.symbol) &&
               emit(emitter, "(") &&
               emit_arguments(emitter, expr->as.call.arguments) &&
               emit(emitter, ")");
    case EXPR_UNARY:
        if (expr->as.unary.op == TOKEN_AMPERSAND || expr->as.unary.op == TOKEN_REF)
            return emit_reference(emitter, expr->as.unary.operand);
        if (expr->as.unary.op == TOKEN_STAR)
            return emit(emitter, "(") && emit_expr(emitter, expr->as.unary.operand) &&
                   emit(emitter, ").get()");
        if (expr->as.unary.op == TOKEN_MINUS &&
            is_signed_integer_type(expr->type.value))
            return emit_integer_conversion_start(emitter, expr->type) &&
                   emit(emitter, "(-") && emit_expr(emitter, expr->as.unary.operand) &&
                   emit(emitter, ")") && emit_integer_conversion_end(emitter, expr->type);
        return emit(emitter, "(%s", token_name(expr->as.unary.op)) &&
               emit_expr(emitter, expr->as.unary.operand) && emit(emitter, ")");
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_ASSIGN)
            if (expr->as.binary.left->kind == EXPR_UNARY &&
                expr->as.binary.left->as.unary.op == TOKEN_STAR)
                return emit(emitter, "(") &&
                       emit_expr(emitter,
                                 expr->as.binary.left->as.unary.operand) &&
                       emit(emitter, ").set(") &&
                       emit_typed_expr(emitter, expr->as.binary.right,
                                       expr->as.binary.left->type) &&
                       emit(emitter, ")");
            else
                return emit(emitter, "(") &&
                       emit_expr(emitter, expr->as.binary.left) &&
                       emit(emitter, " = ") &&
                       emit_typed_expr(emitter, expr->as.binary.right,
                                       expr->as.binary.left->type) &&
                       emit(emitter, ")");
        if (expr->as.binary.op == TOKEN_SLASH || expr->as.binary.op == TOKEN_PERCENT) {
            bool is_unsigned = is_unsigned_integer_type(expr->type.value);
            const char *helper = expr->as.binary.op == TOKEN_SLASH
                ? (is_unsigned ? "meg_div_unsigned" : "meg_div_signed")
                : (is_unsigned ? "meg_rem_unsigned" : "meg_rem_signed");
            return emit_integer_conversion_start(emitter, expr->type) &&
                   emit(emitter, "%s(", helper) &&
                   (!is_unsigned ? emit(emitter, "%u, ", integer_bits(expr->type.value)) : true) &&
                   emit_expr(emitter, expr->as.binary.left) && emit(emitter, ", ") &&
                   emit_expr(emitter, expr->as.binary.right) && emit(emitter, ")") &&
                   emit_integer_conversion_end(emitter, expr->type);
        }
        if (expr->as.binary.op == TOKEN_PLUS || expr->as.binary.op == TOKEN_MINUS ||
            expr->as.binary.op == TOKEN_STAR)
            return emit_integer_conversion_start(emitter, expr->type) &&
                   emit(emitter, "(") && emit_expr(emitter, expr->as.binary.left) &&
                   emit(emitter, " %s ", js_operator(expr->as.binary.op)) &&
                   emit_expr(emitter, expr->as.binary.right) && emit(emitter, ")") &&
                   emit_integer_conversion_end(emitter, expr->type);
        return emit(emitter, "(") && emit_expr(emitter, expr->as.binary.left) &&
               emit(emitter, " %s ", js_operator(expr->as.binary.op)) &&
               emit_expr(emitter, expr->as.binary.right) && emit(emitter, ")");
    }
    diagnostic_emit(emitter->diagnostics, DIAGNOSTIC_ERROR, expr->span,
                    "internal error: unknown expression kind");
    emitter->failed = true;
    return false;
}

static bool emit_statements(JsEmitter *emitter, const Statement *statement, unsigned depth);

static bool emit_block(JsEmitter *emitter, const Statement *block, unsigned depth)
{
    return emit(emitter, "{\n") && emit_statements(emitter, block->as.block.items, depth + 1) &&
           emit_indent(emitter, depth) && emit(emitter, "}");
}

static bool emit_statements(JsEmitter *emitter, const Statement *statement, unsigned depth)
{
    for (; statement; statement = statement->next) {
        if (!emit_indent(emitter, depth)) return false;
        switch (statement->kind) {
        case STMT_LET:
            if (!emit(emitter, "let meg_v_%u = ", statement->as.let.symbol->id) ||
                !emit_typed_expr(emitter, statement->as.let.value, statement->as.let.type) ||
                !emit(emitter, ";\n")) return false;
            break;
        case STMT_RETURN:
            if (!emit(emitter, "return ") ||
                !emit_typed_expr(emitter, statement->as.expression,
                                 (Type){emitter->return_type, TYPE_VALUE}) ||
                !emit(emitter, ";\n")) return false;
            break;
        case STMT_EXPR:
            if (!emit_expr(emitter, statement->as.expression) || !emit(emitter, ";\n")) return false;
            break;
        case STMT_BLOCK:
            if (!emit_block(emitter, statement, depth) || !emit(emitter, "\n")) return false;
            break;
        case STMT_IF:
            if (!emit(emitter, "if (") || !emit_expr(emitter, statement->as.branch.condition) ||
                !emit(emitter, ") ") || !emit_block(emitter, statement->as.branch.then_branch, depth)) return false;
            if (statement->as.branch.else_branch &&
                (!emit(emitter, " else ") || !emit_block(emitter, statement->as.branch.else_branch, depth))) return false;
            if (!emit(emitter, "\n")) return false;
            break;
        case STMT_WHILE:
            if (!emit(emitter, "while (") || !emit_expr(emitter, statement->as.loop.condition) ||
                !emit(emitter, ") ") || !emit_block(emitter, statement->as.loop.body, depth) ||
                !emit(emitter, "\n")) return false;
            break;
        case STMT_FOR: {
            const Statement *initializer = statement->as.iteration.initializer;
            if (!emit(emitter, "for (let meg_v_%u = ", initializer->as.let.symbol->id) ||
                !emit_typed_expr(emitter, initializer->as.let.value,
                                 initializer->as.let.type) || !emit(emitter, "; ") ||
                !emit_expr(emitter, statement->as.iteration.condition) || !emit(emitter, "; ") ||
                !emit_expr(emitter, statement->as.iteration.step) || !emit(emitter, ") ") ||
                !emit_block(emitter, statement->as.iteration.body, depth) ||
                !emit(emitter, "\n")) return false;
            break;
        }
        }
    }
    return true;
}

bool codegen_js(FILE *out, const Program *program, DiagnosticSink diagnostics)
{
    JsEmitter emitter = {out, diagnostics, TYPE_ERROR, false};
    const Function *function;
    if (!out || !program) return false;
    if (!emit(&emitter,
        "\"use strict\";\n\n"
        "function meg_signed(bits, value) {\n"
        "    const narrowed = BigInt.asIntN(bits, value);\n"
        "    if (narrowed !== value) throw new RangeError(\"signed integer overflow\");\n"
        "    return value;\n}\n"
        "function meg_unsigned(bits, value) {\n"
        "    return BigInt.asUintN(bits, value);\n}\n"
        "function meg_div_signed(bits, a, b) {\n"
        "    const minimum = -(1n << BigInt(bits - 1));\n"
        "    if (b === 0n || (a === minimum && b === -1n)) throw new RangeError(\"invalid signed division\");\n"
        "    return a / b;\n}\n"
        "function meg_rem_signed(bits, a, b) {\n"
        "    const minimum = -(1n << BigInt(bits - 1));\n"
        "    if (b === 0n || (a === minimum && b === -1n)) throw new RangeError(\"invalid signed remainder\");\n"
        "    return a %% b;\n}\n\n"
        "function meg_div_unsigned(a, b) {\n"
        "    if (b === 0n) throw new RangeError(\"invalid unsigned division\");\n"
        "    return a / b;\n}\n"
        "function meg_rem_unsigned(a, b) {\n"
        "    if (b === 0n) throw new RangeError(\"invalid unsigned remainder\");\n"
        "    return a %% b;\n}\n\n")) return false;
    for (function = program->functions; function; function = function->next) {
        emitter.return_type = function->return_type;
        if (!emit(&emitter, "function ") ||
            !emit_function_name(&emitter, function->symbol) ||
            !emit(&emitter, "(") ||
            !emit_parameters(&emitter, function->parameters) ||
            !emit(&emitter, ") ") ||
            !emit_block(&emitter, function->body, 0) ||
            !emit(&emitter, "\n\n")) return false;
    }
    if (!emit(&emitter, "\n\nprocess.exitCode = Number(meg_main());\n")) return false;
    return !emitter.failed && !ferror(out);
}
