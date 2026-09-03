#include <meglang/ast.h>
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/diagnostic.h>
#include <meglang/parser.h>
#include <meglang/source.h>
#include <meglang/version.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum OutputMode
{
    OUTPUT_C,
    OUTPUT_AST
} OutputMode;
typedef struct Options
{
    const char *input;
    const char *output;
    OutputMode mode;
} Options;

static void usage(FILE *out, const char *program)
{
    fprintf(out, "usage: %s -i <source.meg> [-o output] [--emit c|ast]\n", program);
}

static bool options_parse(int argc, char **argv, Options *options)
{
    int i;
    *options = (Options){0};
    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0)
        {
            if (options->input || ++i == argc)
                return false;
            options->input = argv[i];
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
        {
            if (options->output || ++i == argc)
                return false;
            options->output = argv[i];
        }
        else if (strcmp(argv[i], "--emit") == 0)
        {
            if (++i == argc)
                return false;
            if (strcmp(argv[i], "c") == 0)
                options->mode = OUTPUT_C;
            else if (strcmp(argv[i], "ast") == 0)
                options->mode = OUTPUT_AST;
            else
                return false;
        }
        else
            return false;
    }
    return options->input != NULL;
}

static bool copy_stream(FILE *from, FILE *to)
{
    char buffer[4096];
    size_t count;
    rewind(from);
    while ((count = fread(buffer, 1, sizeof buffer, from)) != 0)
        if (fwrite(buffer, 1, count, to) != count)
            return false;
    return !ferror(from) && !ferror(to);
}

int main(int argc, char **argv)
{
    Options options;
    Source source = {0};
    ParseResult parsed = {0};
    Checker checker;
    bool checker_live = false;
    DiagnosticSink diagnostics = {diagnostic_print, stderr};
    FILE *temporary = NULL, *destination = stdout;
    int status = 1;
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        usage(stdout, argv[0]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0)
    {
        printf("meg 0.%d\n", meg_version());
        return 0;
    }
    if (!options_parse(argc, argv, &options))
    {
        usage(stderr, argv[0]);
        return 2;
    }
    if (!source_load(&source, options.input))
    {
        fprintf(stderr, "%s: cannot read input\n", options.input);
        goto done;
    }
    parsed = parse_source(&source, diagnostics);
    if (parsed.errors)
        goto done;
    if (options.mode == OUTPUT_C)
    {
        checker_init(&checker, diagnostics);
        checker_live = true;
        if (!checker_check(&checker, parsed.program))
            goto done;
    }
    temporary = tmpfile();
    if (!temporary)
    {
        fputs("meg: cannot create temporary output\n", stderr);
        goto done;
    }
    if (options.mode == OUTPUT_AST)
    {
        if (!ast_print(temporary, parsed.program))
            goto done;
    }
    else if (!codegen_c(temporary, parsed.program, diagnostics))
        goto done;
    if (options.output)
    {
        destination = fopen(options.output, "wb");
        if (!destination)
        {
            fprintf(stderr, "%s: cannot open output\n", options.output);
            goto done;
        }
    }
    if (!copy_stream(temporary, destination))
    {
        fputs("meg: cannot write output\n", stderr);
        goto done;
    }
    status = 0;
done:
    if (destination != stdout && fclose(destination) != 0)
        status = 1;
    if (temporary)
        fclose(temporary);
    if (checker_live)
        checker_destroy(&checker);
    program_destroy(parsed.program);
    source_destroy(&source);
    return status;
}
