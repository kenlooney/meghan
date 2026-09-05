# The Meghan Compiler: Characters Join the Pipeline

Hello again, this is Kenneth Looney, and I am back with the next part of my compiler journey. Episode 3 proved that string literals were clearly part of the roadmap, but there was still one missing piece in the middle: single-character values. I wanted **meg** to understand the tiny building blocks of text — the difference between an ordinary ASCII character, a UTF-8 character, and a UTF-16 character.

This time I added the character literal path all the way through the compiler, and it turned out to be an excellent way to tighten up the boundaries between lexing, parsing, type checking, and code generation.

## Why character literals matter so much

A string is a sequence, but a character is a single value. Once I started thinking in terms of a real language pipeline, it became obvious that the compiler needed to distinguish those two things clearly.

I wanted the lexer to recognize these forms:

```meg
'A'
u8'é'
u'😀'
```

The plain form is restricted to ASCII. The `u8` form expects a valid UTF-8 code point. The `u` form expects a wide character value that eventually becomes a 32-bit code point in the compiler. That gave me a much more concrete way to test not just syntax, but also the encoding rules.

## How the lexer changed

The lexer now emits three distinct token kinds: `TOKEN_CHAR`, `TOKEN_UTF8_CHAR`, and `TOKEN_UCHAR`.

That was the first real sign that the text pipeline was becoming explicit. The scanner now validates the literal shape before handing it off. It rejects empty literals, rejects multi-character forms, rejects plain non-ASCII bytes in an ASCII literal, and validates malformed UTF-8 sequences instead of letting them slip through.

I also had to be careful about escape sequences. A character literal can include a simple escape like `\n`, but it cannot contain more than one character in the final value. That boundary matters because the parser will later decode the literal into its actual numeric value.

Once the scanner had all of that in place, the compiler could finally distinguish the three character encodings at the token level rather than treating them as a vague blob of text.

## What changed in the parser and AST?

The parser now turns those token kinds into expression nodes. I added `EXPR_CHAR`, `EXPR_UTF8_CHAR`, and `EXPR_UCHAR`, and each one stores both the original source span and a decoded numeric value.

That means the AST is no longer just a place where text gets remembered. It can keep enough information to know exactly what the source literal meant. An ASCII literal like `'A'` becomes `65`. A UTF-8 literal like `u8'é'` becomes `233`. A wide literal like `u'😀'` becomes `128512`.

This is a nice example of the boundary I want in the compiler: the lexer decides what the source text is, the parser decides what the expression shape is, and the AST stores the value in a way that the checker and code generators can use.

## How the checker sees them

The semantic checker now assigns value types for each literal kind:

- `char` becomes `TYPE_CHAR`
- `u8'...'` becomes `TYPE_UTF8_CHAR`
- `u'...'` becomes `TYPE_UCHAR`

That was an important step because the compiler can now tell the difference between plain character text and wider Unicode characters before it ever reaches target code generation. It is no longer guessing based on spelling alone.

The checker also makes use of those types when it decides whether an expression is valid in a given context. Character literals are value expressions, not strings, and they are not integers in the same sense as numeric types. That separation keeps the pipeline more honest.

## What the code generators do now

The C backend emits numeric constants for these literals. A plain ASCII character becomes a small `uint8_t`, while UTF-8 and UTF-16 characters turn into `uint32_t` values. That is a good fit for the project right now because the compiler is still focused on correctness and representation before it tries to design a full runtime string model.

The JavaScript backend does the same kind of thing in a simpler form: it emits the numeric value directly. The generated output is still a long way from a proper text runtime, but the literal path is now real and testable.

## Can a function return a character now?

Yes! This was the final connection that made the new types feel like part of the language instead of isolated literals.

The parser now accepts `char`, `utf8_char`, and `uchar` in declarations, parameters, and function return types. That lets me write small functions like these:

```meg
fn ascii_character() -> char {
	return 'A';
}

fn utf8_character() -> utf8_char {
	return u8'é';
}

fn unicode_character() -> uchar {
	return u'😀';
}
```

The checker makes sure each returned value matches the declared character type. Then the C and JavaScript backends preserve the decoded value when one function calls another.

I added a complete example that calls all three functions, stores their results, and compares them with the expected literals. It is a small program, but it proves that the feature travels through the whole pipeline instead of stopping at the lexer.

## What changed when modules arrived

I also added the module loader. That part changed the project in a very visible way because it meant the compiler was no longer only reading one file at a time.

A Meghan source file can now import another source module using an alias:

```meg
import "math.meg" as math;

fn main() -> i64 {
	return math.add(40, 2);
}
```

The loader resolves import paths relative to the file that contains the import, detects import cycles, and loads repeated modules only once. That means a program can spread across multiple files without every module being re-read over and over again.

The nice part is that the alias is reflected in the AST and the checker. A call like `math.add()` is qualified at the expression level, so functions in different modules can share the same name without colliding. The compiler also enforces one important rule: only the entry file may declare `main`. If an imported module tries to define `main`, that is rejected instead of allowing a surprising program entry point.

This was a big step toward making the compiler feel like a real language tool rather than a single-file toy. It also gave me a more concrete reason to keep the pipeline disciplined: imported modules are loaded, parsed, checked, and flattened into a single combined program before code generation.

## What went wrong

The hardest part was not the parser. It was the validation rules.

The scanner had to reject malformed UTF-8 rather than accepting any high-bit byte, and it had to keep plain character literals from silently accepting non-ASCII values. That was one of those places where a tiny bug can hide in plain sight, because a lot of text looks valid if you only look at the first byte and not the whole sequence.

I also had to make sure the code did not accidentally treat a UTF-8 or UTF-16 character as a normal string. They are not the same thing. They look related, but they live in different places in the compiler pipeline and they need different type rules.

The module loader added a different category of problems too. Relative paths had to be normalized correctly, the import graph had to avoid cycles, and the checker needed to understand qualified calls without letting namespaces leak into places they did not belong. There is a real difference between parsing an import and making that import mean something.

## What is next?

The next big decision is what a real Meghan string should look like in memory and how both C and JavaScript should represent it. A single character can live as a decoded numeric value, but a string needs storage, a length, an encoding, and clear ownership rules.

After that, I will keep pushing on the module system too. Qualified variables and types are still not part of the language, and I would like to see what the import model looks like once the language starts exposing more than functions and values across files.

For now, though, I have a much more solid foundation. ASCII, UTF-8, and UTF-16 character literals can move through variables and function calls, and the compiler can now load and combine multiple source modules into a single program with safe aliases and guarded entry points.

Thank you for joining me for this part of the journey. I will see you very soon!
