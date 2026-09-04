# The Meghan Compiler: Characters Join the Pipeline

Hello again, this is Kenneth Looney, and I am back with the next part of my compiler journey. Episode 3 proved that string literals were clearly part of the roadmap, but there was still one missing piece in the middle: single-character values. I wanted **meg** to understand the tiny building blocks of text — the difference between an ordinary ASCII character, a UTF-8 character, and a UTF-16 character.

This time I added the character literal path all the way through the compiler, and it turned out to be an excellent way to tighten up the boundaries between lexing, parsing, type checking, and code generation.

## Why character literals matter so much

A string is a sequence, but a character is a single value. Once I started thinking in terms of a real language pipeline, it became obvious that the compiler needed to distinguish those two things clearly.

I wanted the lexer to recognize these forms:

```meg
'A'
u8'\xc3\xa9'
u'\xf0\x9f\x98\x80'
```

The plain form is restricted to ASCII. The `u8` form expects a valid UTF-8 code point. The `u` form expects a wide character value that eventually becomes a 32-bit code point in the compiler. That gave me a much more concrete way to test not just syntax, but also the encoding rules.

## How the lexer changed

The lexer now emits three distinct token kinds: `TOKEN_CHAR`, `TOKEN_UTF8_CHAR`, and `TOKEN_UCHAR`.

That was the first real sign that the text pipeline was becoming explicit. The scanner now validates the literal shape before handing it off. It rejects empty literals, rejects multi-character forms, rejects plain non-ASCII bytes in an ASCII literal, and validates malformed UTF-8 sequences instead of letting them slip through.

I also had to be careful about escape sequences. A character literal can include a simple escape like `\n`, but it cannot contain more than one character in the final value. That boundary matters because the parser will later decode the literal into its actual numeric value.

Once the scanner had all of that in place, the compiler could finally distinguish the three character encodings at the token level rather than treating them as a vague blob of text.

## What changed in the parser and AST?

The parser now turns those token kinds into expression nodes. I added `EXPR_CHAR`, `EXPR_UTF8_CHAR`, and `EXPR_UCHAR`, and each one stores both the original source span and a decoded numeric value.

That means the AST is no longer just a place where text gets remembered. It can keep enough information to know exactly what the source literal meant. An ASCII literal like `'A'` becomes `65`. A UTF-8 literal like `u8'\xc3\xa9'` becomes `233`. A wide literal like `u'\xf0\x9f\x98\x80'` becomes `128512`.

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

## What went wrong

The hardest part was not the parser. It was the validation rules.

The scanner had to reject malformed UTF-8 rather than accepting any high-bit byte, and it had to keep plain character literals from silently accepting non-ASCII values. That was one of those places where a tiny bug can hide in plain sight, because a lot of text looks valid if you only look at the first byte and not the whole sequence.

I also had to make sure the code did not accidentally treat a UTF-8 or UTF-16 character as a normal string. They are not the same thing. They look related, but they live in different places in the compiler pipeline and they need different type rules.

## What is next?

The next step is to let these character types participate in declarations and function signatures so they are not limited to literal expressions. Once `char`, `utf8_char`, and `uchar` can appear in variable and return types, I can start building the rest of the text story around them.

That should lead naturally into the next big decision: what a real Meghan string should look like in memory and how both C and JavaScript should represent it. For now, though, I have a much more solid foundation. ASCII, UTF-8, and UTF-16 character literals are all recognized, validated, checked, and generated with real semantics behind them.

Thank you for joining me for this part of the journey. I will see you very soon!
