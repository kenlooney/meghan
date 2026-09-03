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

typedef struct JsEmitter { FILE *out; DiagnosticSink diagnostics; bool failed; } JsEmitter;

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

static bool emit_expr(JsEmitter *emitter, const Expr *expr)
{
    switch (expr->kind) {
    case EXPR_INT: return emit(emitter, "%lldn", (long long)expr->as.integer);
    case EXPR_BOOL: return emit(emitter, "%s", expr->as.boolean ? "true" : "false");
    case EXPR_NAME: return emit(emitter, "meg_v_%u", expr->symbol->id);
    case EXPR_UNARY:
        return emit(emitter, "(%s", token_name(expr->as.unary.op)) &&
               emit_expr(emitter, expr->as.unary.operand) && emit(emitter, ")");
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_SLASH || expr->as.binary.op == TOKEN_PERCENT) {
            const char *helper = expr->as.binary.op == TOKEN_SLASH ? "meg_div_i64" : "meg_rem_i64";
            return emit(emitter, "%s(", helper) && emit_expr(emitter, expr->as.binary.left) &&
                   emit(emitter, ", ") && emit_expr(emitter, expr->as.binary.right) && emit(emitter, ")");
        }
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
                !emit_expr(emitter, statement->as.let.value) || !emit(emitter, ";\n")) return false;
            break;
        case STMT_RETURN:
            if (!emit(emitter, "return ") || !emit_expr(emitter, statement->as.expression) ||
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
                !emit_expr(emitter, initializer->as.let.value) || !emit(emitter, "; ") ||
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
    JsEmitter emitter = {out, diagnostics, false};
    if (!out || !program) return false;
    if (!emit(&emitter,
        "\"use strict\";\n\n"
        "function meg_div_i64(a, b) {\n"
        "    if (b === 0n || (a === -9223372036854775808n && b === -1n)) throw new RangeError(\"invalid i64 division\");\n"
        "    return a / b;\n}\n"
        "function meg_rem_i64(a, b) {\n"
        "    if (b === 0n || (a === -9223372036854775808n && b === -1n)) throw new RangeError(\"invalid i64 remainder\");\n"
        "    return a %% b;\n}\n\n"
        "function meg_main() ")) return false;
    if (!emit_block(&emitter, program->function.body, 0)) return false;
    if (!emit(&emitter, "\n\nprocess.exitCode = Number(meg_main());\n")) return false;
    return !emitter.failed && !ferror(out);
}
