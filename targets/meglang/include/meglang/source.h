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

#ifndef MEGLANG_SOURCE_H
#define MEGLANG_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// MegLang source file representation
typedef struct Source {
    char *path;
    char *text;
    size_t length;
} Source;

typedef struct SourceSpan {
    const Source *source;
    size_t start;
    size_t length;
    unsigned line;
    unsigned column;
} SourceSpan;

bool source_from_string(Source *out, const char *path, const char *text);
bool source_load(Source *out, const char *path);
void source_destroy(Source *source);
bool span_valid(SourceSpan span);
bool span_equals(SourceSpan span, const char *text);
bool span_write(FILE *out, SourceSpan span);

#endif // MEGLANG_SOURCE_H
