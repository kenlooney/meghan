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
    char generated[16384] = {0};
    size_t length;
    rewind(output);
    length = fread(generated, 1, sizeof generated - 1, output);
    generated[length] = '\0';
    return strstr(generated, text) != NULL;
}

int main(void)
{
    const char *text =
        "fn ascii() -> char { return 'A'; } "
        "fn utf8() -> utf8_char { return u8'\xc3\xa9'; } "
        "fn wide() -> uchar { return u'\xf0\x9f\x98\x80'; } "
        "fn main() -> char { return '\\n'; }";
    Source source = {0};
    ParseResult parsed;
    Checker checker;
    DiagnosticSink sink = {ignore, NULL};
    const Function *function;
    FILE *output;

    EXPECT(source_from_string(&source, "characters.meg", text));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    function = parsed.program->functions;
    EXPECT(function->body->as.block.items->as.expression->kind == EXPR_CHAR);
    EXPECT(function->body->as.block.items->as.expression->as.character.value == 65);
    function = function->next;
    EXPECT(function->body->as.block.items->as.expression->kind == EXPR_UTF8_CHAR);
    EXPECT(function->body->as.block.items->as.expression->as.utf8_character.value == 0xe9);
    function = function->next;
    EXPECT(function->body->as.block.items->as.expression->kind == EXPR_UCHAR);
    EXPECT(function->body->as.block.items->as.expression->as.uchar.value == 0x1f600);

    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, parsed.program));

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, parsed.program, sink));
        EXPECT(output_contains(output, "static uint8_t"));
        EXPECT(output_contains(output, "static uint32_t"));
        EXPECT(output_contains(output, "UINT32_C(233)"));
        EXPECT(output_contains(output, "UINT32_C(128512)"));
        fclose(output);
    }

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, parsed.program, sink));
        EXPECT(output_contains(output, "return 65;"));
        EXPECT(output_contains(output, "return 233;"));
        EXPECT(output_contains(output, "return 128512;"));
        fclose(output);
    }

    checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
    return RESULT();
}
