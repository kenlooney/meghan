# Meghan Compiler

Meghan is a C11 compiler-construction project, also referred to as `meg`. It
provides source management, diagnostics, lexing, parsing, semantic checking,
and C and JavaScript code generation for the currently supported Meghan
language forms. The `meg` command-line compiler reads a `.meg` source file and
emits hosted C code, freestanding-compatible C code, JavaScript, or a readable
AST representation.

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
- Lexing ASCII, UTF-8-prefixed (`u8`), and UTF-16-prefixed (`u`) character
	and string literals, including escaped quotes and UTF-8 validation.
- Parsing multiple functions with typed parameters, blocks, declarations,
	returns, conditionals, `while` and `for` loops, expressions, and calls.
- An AST for functions, parameters, call arguments, integer, boolean, character,
	and string literals, names, unary and binary expressions, and the supported
	statement forms.
- Semantic checking for `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`,
	`u64`, `bool`, `char`, `utf8_char`, and `uchar`; variable lookup; nested
	scopes; assignments; conditions; operators; loop and parameter scopes; call
	arity and argument types; string and character expression types; literal
	ranges; and guaranteed returns.
- C and JavaScript code generation for typed function signatures, calls, and
	the currently supported expressions and statements.
- Freestanding-compatible C output with a `meg_entry` entry point and an
	external `meg_panic` trap hook.
- Function symbol resolution, forward calls, typed arguments, boolean return
	types, and all supported signed and unsigned integer widths.
- Recursive relative imports, import-cycle detection, module aliases, qualified
	function calls, and entry-file ownership of `main`.
- Automated tests for each pipeline stage, including code generation.

## Pointers and References

Meghan supports typed pointers and references to fixed-width integer values.
Use `*` in a declaration for a pointer, `ref` for a reference, `&` to take an
address, and unary `*` to read or write through either form:

```meg
fn main() -> i64 {
	let value: i64 = 40;
	let pointer: *i64 = &value;
	*pointer = *pointer + 1;
	return value;
}
```

References use the same dereference syntax:

```meg
let value: i64 = 40;
let reference: ref i64 = ref value;
*reference = *reference + 2;
```

Address-taking and references require an assignable value, and pointer or
reference types must match their underlying value type exactly. For example,
an `*i16` cannot be assigned to an `*i8`. The checker also rejects
dereferencing a non-pointer value and assignments whose types do not match.

The C generator emits native typed pointers such as `int64_t *`. The
JavaScript generator models references with objects that provide `get` and
`set` operations, applying the same integer-width normalization used by other
assignments. The current implementation does not include heap allocation,
pointer arithmetic, null pointers, or a complete memory model.

### Not Yet Supported

- Generating machine code or object files.
- A stable Meghan language specification.
- Invoking a C compiler to turn generated C into an executable.
- Native freestanding or hosted backends, including platform startup code.
- Qualified module variables, types, and other exported declarations.

The current source-generation pipeline is:

```text
[Meghan source] -> [tokens] -> [AST] -> [checked AST]
	-> [hosted C, freestanding C, or JavaScript]
```

Hosted C or freestanding-compatible C can then be passed to a C toolchain, and
JavaScript can be passed to Node.js or another JavaScript runtime. Native
Meghan backends remain a long-term goal.

## Functions and Calls

Meghan programs can contain multiple functions with typed parameters. Each
function has its own return type and body, and a function can call another
function even if that function is declared later in the source:

```meg
fn main() -> i64 {
	return add(40, 2);
}

fn add(left: i64, right: i64) -> i64 {
	return left + right;
}
```

The semantic checker collects function declarations before checking their
bodies. It resolves calls, rejects unknown or duplicate functions, and checks
argument counts and types. Parameters support value, pointer, and reference
types. Boolean-returning functions can be used as conditions, and function
types preserve all supported signed and unsigned integer widths.

Arguments must exactly match their declared parameter types. Meghan does not
currently perform implicit integer widening, narrowing, or signedness
conversion at a call boundary. Parameter names share the function body's outer
scope, so duplicate parameter names and conflicting variable declarations at
the top of that body are rejected.

The C generator emits internal names such as `meg_main` and `meg_f_1`, while
the JavaScript generator emits corresponding generated functions. Hosted C
still receives a normal `main` wrapper, and freestanding-compatible C retains
its platform-facing entry-point contract.

The `main` entry function does not accept parameters yet. Hosted C can provide
an argument count and argument strings, but Meghan does not yet have the string,
slice, or nested-pointer types needed to expose that interface.

See [`examples/parameters.meg`](examples/parameters.meg) for value and pointer
parameters used in a complete program.

