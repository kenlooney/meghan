#include "test.h"

#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/codegen_js.h>
#include <meglang/parser.h>
#include <meglang/source.h>

#include <string.h>

static void ignore(void *context, const Diagnostic *diagnostic)
{
    (void)context;
    (void)diagnostic;
}

static bool output_contains(FILE *output, const char *needle)
{
    char generated[16384] = {0};
    rewind(output);
    (void)fread(generated, 1, sizeof generated - 1, output);
    return strstr(generated, needle) != NULL;
}

static void expect_rejected(const char *text)
{
    Source source = {0};
    DiagnosticSink sink = {ignore, NULL};
    ParseResult parsed;
    Checker checker;
    EXPECT(source_from_string(&source, "bad-parameters.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, parsed.program));
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
}

int main(void)
{
    const char *text =
        "fn main() -> i64 { "
        "let small: i8 = add8(20, 22); "
        "let value: i64 = 40; let pointer: *i64 = &value; "
        "let reference: ref i64 = ref value; "
        "let first: i64 = increment(pointer); "
        "let second: i64 = read(reference); "
        "if small != 42 { return 0; } return add(first, second); } "
        "fn add8(left: i8, right: i8) -> i8 { return left + right; } "
        "fn increment(value: *i64) -> i64 { "
        "*value = *value + 1; return *value; } "
        "fn read(value: ref i64) -> i64 { return *value; } "
        "fn add(left: i64, right: i64) -> i64 { return left + right; }";
    Source source = {0};
    DiagnosticSink sink = {ignore, NULL};
    ParseResult parsed;
    Checker checker;
    FILE *output;
    const Function *add8;
    const Expr *call;

    EXPECT(source_from_string(&source, "parameters.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    add8 = parsed.program->functions->next;
    EXPECT(add8->parameters != NULL);
    EXPECT(add8->parameters->next != NULL);
    EXPECT(add8->parameters->next->next == NULL);
    call = parsed.program->functions->body->as.block.items->as.let.value;
    EXPECT(call->kind == EXPR_CALL);
    EXPECT(call->as.call.arguments != NULL);
    EXPECT(call->as.call.arguments->next != NULL);
    EXPECT(call->as.call.arguments->next->next == NULL);

    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    EXPECT(add8->parameters->type.value == TYPE_I8);
    EXPECT(add8->parameters->symbol != NULL);

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(ast_print(output, parsed.program));
        EXPECT(output_contains(output, "function add8(left: i8, right: i8) -> i8"));
        EXPECT(output_contains(output, "add8(20, 22)"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, parsed.program, sink));
        EXPECT(output_contains(output, "static int8_t meg_f_1(int8_t meg_v_"));
        EXPECT(output_contains(output, "meg_f_1(((int8_t)UINT64_C(20)), ((int8_t)UINT64_C(22)))"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, parsed.program, sink));
        EXPECT(output_contains(output, "function meg_f_1(meg_v_"));
        EXPECT(output_contains(output, "meg_f_1(20n, 22n)"));
        fclose(output);
    }

    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);

    expect_rejected(
        "fn main() -> i64 { return add(1); } "
        "fn add(left: i64, right: i64) -> i64 { return left + right; }");
    expect_rejected(
        "fn main() -> i64 { return identity(1, 2); } "
        "fn identity(value: i64) -> i64 { return value; }");
    expect_rejected(
        "fn main() -> i64 { return choose(1); } "
        "fn choose(flag: bool) -> i64 { if flag { return 1; } else { return 0; } }");
    expect_rejected(
        "fn main() -> i64 { return duplicate(1, 2); } "
        "fn duplicate(value: i64, value: i64) -> i64 { return value; }");
    expect_rejected("fn main(value: i64) -> i64 { return value; }");

    return RESULT();
}
