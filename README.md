# Meghan Compiler

Meghan is a C11 compiler-construction project, also referred to as `meg`. It
provides source management, diagnostics, lexing, parsing, semantic checking,
and C code generation for the currently supported Meghan language forms. The
`meg` command-line compiler reads a `.meg` source file and emits C code or a
readable AST representation.

## Current Status

### Supported

- CMake 3.25 or newer.
- C11 compilation with compiler extensions disabled.
- Windows with the Visual Studio 2026 generator and x64 architecture.
- Linux with GCC and Unix Makefiles.
- A shared `meglang` library containing the current language implementation.
- The `meg` command-line executable.
- Source text loaded from memory or from a file.
- Source spans containing byte ranges, line numbers, and column numbers.
- Error and warning diagnostics delivered through a configurable callback.
- Lexing identifiers, integer literals, keywords, punctuation, operators,
  whitespace, and comments.
- Decimal, hexadecimal (`0x`), and binary (`0b`) integer literals.
- Line comments (`//`) and block comments (`/* ... */`).
- Parsing `main` functions with blocks, declarations, returns, conditionals,
  loops, and expressions.
- An AST for integer and boolean values, names, unary and binary expressions,
  and the supported statement forms.
- Semantic checking for `i64` and `bool`, variable lookup, nested scopes,
  assignments, conditions, operators, and guaranteed returns.
- C code generation for the currently supported expressions and statements.
- The `meg` command-line compiler, which reads a `.meg` source file and emits
	C code or an AST representation.
- Automated tests for each pipeline stage, including code generation.

### Not Yet Supported

- Generating machine code or object files.
- A stable Meghan language specification.
- Invoking a C compiler to turn generated C into an executable.
- Freestanding or native hosted backends.

The intended long-term pipeline is:

```text
[Meghan source] -> [tokens] -> [AST] -> [checked AST] -> [C code] -> [machine code]
```

## Quick Start

### Configure and build on Windows

From a Visual Studio developer command prompt:

```powershell
cmake --preset windows
cmake --build --preset windows-debug
```

The executable is written to `bin/Debug/meg.exe` and the shared library is
built alongside the configured targets.

### Configure and build on Linux

With GCC installed:

```bash
cmake --preset linux
cmake --build --preset linux
```

### Compile a Meghan source file to C

Create a source file such as `answer.meg`:

```meg
fn main() -> i64 {
	let answer: i64 = 40 + 2;
	return answer;
}
```

Run `meg` with an input file. C is the default output format and is written to
standard output unless `-o` or `--output` is supplied:

```powershell
.\bin\Debug\meg.exe -i answer.meg -o answer.c
```

The compiler parses and checks the source before writing C. A syntax or
semantic error produces diagnostics and no output file is written.

The command also supports `--emit ast` for a readable AST representation:

```powershell
.\bin\Debug\meg.exe --input answer.meg --emit ast
```

Use `--help` to print the command usage and `--version` to print the compiler
version:

```text
usage: meg -i <source.meg> [-o output] [--emit c|ast]
```

## Meghan Source Examples

The lexer can recognize the current token vocabulary in source text such as:

```meg
fn main() -> i64 {
	let answer: i64 = 0x2a;
	// The lexer skips this comment.
	return answer;
}
```

The currently recognized keywords are:

```text
fn let return if else while true false i64 bool
```

The lexer recognizes these operators and punctuation:

```text
( ) { } : ; + - * / % ! = == != < <= > >= ->
```

Integer literal examples include:

```meg
42
1_000_000
0x2a
0b101010
```

These examples demonstrate the current lexer, parser, checker, and C code
generator input. The compiler currently emits C source; use a C compiler to
produce an executable.

## Using the Language Library

The public headers are under `targets/meglang/include/meglang`. A client can
create source text, initialize a lexer, and consume tokens incrementally:

```c
#include <stdio.h>
#include <meglang/lexer.h>

Source source = {0};
Lexer lexer;
Token token;
DiagnosticSink diagnostics = {0};

if (!source_from_string(&source, "memory.meg", "fn main() 42"))
	return 1;

lexer_init(&lexer, &source, diagnostics);
do {
	token = lexer_next(&lexer);
} while (token.kind != TOKEN_EOF && token.kind != TOKEN_ERROR);

source_destroy(&source);
```

`Token` contains a `TokenKind` and a `SourceSpan`. Use `span_equals` when a
client needs to compare a token's source text, and `span_write` to write the
span bytes to a stream. `token_name` returns a display name for a token kind.

To receive lexer errors and warnings, provide a diagnostic callback:

```c
static void report(void *context, const Diagnostic *diagnostic)
{
	diagnostic_print(context, diagnostic);
}

DiagnosticSink diagnostics = {report, stderr};
```

For example, an unterminated block comment produces an error associated with
the comment's starting source span. An invalid integer literal produces an
error token and increments `lexer.errors`.

After parsing, a client can check the resulting program and generate C:

```c
#include <meglang/checker.h>
#include <meglang/codegen.h>
#include <meglang/parser.h>

ParseResult parsed = parse_source(&source, diagnostics);
Checker checker;

checker_init(&checker, diagnostics);
if (parsed.errors != 0 || !checker_check(&checker, parsed.program))
	return 1;

if (!codegen_c(stdout, parsed.program, diagnostics))
	return 1;

checker_destroy(&checker);
program_destroy(parsed.program);
```

The checker annotates expressions with their `ValueType` and resolves names to
symbols. Always destroy the checker before destroying the program, because the
checked AST stores references to the checker's symbols. `codegen_c` expects a
successfully checked program and writes C source to the supplied `FILE`.

## Testing

Run the tests after building:

### Windows Debug

```powershell
ctest --test-dir out/windows -C Debug --output-on-failure
```

### Windows Release

```powershell
cmake --build --preset windows-release
ctest --preset windows-release
```

### Linux

```bash
ctest --test-dir out/linux --output-on-failure
```

The test suite currently includes:

- `smoke`: verifies the basic build and library connection.
- `source_tests`: checks source ownership, spans, comparisons, and cleanup.
- `lexer_tests`: checks keywords, identifiers, integers, comments, operators,
  positions, and end-of-file handling.
- `ast_tests`: checks AST construction, printing, and cleanup.
- `parser_tests`: checks function, statement, and expression parsing.
- `checker_tests`: checks valid programs, symbol resolution, and semantic
	errors.
- `pipeline_tests`: checks parsing, semantic checking, and C generation in
  sequence.
- `cli_help`: verifies the command-line help path.

## Developer Guide

The repository is organized into these main areas:

```text
targets/meg/       The meg command-line executable
targets/meglang/   The public language library and its C implementation
tests/             CTest-based executable tests
bin/               Built runtime artifacts
out/               CMake build directories
```

The language library is compiled with warnings enabled (`/W4` on MSVC and
`-Wall -Wextra -Wpedantic` on GCC). New behavior should normally include a
focused test in `tests/` and be registered in `tests/CMakeLists.txt`.

The C generator consumes a checked AST and emits `int64_t` and `bool` C code
for the currently supported Meghan program forms. It uses internal symbol IDs
for generated variable names and helper functions that abort on invalid signed
division or remainder. Future work includes invoking a C compiler and adding
native backends while preserving source-span, type, symbol, and diagnostic
information.

## License

Meghan is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE)
for the complete license text.