## Modules and Imports

Meghan source files can import other source files with a module alias. Import
paths are resolved relative to the file containing the import:

```meg
import "math.meg" as math;

fn main() -> i64 {
	return math.add(40, 2);
}
```

The alias qualifies calls into that module using `alias.function()`. An
unqualified call resolves only within the source file containing the call, so
different modules may safely declare functions with the same name. Imported
modules can have their own imports, and the loader detects import cycles and
loads repeated paths only once.

Only the entry file may declare `main`. A function named `main` in an imported
library is rejected and cannot accidentally become the program entry point.
Imports are resolved at compile time; C, freestanding C, and JavaScript output
contain one combined program whose functions use unique generated names.

The complete example under [`examples/modules/`](examples/modules/) includes a
transitive import and two separate modules that both declare `value()`:

```powershell
.\bin\Debug\meg.exe -i examples\modules\main.meg -o modules.c
.\bin\Debug\meg.exe -i examples\modules\main.meg --emit js -o modules.js
```

Aliases currently qualify function calls only. Qualified variables and types
are not yet part of the language.

## Character and String Literals

The lexer recognizes both character and string forms:

```meg
'A'
u8'é'
u'😀'
"plain ASCII"
u8"café"
u"snowman: ☃"
```

Plain ASCII characters and strings are restricted to ASCII. The `u8` prefix
identifies a UTF-8 character or string, while the `u` prefix identifies a
UTF-16 character or string. The lexer accepts escaped quotes and validates the
UTF-8 byte sequence in prefixed forms. Malformed UTF-8, non-ASCII bytes in an
unprefixed literal, and unterminated literals produce diagnostics and error
symbols.

Character literals continue through the parser into `EXPR_CHAR`,
`EXPR_UTF8_CHAR`, and `EXPR_UCHAR` AST nodes, while string literals become
`EXPR_STRING` nodes with their encoding metadata. These are checked as
`TYPE_CHAR`, `TYPE_UTF8_CHAR`, `TYPE_UCHAR`, `TYPE_STRING`, and `TYPE_USTRING`
respectively. The AST keeps the original literal span so the printer can
reproduce the source spelling. Character expressions also store their decoded
Unicode scalar value for portable code generation.

Character types can be used in declarations, parameters, and function return
types. For example:

```meg
fn utf8_character() -> utf8_char {
	return u8'é';
}

fn unicode_character() -> uchar {
	return u'😀';
}
```

The C generator represents `char` as `uint8_t` and both Unicode character
types as `uint32_t`. It emits decoded numeric constants instead of relying on
target-specific character-literal syntax. The JavaScript generator emits the
same scalar values as numbers.

Characters and strings are not interchangeable with integer values. Expressions
such as `"hello" + 1` and other mismatched arithmetic patterns are rejected by
the checker. Declared `string` and `ustring` variables, string initializers,
and C or JavaScript string generation are not supported yet, but the literal
path itself is now implemented and validated across the compiler pipeline.

See [`examples/characters.meg`](examples/characters.meg) for a complete program
that calls functions returning all three character types and validates their
results.

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

Use `--emit js` to generate JavaScript. The generated program uses `BigInt`
for Meghan integer values and can be run with Node.js:

```powershell
.\bin\Debug\meg.exe --input answer.meg --emit js -o answer.js
node answer.js
```

Use `--emit freestanding` to generate C without a hosted `main` wrapper or a
dependency on `<stdlib.h>`:

```powershell
.\bin\Debug\meg.exe --input answer.meg --emit freestanding -o answer.c
```

The generated source exposes `meg_entry` for platform-specific startup code
and declares an external `meg_panic` function for invalid arithmetic traps. It
is freestanding-compatible C output, not a complete native backend; the
target environment must provide startup code, `meg_panic`, and any required
toolchain or linker configuration.

Use `--help` to print the command usage and `--version` to print the compiler
version:

```text
usage: meg -i <source.meg> [-o output] [--emit c|freestanding|js|ast]
```

## Meghan Source Examples

Complete examples are available for [modules](examples/modules/),
[characters](examples/characters.meg),
[functions](examples/functions.meg),
[parameters](examples/parameters.meg),
[pointers](examples/raw-pointer.meg), and
[references](examples/reference.meg).

The lexer can recognize the current token vocabulary in source text such as:

```meg
fn main() -> u64 {
	let sum: u64 = 0;
	for (let i: u8 = 0; i < 4; i = i + 1) {
		sum = sum + i;
	}
	return sum;
}
```

## Integer Types and Checking

Meghan currently supports the following fixed-width integer types:

