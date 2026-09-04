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

#ifndef MEGLANG_MODULE_H
#define MEGLANG_MODULE_H

#include <stdbool.h>

#include <meglang/ast.h>
#include <meglang/diagnostic.h>

typedef struct ModuleFile ModuleFile;
typedef struct ModuleGraph {
    Program *program;
    ModuleFile *files;
    unsigned errors;
} ModuleGraph;

bool module_load(ModuleGraph *graph, const char *entry_path,
                 DiagnosticSink diagnostics);
void module_graph_destroy(ModuleGraph *graph);

#endif // MEGLANG_MODULE_H