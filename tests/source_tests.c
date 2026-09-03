#include "test.h"
#include <meglang/source.h>
#include <string.h>
int main(void)
{
    Source source = {0};
    SourceSpan span;
    EXPECT(source_from_string(&source, "memory.meg", "hello"));
    EXPECT(source.length == 5);
    span = (SourceSpan){&source, 1, 3, 1, 2};
    EXPECT(span_valid(span));
    EXPECT(span_equals(span, "ell"));
    EXPECT(!span_equals(span, "hello"));
    source_destroy(&source);
    source_destroy(&source);
    EXPECT(source.text == NULL);
    return RESULT();
}
