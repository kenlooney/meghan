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

#include "meglang/diagnostic.h"

#include <stdio.h>

void diagnostic_emit(DiagnosticSink sink, DiagnosticLevel level,
                     SourceSpan span, const char *message)
{
    Diagnostic diagnostic = {level, span, message};
    if (sink.emit)
        sink.emit(sink.context, &diagnostic);
}

void diagnostic_print(void *context, const Diagnostic *diagnostic)
{
    FILE *out = context ? context : stderr;
    const char *path = "<source>";
    const char *level = "error";
    if (!diagnostic)
        return;
    if (diagnostic->span.source && diagnostic->span.source->path)
        path = diagnostic->span.source->path;
    if (diagnostic->level == DIAGNOSTIC_WARNING)
        level = "warning";
    fprintf(out, "%s:%u:%u: %s: %s\n", path,
            diagnostic->span.line, diagnostic->span.column,
            level, diagnostic->message ? diagnostic->message : "diagnostic");
}