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
int main(void)
{
    const char *text =
        "fn main() -> u64 { let x: i16 = 40; let y: i32 = 2; "
        "let a: u8 = 255; let b: u16 = 65535; let c: u32 = 4294967295; "
        "let d: u64 = 18446744073709551615; x = x + 1; a = a + 1; "
        "return d / 1; }";
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
        {
            char generated[8192];
            size_t length;
            rewind(output);
            length = fread(generated, 1, sizeof generated - 1, output);
            generated[length] = '\0';
            EXPECT(strstr(generated, "int16_t meg_v_0") != NULL);
            EXPECT(strstr(generated, "int32_t meg_v_1") != NULL);
            EXPECT(strstr(generated, "uint8_t meg_v_2") != NULL);
            EXPECT(strstr(generated, "uint16_t meg_v_3") != NULL);
            EXPECT(strstr(generated, "uint32_t meg_v_4") != NULL);
            EXPECT(strstr(generated, "uint64_t meg_v_5") != NULL);
            EXPECT(strstr(generated, "UINT64_C(18446744073709551615)") != NULL);
            EXPECT(strstr(generated, "static uint64_t meg_main(void)") != NULL);
            EXPECT(strstr(generated, "return meg_div_u64(meg_v_5") != NULL);
        }
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, parsed.program, sink));
        {
            char generated[8192];
            size_t length;
            rewind(output);
            length = fread(generated, 1, sizeof generated - 1, output);
            generated[length] = '\0';
            EXPECT(strstr(generated, "18446744073709551615n") != NULL);
            EXPECT(strstr(generated, "meg_signed(16, (meg_v_0 + 1n))") != NULL);
            EXPECT(strstr(generated, "meg_unsigned(8, (meg_v_2 + 1n))") != NULL);
            EXPECT(strstr(generated, "meg_div_unsigned(meg_v_5, 1n)") != NULL);
            EXPECT(strstr(generated, "return meg_unsigned(64,") != NULL);
        }
        fclose(output);
    }
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
    return RESULT();
}
