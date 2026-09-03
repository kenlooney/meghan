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

#ifndef MEGLANG_CHECKER_H
#define MEGLANG_CHECKER_H

#include <stdbool.h>

#include <meglang/ast.h>
#include <meglang/diagnostic.h>

struct Symbol {
    SourceSpan name;
    ValueType type;
    unsigned id;
    struct Symbol *next_in_scope;
    struct Symbol *next_allocated;
};

typedef struct Scope Scope;
typedef struct Checker {
    DiagnosticSink diagnostics;
    Scope *scope;
    Symbol *allocated;
    unsigned next_id;
    unsigned errors;
} Checker;

void checker_init(Checker *checker, DiagnosticSink diagnostics);
bool checker_check(Checker *checker, Program *program);
void checker_destroy(Checker *checker);
const char *value_type_name(ValueType type);

#endif // MEGLANG_CHECKER_H