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

#ifndef MEGLANG_LEXER_H
#define MEGLANG_LEXER_H

#include <stddef.h>

#include <meglang/diagnostic.h>
#include <meglang/source.h>
#include <meglang/token.h>

typedef struct Lexer {
    const Source *source;
    DiagnosticSink diagnostics;
    size_t current;
    unsigned line;
    unsigned column;
    unsigned errors;
} Lexer;

void lexer_init(Lexer *lexer, const Source *source,
                DiagnosticSink diagnostics);
Token lexer_next(Lexer *lexer);

#endif // MEGLANG_LEXER_H