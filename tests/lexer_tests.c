#include "test.h"
#include <meglang/lexer.h>
static void no_diagnostic(void *context, const Diagnostic *diagnostic)
{ (void)context; (void)diagnostic; }
int main(void)
{
    Source source = {0};
    Lexer lexer;
    Token token;
    DiagnosticSink sink = {no_diagnostic, NULL};
    EXPECT(source_from_string(&source, "memory.meg", "fn main // c\r\n0x2a != false for"));
    lexer_init(&lexer, &source, sink);
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_FN); EXPECT(token.span.line == 1);
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_IDENTIFIER); EXPECT(span_equals(token.span, "main"));
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_INTEGER); EXPECT(token.span.line == 2); EXPECT(token.span.column == 1);
    EXPECT(lexer_next(&lexer).kind == TOKEN_NOT_EQUAL);
    EXPECT(lexer_next(&lexer).kind == TOKEN_FALSE);
    EXPECT(lexer_next(&lexer).kind == TOKEN_FOR);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 0);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "types.meg", "u8, u16 u32 u64"));
    lexer_init(&lexer, &source, sink);
    EXPECT(lexer_next(&lexer).kind == TOKEN_U8);
    EXPECT(lexer_next(&lexer).kind == TOKEN_COMMA);
    EXPECT(lexer_next(&lexer).kind == TOKEN_U16);
    EXPECT(lexer_next(&lexer).kind == TOKEN_U32);
    EXPECT(lexer_next(&lexer).kind == TOKEN_U64);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 0);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "strings.meg",
                              "\"plain\" u8\"caf\xc3\xa9\" "
                              "u\"snowman: \xe2\x98\x83\" "
                              "\"escaped \\\" quote\""));
    lexer_init(&lexer, &source, sink);
    token = lexer_next(&lexer);
    EXPECT(token.kind == TOKEN_STRING);
    EXPECT(span_equals(token.span, "\"plain\""));
    token = lexer_next(&lexer);
    EXPECT(token.kind == TOKEN_STRING);
    EXPECT(span_equals(token.span, "u8\"caf\xc3\xa9\""));
    token = lexer_next(&lexer);
    EXPECT(token.kind == TOKEN_USTRING);
    EXPECT(span_equals(token.span, "u\"snowman: \xe2\x98\x83\""));
    token = lexer_next(&lexer);
    EXPECT(token.kind == TOKEN_STRING);
    EXPECT(span_equals(token.span, "\"escaped \\\" quote\""));
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 0);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "non-ascii.meg", "\"\xc3\xa9\""));
    lexer_init(&lexer, &source, sink);
    EXPECT(lexer_next(&lexer).kind == TOKEN_ERROR);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 1);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "invalid-utf8.meg", "u8\"\xc0\xaf\""));
    lexer_init(&lexer, &source, sink);
    EXPECT(lexer_next(&lexer).kind == TOKEN_ERROR);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 1);
    source_destroy(&source);

    EXPECT(source_from_string(&source, "unterminated.meg", "\"open"));
    lexer_init(&lexer, &source, sink);
    EXPECT(lexer_next(&lexer).kind == TOKEN_ERROR);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 1);
    source_destroy(&source);
    return RESULT();
}
