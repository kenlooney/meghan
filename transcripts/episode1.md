# The Meghan Compiler: From First Token to Two Backends

Hello again, this is Kenneth Looney, and this is the first real episode in my journey of building **Meghan**, also called the **meg** compiler.

In the intro I said I was going to begin with project setup, write the first *tokenizer*, and maybe do more. Well, that “maybe more” turned into quite a journey! What began as an empty compiler project now reads Meghan source, checks whether it makes sense, and turns it into either C or JavaScript.

There is a lot between those two points. I had to learn how a compiler remembers source locations, how it separates syntax from meaning, how it handles scopes and types, and why generating code for two different languages is about much more than printing different words.

So let me start where **meg** started: with a project that could build, but could not understand a single line of Meghan code yet.

## Where do I even start?

The first step was getting a real project in place. I set up **meg** with CMake and C11, separated the compiler executable from the language library, and added a test setup so I could check every new piece as I built it.

The project builds on both Windows and Ubuntu. That might sound like a small milestone compared with parsing a language or generating code, but it gives everything else a dependable foundation. I do not want to discover much later that I accidentally tied the whole compiler to one machine or one development environment.

The command-line program started very small. It could show help and version information, but it could not compile Meghan source. Even so, that gave the project a real executable to grow into instead of leaving it as a loose collection of experiments.

I also created a source module. Before the compiler can understand a program, it needs to know where that program came from. A source can hold text loaded from a file or supplied directly from memory, together with its path and length.

Then I added *source spans*. A span describes a particular piece of source text using its starting position, length, line, and column. This is what lets the compiler eventually say where an error happened instead of only announcing that something went wrong somewhere.

The diagnostic system sits beside that source tracking. It can send errors and warnings through a diagnostic sink and print the source location with the message. Even before **meg** understands the language, the source code is beginning to carry its own map.

That map turns out to matter in nearly every stage that follows.

## How can the compiler recognize source code?

The first big language step was the *lexer*, also called a *tokenizer*. Its job is to read raw characters and group them into tokens that the rest of the compiler can work with.

If the source contains `return 42;`, the later parts of the compiler should not have to rediscover where the word ends or whether `42` is a number. The lexer identifies a return keyword, an integer literal, a semicolon, and eventually the end of the file.

I gave **meg** its first token vocabulary. It recognizes identifiers and integer literals, along with keywords such as `fn`, `let`, `return`, `if`, `else`, `while`, `true`, `false`, `i64`, and `bool`. It also recognizes punctuation, arithmetic operators, comparisons, assignment, and the arrow used before a function return type.

The lexer skips whitespace as well as line and block comments. Integer literals can be decimal, hexadecimal, or binary, and underscores can separate digits to make large values easier to read.

It also keeps track of lines and columns, including Windows-style line endings. That is exactly the sort of detail that is easy to overlook when imagining a tokenizer as something that only checks one character at a time.

The first pipeline looked like this:

```meg
[source characters] -> [tokens] -> [parser later]
```

At that point the parser really was “later.” The tokens did not yet form a program, but they gave every later stage something structured to receive.

## How do I know the first layer works?

I wrote tests for both the source module and the lexer. The lexer test uses a small piece of Meghan-like text containing a function keyword, an identifier, a hexadecimal integer, a comparison operator, a boolean value, and a comment between them.

The test checks the token kinds, source spans, line and column information, the end-of-file token, and whether any errors were reported. That is more than asking whether the tokenizer happened to recognize a few words. It checks whether the bookkeeping stays correct while it does so.

This taught me that a tokenizer is not only about recognizing characters. Line endings, comments, malformed numbers, source positions, and useful diagnostics all have to work together.

It also gave me the first glimpse of how this compiler would grow: one layer creates reliable information for the next layer to use.

## What happens after tokens?

Tokens tell me what the individual pieces are, but they do not tell me how a whole program fits together.

That is where the *parser* comes in. It reads those tokens, checks whether they follow the structure that **meg** expects, and builds a tree representing the program. Later compiler stages can then work with relationships and meaning instead of individual characters.

That tree is called an *abstract syntax tree*, or AST. It is a structured version of the source program that keeps the important relationships while leaving behind details that later stages no longer need.

