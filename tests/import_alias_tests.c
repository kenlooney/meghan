#include "test.h"
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/codegen_js.h>
#include <meglang/module.h>
#include <meglang/parser.h>
#include <string.h>

static void ignore(void *context, const Diagnostic *diagnostic)
{
    (void)context;
    (void)diagnostic;
}

static bool write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    bool written = file && fputs(text, file) >= 0;
    if (file && fclose(file) != 0)
        written = false;
    return written;
}

int main(void)
{
    const char *entry_path = "meg_import_alias_entry.meg";
    const char *library_path = "meg_import_alias_library.meg";
    Source source = {0};
    ParseResult parsed;
    DiagnosticSink sink = {ignore, NULL};
    ModuleGraph graph = {0};
    Checker checker;
    const Function *function;
    const Expr *call = NULL;
    FILE *output;

    EXPECT(source_from_string(
        &source, "entry.meg",
        "import \"library.meg\" as library; "
        "fn main() -> i64 { return library.answer(); }"));
    parsed = parse_source(&source, sink);
    EXPECT(parsed.errors == 0);
    EXPECT(parsed.program->imports != NULL);
    EXPECT(span_equals(parsed.program->imports->path, "library.meg"));
    EXPECT(span_equals(parsed.program->imports->alias, "library"));
    call = parsed.program->functions->body->as.block.items->as.expression;
    EXPECT(call->kind == EXPR_CALL);
    EXPECT(span_equals(call->as.call.qualifier, "library"));
    EXPECT(span_equals(call->as.call.name, "answer"));

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        char printed[256] = {0};
        size_t length;
        EXPECT(ast_print(output, parsed.program));
        rewind(output);
        length = fread(printed, 1, sizeof printed - 1, output);
        printed[length] = '\0';
        EXPECT(strstr(printed, "import \"library.meg\" as library") != NULL);
        EXPECT(strstr(printed, "library.answer()") != NULL);
        fclose(output);
    }

    program_destroy(parsed.program);
    source_destroy(&source);

    EXPECT(write_text(library_path,
                      "fn answer() -> i64 { return 42; }"));
    EXPECT(write_text(entry_path,
                      "import \"meg_import_alias_library.meg\" as library; "
                      "fn answer() -> i64 { return 1; } "
                      "fn main() -> i64 { return library.answer(); }"));
    EXPECT(module_load(&graph, entry_path, sink));
    EXPECT(graph.program != NULL);
    EXPECT(graph.program->imports != NULL);
    EXPECT(graph.program->imports->target != NULL);

    checker_init(&checker, sink);
    EXPECT(checker_check(&checker, graph.program));
    call = NULL;
    for (function = graph.program->functions; function; function = function->next)
        if (span_equals(function->name, "main"))
            call = function->body->as.block.items->as.expression;
    EXPECT(call != NULL);
    EXPECT(call->as.call.symbol != NULL);
    if (call && call->as.call.symbol)
        EXPECT(call->as.call.symbol->name.source == graph.program->imports->target);

    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_c(output, graph.program, sink));
        fclose(output);
    }
    output = tmpfile();
    EXPECT(output != NULL);
    if (output)
    {
        EXPECT(codegen_js(output, graph.program, sink));
        fclose(output);
    }

    checker_destroy(&checker);
    module_graph_destroy(&graph);

    EXPECT(write_text(library_path,
                      "fn main() -> i64 { return 42; }"));
    EXPECT(write_text(entry_path,
                      "import \"meg_import_alias_library.meg\" as library; "
                      "fn start() -> i64 { return library.main(); }"));
    EXPECT(module_load(&graph, entry_path, sink));
    checker_init(&checker, sink);
    EXPECT(!checker_check(&checker, graph.program));
    EXPECT(checker.errors >= 2);
    checker_destroy(&checker);
    module_graph_destroy(&graph);

    (void)remove(entry_path);
    (void)remove(library_path);
    return RESULT();
}
