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

    EXPECT(source_from_string(
        &source, "integer-types.meg",
        "fn main() -> i8 { let a: i8 = 1; let b: i16 = 2; "
        "let c: i32 = 3; let d: i64 = 4; return a; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    EXPECT(span_equals(result.program->function.return_type_name, "i8"));
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "unsigned-types.meg",
        "fn main() -> u64 { let a: u8 = 1; let b: u16 = 2; "
        "let c: u32 = 3; let d: u64 = 4; return d; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    EXPECT(span_equals(result.program->function.return_type_name, "u64"));
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "u64-overflow.meg",
        "fn main() -> u64 { return 18446744073709551616; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 1);
    program_destroy(result.program);
    source_destroy(&source);
    return RESULT();
}
