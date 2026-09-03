#include "test.h"
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/parser.h>
#include <meglang/source.h>
static void ignore(void *context, const Diagnostic *diagnostic)
{
    (void)context;
    (void)diagnostic;
}
int main(void)
{
    const char *text = "fn main() -> i64 { let x: i64 = 40 + 2; return x; }";
    Source source = {0};
    ParseResult parsed;
    Checker checker;
    FILE *output;
    DiagnosticSink sink = {ignore, NULL};
    EXPECT(source_from_string(&source, "test.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, parsed.program, sink));
        fclose(output);
    }
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
    return RESULT();
}
