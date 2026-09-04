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

    EXPECT(source_from_string(
        &source, "function-widths.meg",
        "fn main() -> u64 { "
        "let a: i8 = signed8(); let b: i16 = signed16(); "
        "let c: i32 = signed32(); let d: i64 = signed64(); "
        "let e: u8 = unsigned8(); let f: u16 = unsigned16(); "
        "let g: u32 = unsigned32(); let h: u64 = unsigned64(); "
        "if -1 < a { return h; } else { return 0; } } "
        "fn signed8() -> i8 { return 127; } "
        "fn signed16() -> i16 { return 32767; } "
        "fn signed32() -> i32 { return 2147483647; } "
        "fn signed64() -> i64 { return 9223372036854775807; } "
        "fn unsigned8() -> u8 { return 255; } "
        "fn unsigned16() -> u16 { return 65535; } "
        "fn unsigned32() -> u32 { return 4294967295; } "
        "fn unsigned64() -> u64 { return 18446744073709551615; }"));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    EXPECT(parsed.program->functions->next->return_type == TYPE_I8);
    EXPECT(parsed.program->functions->next->next->return_type == TYPE_I16);
    EXPECT(parsed.program->functions->next->next->next->return_type == TYPE_I32);
    EXPECT(parsed.program->functions->next->next->next->next->return_type == TYPE_I64);
    EXPECT(parsed.program->functions->next->next->next->next->next->return_type == TYPE_U8);
    EXPECT(parsed.program->functions->next->next->next->next->next->next->return_type == TYPE_U16);
    EXPECT(parsed.program->functions->next->next->next->next->next->next->next->return_type == TYPE_U32);
    EXPECT(parsed.program->functions->next->next->next->next->next->next->next->next->return_type == TYPE_U64);

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, parsed.program, sink));
        EXPECT(output_contains(output, "static int8_t meg_f_1(void)"));
        EXPECT(output_contains(output, "static int16_t meg_f_2(void)"));
        EXPECT(output_contains(output, "static int32_t meg_f_3(void)"));
        EXPECT(output_contains(output, "static int64_t meg_f_4(void)"));
        EXPECT(output_contains(output, "static uint8_t meg_f_5(void)"));
        EXPECT(output_contains(output, "static uint16_t meg_f_6(void)"));
        EXPECT(output_contains(output, "static uint32_t meg_f_7(void)"));
        EXPECT(output_contains(output, "static uint64_t meg_f_8(void)"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, parsed.program, sink));
        EXPECT(output_contains(output, "return meg_signed(8, 127n)"));
        EXPECT(output_contains(output, "return meg_signed(16, 32767n)"));
        EXPECT(output_contains(output, "return meg_signed(32, 2147483647n)"));
        EXPECT(output_contains(output, "return meg_signed(64, 9223372036854775807n)"));
        EXPECT(output_contains(output, "return meg_unsigned(8, 255n)"));
        EXPECT(output_contains(output, "return meg_unsigned(16, 65535n)"));
        EXPECT(output_contains(output, "return meg_unsigned(32, 4294967295n)"));
        EXPECT(output_contains(output, "return meg_unsigned(64, 18446744073709551615n)"));
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
