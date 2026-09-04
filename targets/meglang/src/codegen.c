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
    switch (type)
    {
    case TYPE_I8:
        return "int8_t";
    case TYPE_I16:
        return "int16_t";
    case TYPE_I32:
        return "int32_t";
    case TYPE_I64:
        return "int64_t";
    case TYPE_U8:
        return "uint8_t";
    case TYPE_U16:
        return "uint16_t";
    case TYPE_U32:
        return "uint32_t";
    case TYPE_U64:
        return "uint64_t";
    case TYPE_BOOL:
        return "bool";
    default:
        return "int64_t";
    }
}

static bool is_signed_integer_type(ValueType type)
{
    return type == TYPE_I8 || type == TYPE_I16 ||
           type == TYPE_I32 || type == TYPE_I64;
}

static bool is_unsigned_integer_type(ValueType type)
{
    return type == TYPE_U8 || type == TYPE_U16 ||
           type == TYPE_U32 || type == TYPE_U64;
}

static bool emit_c_type(Emitter *emitter, Type type)
{
    return emit(emitter, "%s%s", c_type(type.value),
                type.form == TYPE_VALUE ? "" : " *");
}

static bool emit_expr(Emitter *emitter, const Expr *expr)
{
    const char *operator_text;
    switch (expr->kind)
    {
    case EXPR_INT:
        return emit(emitter, "((%s)UINT64_C(%llu))", c_type(expr->type.value),
                    (unsigned long long)expr->as.integer);
    case EXPR_BOOL:
        return emit(emitter, "%s", expr->as.boolean ? "true" : "false");
    case EXPR_NAME:
        return emit(emitter, "meg_v_%u", expr->symbol->id);
    case EXPR_UNARY:
        if (expr->as.unary.op == TOKEN_MINUS &&
            expr->as.unary.operand->kind == EXPR_INT &&
            is_signed_integer_type(expr->type.value))
        {
            if (expr->type.value == TYPE_I64 &&
                expr->as.unary.operand->as.integer == (uint64_t)INT64_MAX + 1)
                return emit(emitter, "INT64_MIN");
            return emit(emitter, "((%s)(-INT64_C(%llu)))", c_type(expr->type.value),
                        (unsigned long long)expr->as.unary.operand->as.integer);
        }
        return emit(emitter, "(%s", expr->as.unary.op == TOKEN_REF
                    ? "&" : token_name(expr->as.unary.op)) &&
               emit_expr(emitter, expr->as.unary.operand) && emit(emitter, ")");
    case EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_SLASH || expr->as.binary.op == TOKEN_PERCENT)
        {
            bool is_unsigned = is_unsigned_integer_type(expr->type.value);
            const char *helper = expr->as.binary.op == TOKEN_SLASH
                                     ? (is_unsigned ? "meg_div_u64" : "meg_div_i64")
                                     : (is_unsigned ? "meg_rem_u64" : "meg_rem_i64");
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
            if (!emit_c_type(emitter, statement->as.let.type) ||
                !emit(emitter, " meg_v_%u = ", statement->as.let.symbol->id) ||
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
            if (!emit(emitter, "for (") ||
                !emit_c_type(emitter, initializer->as.let.type) ||
                !emit(emitter, " meg_v_%u = ", initializer->as.let.symbol->id) ||
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

static bool codegen_c_impl(FILE *out, const Program *program, DiagnosticSink diagnostics, bool freestanding)
{
    Emitter emitter = {out, diagnostics, false};
    const char *trap = freestanding ? "meg_trap()" : "abort()";
    if (!out || !program)
        return false;
    if (freestanding)
    {
        if (!emit(&emitter,
                  "#include <stdbool.h>\n#include <stdint.h>\n\n"
                  "extern void meg_panic(void);\n"
                  "static void meg_trap(void) { meg_panic(); for (;;) {} }\n"))
            return false;
    }
    else if (!emit(&emitter,
                   "#include <stdbool.h>\n#include <stdint.h>\n#include <stdlib.h>\n\n"))
        return false;

    /* Division helpers trap instead of invoking C's undefined behaviour. */
    if (!emit(&emitter,
              "int64_t meg_div_i64(int64_t a, int64_t b) {\n"
              "    if (b == 0 || (a == INT64_MIN && b == -1)) %s;\n"
              "    return a / b;\n}\n"
              "int64_t meg_rem_i64(int64_t a, int64_t b) {\n"
              "    if (b == 0 || (a == INT64_MIN && b == -1)) %s;\n"
              "    return a %% b;\n}\n\n"
              "uint64_t meg_div_u64(uint64_t a, uint64_t b) {\n"
              "    if (b == 0) %s;\n"
              "    return a / b;\n}\n"
              "uint64_t meg_rem_u64(uint64_t a, uint64_t b) {\n"
              "    if (b == 0) %s;\n"
              "    return a %% b;\n}\n\n",
              trap, trap, trap, trap))
        return false;

    if (!emit(&emitter, freestanding ? "%s meg_entry(void) " : "static %s meg_main(void) ",
              c_type(program->function.return_type)))
        return false;
    if (!emit_block(&emitter, program->function.body, 0))
        return false;
    if (!freestanding &&
        !emit(&emitter, "\n\nint main(void) { return (int)meg_main(); }\n"))
        return false;
    if (freestanding && !emit(&emitter, "\n"))
        return false;
    return !emitter.failed && !ferror(out);
}
bool codegen_c(FILE *out, const Program *program, DiagnosticSink diagnostics)
{
    return codegen_c_impl(out, program, diagnostics, false);
}

bool codegen_freestanding_c(FILE *out, const Program *program,
                            DiagnosticSink diagnostics)
{
    return codegen_c_impl(out, program, diagnostics, true);
}
