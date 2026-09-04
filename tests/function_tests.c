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

int main(void)
{
    const char *text =
        "fn main() -> i64 { return answer(); } "
        "fn answer() -> i64 { return forty() + 2; } "
        "fn forty() -> i64 { return 40; }";
    Source source = {0};
    ParseResult parsed;
    Checker checker;
    DiagnosticSink sink = {ignore, NULL};
    FILE *output;

    EXPECT(source_from_string(&source, "functions.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    EXPECT(parsed.program->functions != NULL);
    EXPECT(parsed.program->functions->next != NULL);
    EXPECT(parsed.program->functions->next->next != NULL);
    EXPECT(parsed.program->functions->body->as.block.items->as.expression->kind == EXPR_CALL);

    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    EXPECT(parsed.program->functions->body->as.block.items->as.expression->as.call.symbol != NULL);

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, parsed.program, sink));
        EXPECT(output_contains(output, "static int64_t meg_main(void)"));
        EXPECT(output_contains(output, "meg_f_1()"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, parsed.program, sink));
        EXPECT(output_contains(output, "function meg_main()"));
        EXPECT(output_contains(output, "meg_f_1()"));
        fclose(output);
    }

    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "unknown.meg",
                              "fn main() -> i64 { return missing(); }"));
    parsed = parse_source(&source, sink);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, parsed.program));
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);

    return RESULT();
}
