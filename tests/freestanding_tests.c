#include "test.h"
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/parser.h>
#include <meglang/source.h>

#include <string.h>

static void ignore(void *context, const Diagnostic *diagnostic)
{ (void)context; (void)diagnostic; }

int main(void)
{
    const char *text =
        "fn main() -> i64 { let sum: i64 = 0; "
        "for (let i: i64 = 0; i < 7; i = i + 1) { sum = sum + i; } return sum * 2; }";
    Source source = {0};
    ParseResult parsed;
    Checker checker;
    FILE *output;
    char generated[16384] = {0};
    DiagnosticSink sink = {ignore, NULL};
    EXPECT(source_from_string(&source, "freestanding.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    output = tmpfile();
    EXPECT(output != NULL);
    if (output) {
        EXPECT(codegen_freestanding_c(output, parsed.program, sink));
        rewind(output);
        EXPECT(fread(generated, 1, sizeof generated - 1, output) > 0);
        EXPECT(strstr(generated, "int64_t meg_entry(void)") != NULL);
        EXPECT(strstr(generated, "extern void meg_panic(void)") != NULL);
        EXPECT(strstr(generated, "#include <stdlib.h>") == NULL);
        EXPECT(strstr(generated, "abort()") == NULL);
        EXPECT(strstr(generated, "int main(void)") == NULL);
        fclose(output);
    }
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
    return RESULT();
}
