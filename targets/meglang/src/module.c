// Copyright 2026 Kenneth Looney
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <meglang/module.h>
#include <meglang/parser.h>
#include <meglang/source.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum LoadState { LOAD_IN_PROGRESS, LOAD_COMPLETE } LoadState;

struct ModuleFile {
    Source source;
    LoadState state;
    ModuleFile *next;
};

static char *copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy) memcpy(copy, text, length + 1);
    return copy;
}

static char *copy_span(SourceSpan span)
{
    char *copy;
    if (!span_valid(span)) return NULL;
    copy = malloc(span.length + 1);
    if (!copy) return NULL;
    memcpy(copy, span.source->text + span.start, span.length);
    copy[span.length] = '\0';
    return copy;
}

static bool separator(char c) { return c == '/' || c == '\\'; }

static char *normalize_path(const char *path)
{
    size_t length, read = 0, count = 0, i, output_length = 0;
    bool rooted = false;
    char *work, *output;
    char **segments;
    const char *prefix = "";
    size_t prefix_length = 0;
    if (!path) return NULL;
    length = strlen(path);
    work = copy_text(path);
    segments = malloc((length + 1) * sizeof *segments);
    if (!work || !segments) { free(work); free(segments); return NULL; }
    for (i = 0; i < length; ++i) if (work[i] == '\\') work[i] = '/';
    if (length >= 2 && work[0] == '/' && work[1] == '/') {
        prefix = "//"; prefix_length = 2; rooted = true; read = 2;
        while (work[read] == '/') ++read;
    } else if (length >= 2 && isalpha((unsigned char)work[0]) && work[1] == ':') {
        prefix = work; prefix_length = 2; read = 2;
        if (work[read] == '/') { rooted = true; ++prefix_length; ++read; }
    } else if (work[0] == '/') {
        prefix = "/"; prefix_length = 1; rooted = true; read = 1;
        while (work[read] == '/') ++read;
    }
    while (read < length) {
        char *segment;
        while (work[read] == '/') ++read;
        if (read >= length) break;
        segment = work + read;
        while (read < length && work[read] != '/') ++read;
        if (read < length) work[read++] = '\0';
        if (strcmp(segment, ".") == 0) continue;
        if (strcmp(segment, "..") == 0) {
            if (count && strcmp(segments[count - 1], "..") != 0) {
                --count;
                continue;
            }
            if (rooted) continue;
        }
        segments[count++] = segment;
    }
    output_length = prefix_length;
    for (i = 0; i < count; ++i)
        output_length += strlen(segments[i]) + (i || prefix_length ? 1 : 0);
    if (!count && !prefix_length) output_length = 1;
    output = malloc(output_length + 1);
    if (!output) { free(segments); free(work); return NULL; }
    output[0] = '\0';
    if (prefix_length) {
        memcpy(output, prefix, prefix_length);
        output[prefix_length] = '\0';
    }
    for (i = 0; i < count; ++i) {
        size_t used = strlen(output);
        if (used && output[used - 1] != '/') output[used++] = '/';
        memcpy(output + used, segments[i], strlen(segments[i]) + 1);
    }
    if (!count && !prefix_length) strcpy(output, ".");
    free(segments);
    free(work);
    return output;
}

static bool absolute_path(const char *path)
{
    return path && (separator(path[0]) ||
           (isalpha((unsigned char)path[0]) && path[1] == ':'));
}

static char *resolve_path(const char *importer, const char *imported)
{
    const char *slash;
    char *joined, *normalized;
    size_t directory_length, imported_length;
    if (absolute_path(imported)) return normalize_path(imported);
    slash = strrchr(importer, '/');
    directory_length = slash ? (size_t)(slash - importer + 1) : 0;
    imported_length = strlen(imported);
    joined = malloc(directory_length + imported_length + 1);
    if (!joined) return NULL;
    memcpy(joined, importer, directory_length);
    memcpy(joined + directory_length, imported, imported_length + 1);
    normalized = normalize_path(joined);
    free(joined);
    return normalized;
}

