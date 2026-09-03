#include "test.h"
#include <meglang/checker.h>
#include <meglang/parser.h>

static void ignore(void *context, const Diagnostic *diagnostic)
{
    (void)context;
    (void)diagnostic;
}
int main(void)
{
    Source source = {0};
    ParseResult result;
    Checker checker;
    DiagnosticSink sink = {ignore, NULL};
    EXPECT(source_from_string(&source, "test.meg", "fn main() -> i64 { let x: i64 = 42; return x; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, result.program));
    EXPECT(result.program->function.body->as.block.items->as.let.symbol != NULL);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "bad.meg", "fn main() -> i64 { return missing; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 1);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "bad.meg",
                              "fn main() -> i64 { let x: i64 = 0; return x = 1; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 1);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);
    return RESULT();
}