For example, consider this expression:

```meg
1 + 2 * 3
```

The multiplication must happen before the addition. If the parser grouped everything from left to right, it could accidentally treat the expression like `(1 + 2) * 3`, which produces a different result. The AST preserves the intended grouping.

I added expression nodes for integer values, boolean values, names, unary operations, and binary operations. I also added statement nodes for variable declarations, returns, expression statements, blocks, `if` statements, and `while` loops.

The parser could now accept a first function shaped like this:

```meg
fn main() -> i64 {
    return 1 + 2 * 3;
}
```

Inside the compiler, that is no longer just a sequence of bytes. It becomes a program containing a function, a block, a return statement, and a binary expression. The multiplication is nested below the addition so the tree matches the program I wrote.

This was one of those moments when **meg** stopped feeling like a collection of character checks and began to feel like a language.

## How does the parser understand expressions?

The parser uses precedence levels while it builds expression trees. Multiplication, division, and remainder bind more tightly than addition and subtraction. Comparisons and equality have their own levels, while assignment sits lower.

Parentheses can override those normal rules. The parser also supports unary minus and the bang operator, so an expression such as this keeps exactly the structure I intended:

```meg
-(answer + 1) * 2
```

The function body can contain blocks, `let` declarations using the current types, returns, expression statements, conditional branches, and `while` loops.

The parser preserves source spans when it creates AST nodes. That means the tree still knows where its pieces came from in the original text. The source map I created near the beginning is already following the program deeper into the compiler.

I added AST printing too. It produces a simple, readable view of the parsed function and its statements. Before the compiler could generate C or machine code, this gave me a way to look inside and confirm that the parser had built the tree I expected.

## What can go wrong while parsing?

Recognizing valid expressions is only part of the work. The parser must also respond when something is missing, such as a closing parenthesis, a semicolon, or a block brace.

It reports an error through the diagnostic system and tries to retain enough state to continue parsing where possible. Error recovery is important because stopping at the very first mistake would hide other useful information from the programmer.

Integer values created another boundary between stages. The lexer identifies the text as an integer literal, but the parser turns those source bytes into a value and detects values that are too large. Learning where recognition ends and interpretation begins helped me see why compilers are divided into these separate layers.

Memory ownership also became real very quickly. Every expression and statement in the AST is allocated, and the tree has to clean up nested expressions, lists of statements, branches, and loops correctly. Long before code generation, the compiler already has to take responsibility for every structure it creates.

I added separate AST and parser tests. One parser test checks that `return 1 + 2 * 3` becomes a program with a block, a return statement, and a correctly nested binary expression. The AST tests cover construction, printing, and cleanup.

The pipeline had now grown into this:

```meg
[Meghan source] -> [tokens] -> [AST]
```

That was a big change, but the AST only said that a program had the right shape. It could not yet say whether the program actually made sense.

## What does it mean for a program to make sense?

A parser can recognize a `let`, a `return`, an `if`, or an expression. A program can follow all of those grammar rules and still be wrong.

It might use a variable that was never declared. It might try to add a boolean to an integer. It might return a boolean from a function that promises an integer. Those are not syntax errors, so the parser cannot answer them on its own.

This is the job of the *semantic checker*.

Semantic checking is where **meg** begins checking the meaning behind the structure. I added a checker that walks the AST after parsing and records what it learns about expressions and variables.

At first, the two value types were `i64` and `bool`. Integer expressions produce `i64`, boolean expressions produce `bool`, and names receive their type from the variables they refer to.

This program makes sense:

```meg
fn main() -> i64 {
    let answer: i64 = 42;
    return answer;
}
```

The checker can reject the program if the initializer does not match the declared type, or if `main` tries to return a boolean instead of an `i64`.

The parser builds a model of how the code is arranged. The checker begins to decide what that model means.

## How does **meg** remember variables?

The checker has a symbol table. When it sees a declaration, it creates a *symbol* containing the variable's source name, type, and an internal ID. When it later finds that name in an expression, it looks up the symbol and attaches it to the AST node.

