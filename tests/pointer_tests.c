#include "test.h"
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/codegen_js.h>
#include <meglang/parser.h>
#include <string.h>

static void ignore(void *context, const Diagnostic *diagnostic)
{
    (void)context;
    (void)diagnostic;
}

static bool output_contains(FILE *output, const char *text)
{
    char generated[32768];
    size_t length;
    rewind(output);
    length = fread(generated, 1, sizeof generated - 1, output);
    generated[length] = '\0';
    return strstr(generated, text) != NULL;
}

int main(void)
{
    const char *text =
        "fn main() -> i64 { "
        "let a: i8 = 1; let ap: *i8 = &a; let ar: ref i8 = ref a; "
        "let b: i16 = 2; let bp: *i16 = &b; let br: ref i16 = ref b; "
        "let c: i32 = 3; let cp: *i32 = &c; let cr: ref i32 = ref c; "
        "let d: i64 = 4; let dp: *i64 = &d; let dr: ref i64 = ref d; "
        "let e: u8 = 5; let ep: *u8 = &e; let er: ref u8 = ref e; "
        "let f: u16 = 6; let fp: *u16 = &f; let fr: ref u16 = ref f; "
        "let g: u32 = 7; let gp: *u32 = &g; let gr: ref u32 = ref g; "
        "let h: u64 = 8; let hp: *u64 = &h; let hr: ref u64 = ref h; "
        "*ap = 9; *ar = 10; *hp = 11; *hr = 12; return *dp; }";
    Source source = {0};
    ParseResult parsed;
    Checker checker;
    DiagnosticSink sink = {ignore, NULL};
    Statement *first;
    FILE *output;

    EXPECT(source_from_string(&source, "pointers.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));
    first = parsed.program->functions->body->as.block.items;
    EXPECT(first->next->as.let.type.value == TYPE_I8);
    EXPECT(first->next->as.let.type.form == TYPE_POINTER);
    EXPECT(first->next->next->as.let.type.value == TYPE_I8);
    EXPECT(first->next->next->as.let.type.form == TYPE_REFERENCE);

    output = tmpfile();
    EXPECT(output != NULL);
    if (output) {
        EXPECT(codegen_c(output, parsed.program, sink));
        EXPECT(output_contains(output, "int8_t * meg_v_1 = (&meg_v_0)"));
        EXPECT(output_contains(output, "uint64_t * meg_v_22 = (&meg_v_21)"));
        EXPECT(output_contains(output, "(*meg_v_1) = ((int8_t)UINT64_C(9))"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output) {
        EXPECT(codegen_js(output, parsed.program, sink));
        EXPECT(output_contains(output, "get: () => meg_v_0"));
        EXPECT(output_contains(output, ").set(meg_signed(8, 9n))"));
        EXPECT(output_contains(output, ").set(meg_unsigned(64, 12n))"));
        fclose(output);
    }

    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);

    EXPECT(source_from_string(
        &source, "bad-pointer.meg",
        "fn main() -> i64 { let x: i16 = 1; let p: *i8 = &x; return 0; }"));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, parsed.program));
    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);

    return RESULT();
}