```text
Signed:   i8  i16  i32  i64
Unsigned: u8  u16  u32  u64
```

Integer literals are parsed as unsigned 64-bit values, including decimal,
hexadecimal, and binary forms. During semantic checking, a literal must fit
the type declared for an initializer or expected by an expression. A negative
literal is permitted only for a signed integer type and is checked against its
negative bound.

Integer arithmetic, assignment, comparison, and return expressions require
matching integer types. Meghan does not currently perform implicit integer
conversions or support mixed-width arithmetic. Conditions and logical negation
require `bool`.

The currently recognized keywords are:

```text
fn let return if else while for true false ref import as i8 i16 i32 i64 u8 u16 u32 u64 bool
```

The lexer recognizes these operators and punctuation:

```text
( ) { } : , ; . + - * / % ! & = == != < <= > >= ->
```

Integer literal examples include:

```meg
42
1_000_000
0x2a
0b101010
```

These examples demonstrate the current lexer, parser, checker, and source
generators. Use a C compiler for generated C or Node.js for generated
JavaScript to produce a running program.

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

JavaScript output uses the same checked AST and is available through the
parallel API:

```c
#include <meglang/codegen_js.h>

if (!codegen_js(stdout, parsed.program, diagnostics))
	return 1;
```

`codegen_c` maps Meghan integer types to the corresponding C fixed-width types
from `<stdint.h>`. It emits helpers that reject division by zero and the
signed 64-bit minimum-value division or remainder edge case.

Freestanding-compatible C output uses the same checked AST through a separate
API:

```c
if (!codegen_freestanding_c(stdout, parsed.program, diagnostics))
	return 1;
```

`codegen_freestanding_c` emits `meg_entry` instead of a hosted `main` wrapper,
omits `<stdlib.h>`, and routes invalid division and remainder operations to an
external `meg_panic` hook. The caller's target environment is responsible for
providing that hook and the platform startup code.

`codegen_js` writes JavaScript using `BigInt` for integer values. It narrows
unsigned arithmetic to the declared width and reports signed arithmetic
overflow. Values are normalized again when assigned, initialized, or returned.

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
	positions, integer type keywords, string literals, UTF-8 validation, and
	end-of-file handling.
- `ast_tests`: checks AST construction, printing, and cleanup.
- `parser_tests`: checks function, statement, and expression parsing.
- `checker_tests`: checks valid programs, symbol resolution, integer literal
	ranges, signed minimum values, unsigned types, string expression types,
	integer-only arithmetic, and semantic errors.
- `pipeline_tests`: checks parsing, semantic checking, and C and JavaScript
	generation in sequence.
- `for_loops`: checks valid and invalid `for` loop parsing, checking, and
	scoping.
- `freestanding_tests`: checks freestanding C entry-point and trap-hook
	generation without hosted runtime dependencies.
- `pointer_tests`: checks pointer and reference typing, dereferencing,
	assignment, and C and JavaScript generation.
- `function_tests`: checks multiple functions, forward calls, return types,
	unknown-function errors, and C and JavaScript generation.
- `parameter_tests`: checks value, pointer, and reference parameters; argument
	counts and types; parameter scopes; AST output; and both generators.
- `character_tests`: checks ASCII, UTF-8, and Unicode character parsing,
	typing, function returns, and C and JavaScript generation.
- `import_alias_tests`: checks alias parsing and AST output, recursive module
	loading, source-qualified function lookup, entry ownership, and both source
	generators.
- `cli_help`: verifies the command-line help path.

## Developer Guide

The repository is organized into these main areas:

```text
targets/meg/       The meg command-line executable
targets/meglang/   The public language library and its C implementation
tests/             CTest-based executable tests
examples/          Complete Meghan programs demonstrating supported features
bin/               Built runtime artifacts
out/               CMake build directories
```

The language library is compiled with warnings enabled (`/W4` on MSVC and
`-Wall -Wextra -Wpedantic` on GCC). New behavior should normally include a
focused test in `tests/` and be registered in `tests/CMakeLists.txt`.

The C and JavaScript generators consume a checked AST and emit the currently
supported Meghan program forms. Hosted C uses a normal `main` wrapper, while
freestanding-compatible C exposes `meg_entry` and an external `meg_panic` hook.
Both C modes use internal symbol IDs for generated variable names and helper
functions that reject invalid signed division or remainder. Future work
includes invoking target-language compilers, adding platform startup code, and
building native backends. Pointer and reference support operates on typed local
values and parameters. Future work must preserve source-span, type, symbol, and
diagnostic information as these models grow.

## License

Meghan is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE)
for the complete license text.
