#include "test.h"
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
    DiagnosticSink sink = {ignore, NULL};
    EXPECT(source_from_string(&source, "test.meg", "fn main() -> i64 { return 1 + 2 * 3; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    EXPECT(result.program != NULL);
    EXPECT(result.program->function.body->kind == STMT_BLOCK);
    EXPECT(result.program->function.body->as.block.items->kind == STMT_RETURN);
    EXPECT(result.program->function.body->as.block.items->as.expression->kind == EXPR_BINARY);
    program_destroy(result.program);
    source_destroy(&source);
    return RESULT();
}
