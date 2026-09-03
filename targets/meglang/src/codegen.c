#include <meglang/codegen.h>
#include <meglang/checker.h>
#include <stdarg.h>

typedef struct Emitter
{
    FILE *out;
    DiagnosticSink diagnostics;
    bool failed;
} Emitter;

static bool emit(Emitter *emitter, const char *format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vfprintf(emitter->out, format, arguments);
    va_end(arguments);
    if (result < 0)
        emitter->failed = true;
    return result >= 0;
}

static bool emit_indent(Emitter *emitter, unsigned depth)
{
    while (depth--)
        if (!emit(emitter, "    "))
            return false;
    return true;
}

static const char *c_type(ValueType type)
{
    return type == TYPE_BOOL ? "bool" : "int64_t";
}

static bool emit_expr(Emitter *emitter, const Expr *expr)
{
    const char *operator_text;
    switch (expr->kind)
    {
    case EXPR_INT:
        return emit(emitter, "INT64_C(%lld)", (long long)expr->as.integer);
    case EXPR_BOOL:
        return emit(emitter, "%s", expr->as.boolean ? "true" : "false");
    case EXPR_NAME:
        return emit(emitter, "meg_v_%u", expr->symbol->id);
    case EXPR_UNARY:
        return emit(emitter, "(%s", token_name(expr->as.unary.op)) &&
               emit_expr(emitter, expr->as.unary.operand) && emit(emitter, ")");
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_SLASH || expr->as.binary.op == TOKEN_PERCENT)
        {
            const char *helper = expr->as.binary.op == TOKEN_SLASH ? "meg_div_i64" : "meg_rem_i64";
            return emit(emitter, "%s(", helper) && emit_expr(emitter, expr->as.binary.left) &&
                   emit(emitter, ", ") && emit_expr(emitter, expr->as.binary.right) && emit(emitter, ")");
        }
        operator_text = token_name(expr->as.binary.op);
        return emit(emitter, "(") && emit_expr(emitter, expr->as.binary.left) &&
               emit(emitter, " %s ", operator_text) && emit_expr(emitter, expr->as.binary.right) &&
               emit(emitter, ")");
    }
    diagnostic_emit(emitter->diagnostics, DIAGNOSTIC_ERROR, expr->span,
                    "internal error: unknown expression kind");
    emitter->failed = true;
    return false;
}

static bool emit_statements(Emitter *emitter, const Statement *statement, unsigned depth);

static bool emit_block(Emitter *emitter, const Statement *block, unsigned depth)
{
    return emit(emitter, "{\n") && emit_statements(emitter, block->as.block.items, depth + 1) &&
           emit_indent(emitter, depth) && emit(emitter, "}");
}

static bool emit_statements(Emitter *emitter, const Statement *statement, unsigned depth)
{
    for (; statement; statement = statement->next)
    {
        if (!emit_indent(emitter, depth))
            return false;
        switch (statement->kind)
        {
        case STMT_LET:
            if (!emit(emitter, "%s meg_v_%u = ", c_type(statement->as.let.type),
                      statement->as.let.symbol->id) ||
                !emit_expr(emitter, statement->as.let.value) || !emit(emitter, ";\n"))
                return false;
            break;
        case STMT_RETURN:
            if (!emit(emitter, "return ") || !emit_expr(emitter, statement->as.expression) ||
                !emit(emitter, ";\n"))
                return false;
            break;
        case STMT_EXPR:
            if (!emit_expr(emitter, statement->as.expression) || !emit(emitter, ";\n"))
                return false;
            break;
        case STMT_BLOCK:
            if (!emit_block(emitter, statement, depth) || !emit(emitter, "\n"))
                return false;
            break;
        case STMT_IF:
            if (!emit(emitter, "if (") || !emit_expr(emitter, statement->as.branch.condition) ||
                !emit(emitter, ") ") || !emit_block(emitter, statement->as.branch.then_branch, depth))
                return false;
            if (statement->as.branch.else_branch &&
                (!emit(emitter, " else ") || !emit_block(emitter, statement->as.branch.else_branch, depth)))
                return false;
            if (!emit(emitter, "\n"))
                return false;
            break;
        case STMT_WHILE:
            if (!emit(emitter, "while (") || !emit_expr(emitter, statement->as.loop.condition) ||
                !emit(emitter, ") ") || !emit_block(emitter, statement->as.loop.body, depth) ||
                !emit(emitter, "\n"))
                return false;
            break;
        case STMT_FOR:
        {
            const Statement *initializer = statement->as.iteration.initializer;
            if (!emit(emitter, "for (%s meg_v_%u = ", c_type(initializer->as.let.type),
                      initializer->as.let.symbol->id) ||
                !emit_expr(emitter, initializer->as.let.value) || !emit(emitter, "; ") ||
                !emit_expr(emitter, statement->as.iteration.condition) || !emit(emitter, "; ") ||
                !emit_expr(emitter, statement->as.iteration.step) || !emit(emitter, ") ") ||
                !emit_block(emitter, statement->as.iteration.body, depth) ||
                !emit(emitter, "\n"))
                return false;
            break;
        }
        }
    }
    return true;
}

bool codegen_c(FILE *out, const Program *program, DiagnosticSink diagnostics)
{
    Emitter emitter = {out, diagnostics, false};
    if (!out || !program)
        return false;
    if (!emit(&emitter,
              "#include <stdbool.h>\n#include <stdint.h>\n#include <stdlib.h>\n\n"
              "int64_t meg_div_i64(int64_t a, int64_t b) {\n"
              "    if (b == 0 || (a == INT64_MIN && b == -1)) abort();\n"
              "    return a / b;\n}\n"
              "int64_t meg_rem_i64(int64_t a, int64_t b) {\n"
              "    if (b == 0 || (a == INT64_MIN && b == -1)) abort();\n"
              "    return a %% b;\n}\n\n"
              "static int64_t meg_main(void) "))
        return false;
    if (!emit_block(&emitter, program->function.body, 0))
        return false;
    if (!emit(&emitter, "\n\nint main(void) { return (int)meg_main(); }\n"))
        return false;
    return !emitter.failed && !ferror(out);
}