static ModuleFile *find_file(ModuleGraph *graph, const char *path)
{
    ModuleFile *file;
    for (file = graph->files; file; file = file->next)
        if (strcmp(file->source.path, path) == 0) return file;
    return NULL;
}

static void append_functions(Program *program, Function *functions)
{
    Function **tail = &program->functions;
    while (*tail) tail = &(*tail)->next;
    *tail = functions;
}

static void append_imports(Program *program, Import *imports)
{
    Import **tail = &program->imports;
    while (*tail) tail = &(*tail)->next;
    *tail = imports;
}

static void report(ModuleGraph *graph, DiagnosticSink diagnostics,
                   SourceSpan span, const char *message)
{
    diagnostic_emit(diagnostics, DIAGNOSTIC_ERROR, span, message);
    ++graph->errors;
}

static bool load_file(ModuleGraph *graph, const char *path, SourceSpan import_span,
                      DiagnosticSink diagnostics, bool entry)
{
    ModuleFile *file = find_file(graph, path);
    ParseResult parsed;
    Import *import;
    if (file) {
        if (file->state == LOAD_IN_PROGRESS) {
            report(graph, diagnostics, import_span, "import cycle detected");
            return false;
        }
        return true;
    }
    file = calloc(1, sizeof *file);
    if (!file) {
        report(graph, diagnostics, import_span, "cannot allocate module");
        return false;
    }
    if (!source_load(&file->source, path)) {
        if (entry) {
            Source temporary = {(char *)path, (char *)"", 0};
            SourceSpan span = {&temporary, 0, 0, 1, 1};
            report(graph, diagnostics, span, "cannot read entry source");
        } else {
            report(graph, diagnostics, import_span, "cannot read imported source");
        }
        free(file);
        return false;
    }
    file->state = LOAD_IN_PROGRESS;
    file->next = graph->files;
    graph->files = file;
    if (entry) graph->program->source = &file->source;
    parsed = parse_source(&file->source, diagnostics);
    graph->errors += parsed.errors;
    for (import = parsed.program->imports; import; import = import->next) {
        if (!import->path.length) continue;
        char *written = copy_span(import->path);
        char *resolved = NULL;
        if (written && absolute_path(written)) {
            report(graph, diagnostics, import->span,
                   "import path must be relative");
        } else if (written) {
            resolved = resolve_path(file->source.path, written);
        }
        if (!written || (written && !absolute_path(written) && !resolved)) {
            report(graph, diagnostics, import->span, "cannot allocate import path");
        } else if (resolved) {
            if (load_file(graph, resolved, import->span, diagnostics, false)) {
                ModuleFile *imported = find_file(graph, resolved);
                if (imported) import->target = &imported->source;
            }
        }
        free(resolved);
        free(written);
    }
    append_functions(graph->program, parsed.program->functions);
    append_imports(graph->program, parsed.program->imports);
    parsed.program->functions = NULL;
    parsed.program->imports = NULL;
    program_destroy(parsed.program);
    file->state = LOAD_COMPLETE;
    return true;
}

bool module_load(ModuleGraph *graph, const char *entry_path,
                 DiagnosticSink diagnostics)
{
    char *normalized;
    bool loaded;
    if (!graph || !entry_path) return false;
    *graph = (ModuleGraph){0};
    graph->program = calloc(1, sizeof *graph->program);
    normalized = normalize_path(entry_path);
    if (!graph->program || !normalized) {
        free(normalized);
        module_graph_destroy(graph);
        return false;
    }
    loaded = load_file(graph, normalized, (SourceSpan){0}, diagnostics, true);
    free(normalized);
    return loaded && graph->errors == 0;
}

void module_graph_destroy(ModuleGraph *graph)
{
    ModuleFile *file;
    if (!graph) return;
    program_destroy(graph->program);
    file = graph->files;
    while (file) {
        ModuleFile *next = file->next;
        source_destroy(&file->source);
        free(file);
        file = next;
    }
    *graph = (ModuleGraph){0};
}