Lookup follows nested scopes. A variable declared in an outer block can be visible inside a nested block, but a variable created in that inner block should not escape back out. This is the first time the compiler has to understand context that reaches beyond one individual expression.

The checker detects duplicate variables in the same block and reports unknown variables too. The rules are small, but they are fundamental. A compiler needs to know not only that a piece of text looks like a name, but exactly which declaration that name belongs to.

The type rules started small and deliberate. Arithmetic operators such as `+`, `-`, `*`, `/`, and `%` require integer operands and produce an integer result. Comparisons also require integers but produce a `bool`. Equality and inequality require both operands to have the same type, and they produce a `bool` as well.

Unary minus requires an integer, while unary `!` requires a boolean. Conditions in `if` and `while` statements must be boolean. Assignments must target variables, and the assigned value must have the correct type.

Here is a program the parser can understand but the checker must reject:

```meg
fn main() -> i64 {
    let ready: bool = true;
    return ready;
}
```

Its structure is valid, but its meaning violates the function's promise to return an integer. That makes it a semantic error rather than a syntax error.

## Why is checking more than a list of type rules?

One interesting lesson was that I could not apply every rule in every place. Assignment is allowed as a statement, but it is not allowed to hide inside another expression. The checker needs to know the context in which it is examining a node.

Returns have a similar complication. Finding a return statement somewhere in the function is not enough. **meg** checks whether execution can reach the end of the function without returning.

If both the `then` and `else` branches of an `if` return, that can guarantee a result. If only one branch returns, the other path can still reach the end. The checker has to think about paths through the program, not simply count keywords.

I added checker tests beside the source, lexer, AST, and parser tests. A valid declaration-and-return test verifies that the declaration is connected to a symbol. Other tests cover an unknown variable and an invalid assignment, confirming that the checker reports errors and increments its error count.

I learned that the AST is more than a convenient tree for printing. It is a shared structure where compiler stages can leave information for the stages that follow. The parser creates it, and the checker enriches it with types and symbol references.

The pipeline now looked like this:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST]
```

For the first time, **meg** could tell the difference between a program that merely had the right shape and a program that also followed the language's rules.

It still could not produce another program, though. That was the next big bridge to cross.

## Can **meg** finally compile something?

Yes, in its first small way!

I added the first *code generation* stage. It receives the checked abstract syntax tree and writes a C program.

The pipeline finally reached the target I described in the introduction:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST] -> [C code]
```

That generated C can then be passed to a C compiler and turned into machine code. **meg** is still a transpiler at this stage, just as I planned from the beginning, but watching Meghan source travel through the entire pipeline felt like a huge moment.

The `meg` command can accept an input file and send generated output either to standard output or to a file. C is the default output, and I kept an option for printing the AST whenever I want to inspect what the parser produced.

For example, this Meghan function can travel through the lexer, parser, checker, and generator in one command:

```meg
fn main() -> i64 {
    let answer: i64 = 40 + 2;
    return answer;
}
```

Semantic checking happens before C generation. A program containing an unknown variable or a type mismatch stops before it can produce misleading output.

## What does the first C generator understand?

The C generator emits all of the language pieces that **meg** understands so far: integer and boolean values, names, unary and binary expressions, variable declarations, returns, blocks, `if` statements, and `while` loops.

Names in the generated C use the internal symbol IDs instead of copying source names directly. That gives each checked declaration a unique, dependable C name, even if a nested block uses the same source name again.

The generated program contains a `meg_main` function and a normal C `main` function that calls it. It also includes the C headers needed for types such as `int64_t` and `bool`.

The generator puts parentheses around unary and binary expressions. The parser has already decided the Meghan precedence, and those parentheses make sure the C compiler preserves exactly the same structure.

Code generation taught me that similar-looking operators do not always have identical behavior in the target language. Division and remainder were the first clear examples.

Some signed C division cases have undefined behavior, including division by zero and dividing the smallest 64-bit integer by negative one. Instead of emitting those operations directly, **meg** writes helper functions. The helpers call `abort()` when one of those invalid cases occurs.

It is a small piece of the generated program, but an important one. A compiler should not quietly produce dangerous C and hope that the edge case never happens.

