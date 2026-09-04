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

    EXPECT(source_from_string(&source, "small.meg",
                              "fn main() -> i8 { let x: i8 = 40 + 2; x = x + 1; return x; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, result.program));
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "too-large.meg",
                              "fn main() -> i8 { let x: i8 = 128; return x; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 1);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "medium.meg",
        "fn main() -> i32 { let x: i16 = 32767; "
        "let y: i32 = 2147483647; return y; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, result.program));
    EXPECT(result.program->function.body->as.block.items->as.let.type == TYPE_I16);
    EXPECT(result.program->function.body->as.block.items->next->as.let.type == TYPE_I32);
    EXPECT(result.program->function.return_type == TYPE_I32);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "medium-too-large.meg",
        "fn main() -> i32 { let x: i16 = 32768; "
        "let y: i32 = 2147483648; return 0; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 2);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "unsigned.meg",
        "fn main() -> u64 { let a: u8 = 255; let b: u16 = 65535; "
        "let c: u32 = 4294967295; let d: u64 = 18446744073709551615; return d; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, result.program));
    EXPECT(result.program->function.return_type == TYPE_U64);
    EXPECT(result.program->function.body->as.block.items->as.let.type == TYPE_U8);
    EXPECT(result.program->function.body->as.block.items->next->as.let.type == TYPE_U16);
    EXPECT(result.program->function.body->as.block.items->next->next->as.let.type == TYPE_U32);
    EXPECT(result.program->function.body->as.block.items->next->next->next->as.let.type == TYPE_U64);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "unsigned-too-large.meg",
        "fn main() -> u64 { let a: u8 = 256; let b: u16 = 65536; "
        "let c: u32 = 4294967296; return 0; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 3);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "unsigned-negative.meg",
        "fn main() -> u8 { return -1; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, result.program));
    EXPECT(checker.errors == 1);
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "signed-minimums.meg",
        "fn main() -> i64 { let a: i8 = -128; let b: i16 = -32768; "
        "let c: i32 = -2147483648; return -9223372036854775808; }"));
    result = parse_source(&source, sink);
    EXPECT(result.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, result.program));
    checker_destroy(&checker);
    program_destroy(result.program);
    source_destroy(&source);
    return RESULT();
}
