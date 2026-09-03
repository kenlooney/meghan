# Meghan Compiler

Meghan is a compiler-construction project written in C. The compiler is also
referred to as `meg`.

The project is at an early stage. It currently provides a C11 language library
with source management, diagnostics, token definitions, and a lexer. The
`meg` command-line program is a small executable foundation and currently
supports only help and version information.

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
- Automated tests for the project, source, lexer, and CLI foundation.

### Not Yet Supported

- Parsing tokens into an abstract syntax tree.
- Type checking or semantic analysis.
- Transpiling Meghan source into C.
- Generating machine code or object files.
- Compiling a `.meg` source file from the command line.
- A stable Meghan language specification.
- A complete diagnostic command-line experience.
- Freestanding or native hosted backends.

The intended long-term pipeline is:

```text
[Meghan source] -> [tokens] -> [parsed program] -> [C or native backend]
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

### Run the command-line program

The current executable accepts one supported option at a time:

```text
meg --help
meg --version
```

Example output:

```text
usage: meg --help | --version
meg 0.1
```

Any other argument currently prints the usage message to standard error and
returns a non-zero exit status. The executable does not read or compile
source files yet.

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

These examples demonstrate lexer input only. They are not executable Meghan
programs yet because parsing, type checking, and code generation are not
implemented.

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

The next major implementation boundary is the parser: it will consume the
tokens produced by `lexer_next` and create a representation of a Meghan
program. Until then, changes should preserve the source-span and diagnostic
information needed by later compiler stages.

## License

Meghan is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE)
for the complete license text.