I added a pipeline test that begins with a small Meghan function, parses it, checks it, and passes it to the C generator. Earlier tests proved that individual pieces worked. This test proves that those pieces can now cooperate from the start of the compiler to its first output language.

At this point **meg** did not invoke a C compiler itself, produce object files, or contain native machine-code backends. The language also did not have user-defined functions, arguments, or a large type system.

Still, the most important bridge was in place: a checked Meghan tree could become real source code in another language.

## Why would I add JavaScript too?

C remains an important part of the original plan, but I wanted another target to experiment with. JavaScript is easy to run with a modern runtime such as Node.js, and adding it would force me to see which parts of Meghan's meaning were truly independent of the output language.

I added `--emit js` to the command line. **meg** can now select C, JavaScript, or an AST view:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST] -> [C or JavaScript]
```

Both generators receive the same checked program. The semantic checker still decides whether names, scopes, and types make sense, rather than making each backend rediscover those rules.

The JavaScript generator emits a `meg_main` function and sets the process exit code from the returned value. Meghan's `i64` values become JavaScript `BigInt` values, so integer literals receive the `n` suffix. Ordinary JavaScript numbers cannot represent every 64-bit integer exactly, so this is necessary to preserve the values Meghan promises.

Meghan equality operators become strict JavaScript comparisons. Generated variables use checked symbol IDs just as they do in C, keeping declarations unambiguous across nested scopes.

Division and remainder need help here as well. JavaScript `BigInt` already throws on division by zero, but the smallest signed 64-bit value divided by negative one is still a special Meghan edge case. I added JavaScript helpers so both output targets enforce the same invalid-operation checks.

That was an important lesson: adding a second backend exposes assumptions that are easy to miss with only one. A generator cannot print a similar operator and simply hope that two target languages give it the same meaning.

## How did `for` loops travel through the compiler?

While adding the JavaScript backend, I also gave the language a `for` loop. Its syntax has an initializer, a boolean condition, a step expression, and a block for its body:

```meg
for (let i: i64 = 0; i < 4; i = i + 1) {
    sum = sum + i;
}
```

This feature showed me the full length of the compiler pipeline because a new language construct does not live in only one place.

The lexer must recognize the `for` keyword. The parser must understand its punctuation and store the initializer, condition, step, and body in the AST. AST printing and cleanup need to support the new node. The checker must give the loop its own scope, check every part, and require a boolean condition. Finally, both generators must turn the checked loop into valid target code.

The loop variable belongs to the loop scope. It can be used by the condition, step, and body, but it should not unexpectedly escape after the loop has finished.

C receives a normal `for` statement. JavaScript receives one using `let`. The syntax looks familiar in both targets, but that final printed statement only works because every earlier stage agrees about what the loop means.

One useful failure case is a loop with an integer condition instead of a boolean. The parser understands its shape, but the checker correctly rejects it. I added tests for both a valid loop and an invalid one, including an attempt to use the loop variable after its scope ends.

Most of the expanded suite passed, covering the lexer, parser, checker, AST, pipeline, loop, and command line. One older smoke test still expected the previous compiler version after I bumped the version to 2, so that test failed.

It was not a problem with the loop or either generator. It was a small release-maintenance detail, but still an honest reminder that even a version string is part of the behavior tests can observe.

## Why does integer size matter?

Up to this point, Meghan mostly lived in the world of `i64`, a signed 64-bit integer. That was a useful place to begin, but real programs need a clearer way to say what kind of number they are working with.

I expanded the language with signed integer types `i8`, `i16`, `i32`, and `i64`, together with unsigned types `u8`, `u16`, `u32`, and `u64`.

A `u8` can store a small non-negative value from zero to 255. A `u64` can represent a much larger non-negative value. Signed integers can represent negative values, but their exact range depends on their width.

```meg
let small: i8 = 42;
let maximum: u64 = 18446744073709551615;
```

This was not just a matter of adding eight names. Each type has its own range, its own arithmetic behavior, and a responsibility that must be carried through the entire compiler.

The lexer now recognizes each type keyword, and the parser accepts them in variable declarations and function return types. I changed integer-literal parsing so a positive literal can first reach the full `u64` range instead of stopping at the signed 64-bit maximum.

The semantic checker is where the new types gain their meaning. It knows which integer types are signed, which are unsigned, how wide they are, and whether a literal fits the type requested by the program.

For example, 127 fits into an `i8`, but 128 does not. The checker catches that before the value reaches a code generator. Negative values need careful handling because the smallest signed value has one more possible magnitude on the negative side than on the positive side.

Assignments, returns, comparisons, and arithmetic still have strict rules. Integer operands must agree on their type, and an assigned or returned value must match the declared type. That keeps Meghan's behavior clear while I continue learning where conversions and mixed-width expressions should eventually fit.

## How do two backends preserve the new integer rules?

The C backend maps Meghan's integer types to fixed-width C types such as `int16_t` and `uint32_t`. It also emits the correct literal form for values that require the full unsigned 64-bit range.

JavaScript needs more machinery. Meghan already represents its integers with `BigInt` there, but a `BigInt` by itself has no fixed eight-, sixteen-, thirty-two-, or sixty-four-bit width.

The generated JavaScript therefore keeps track of the declared integer width too. Unsigned arithmetic wraps back into that width, like an actual fixed-width unsigned integer. Signed arithmetic checks whether its result remains within range and reports an overflow if it does not.

Division and remainder distinguish signed and unsigned rules, including division by zero and the minimum-signed-value edge case.

This part matters to me because code generation is not complete merely when the output looks similar to the source. It has to preserve the source language's meaning, even when C and JavaScript make different choices by default.

I expanded the lexer, parser, checker, and pipeline tests for the whole integer family. The tests cover valid small, medium, and unsigned values, values that exceed their declared types, and the C and JavaScript forms produced by the generators.

I also added a program that uses every unsigned type from `u8` through `u64`. Seeing the largest possible `u64` literal travel through parsing, checking, and both output targets made the work feel much more real than merely adding new type names.

## What did this first stretch teach me?

When I started, I thought the tokenizer would mostly be a loop that categorized characters. Instead, I learned how quickly source positions, malformed input, diagnostics, and platform line endings become part of the compiler's foundation.

The parser taught me that recognizing tokens is different from understanding their relationships. Operator precedence, nested statements, error recovery, AST ownership, and cleanup all become necessary before the program has any meaning checked at all.

Semantic analysis taught me that valid grammar is not the same as a valid program. Names need declarations. Declarations live in scopes. Expressions have types. Returns describe paths through a function, not just isolated statements.

Code generation showed me why a backend is more than a printer. Operations that seem simple can hide undefined behavior, overflow rules, or target-language differences. Adding JavaScript beside C made those differences much harder to ignore, which is exactly what made the second backend useful as a learning tool.

And adding `for` loops plus eight integer types showed me what it really means to grow a language. A feature has to pass through every layer: tokens, syntax, tree structure, semantic rules, diagnostics, code generation, and tests.

The full journey in this episode now looks like this:

```meg
[Meghan source]
    -> [tokens]
    -> [AST]
    -> [checked AST]
    -> [C or JavaScript]
    -> [a target compiler or runtime]
```

**meg** is still young. It does not yet invoke the target compiler itself, and it does not yet have the native freestanding or hosted backends that I ultimately want to understand and build. User-defined functions, arguments, conversions, mixed-width expressions, and a larger type system are still ahead of me too.

But it is no longer an empty project or a command that only prints its version. It can follow source text from characters to tokens, build a structured program, resolve names, enforce scopes and types, and generate code for two very different targets.

That is far more than I expected when I said “maybe more!”

## What is next?

Next I want to keep tightening the language rules and improving the tests while making the command-line journey from Meghan source to a runnable program smoother. I also want to explore conversions, mixed-width expressions, and the features a useful language will need, without losing sight of the longer goal: learning enough to give **meg** real backends for freestanding environments, Windows, and Linux.

There is still a lot I do not know, but that is the fun of building this compiler. Every layer gives me another reason to look lower, ask better questions, and understand more than I did before.

Thank you for joining me for this much bigger first episode of the journey. Take care, and I will see you very soon!
