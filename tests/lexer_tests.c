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
    EXPECT(source_from_string(&source, "memory.meg", "fn main // c\r\n0x2a != false"));
    lexer_init(&lexer, &source, sink);
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_FN); EXPECT(token.span.line == 1);
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_IDENTIFIER); EXPECT(span_equals(token.span, "main"));
    token = lexer_next(&lexer); EXPECT(token.kind == TOKEN_INTEGER); EXPECT(token.span.line == 2); EXPECT(token.span.column == 1);
    EXPECT(lexer_next(&lexer).kind == TOKEN_NOT_EQUAL);
    EXPECT(lexer_next(&lexer).kind == TOKEN_FALSE);
    EXPECT(lexer_next(&lexer).kind == TOKEN_EOF);
    EXPECT(lexer.errors == 0);
    source_destroy(&source);
    return RESULT();
}
