# The Meghan Compiler: Episode 4

Hello again, this is Kenneth Looney, and welcome back to my journey of building **Meghan**, also called the **meg** compiler. In Episode 3, I gave the compiler a semantic checker so it could decide whether a program makes sense. This time, the program finally goes somewhere: **meg** can turn a checked Meghan program into C code!

## Can **meg** finally compile something?

Yes, in its first small way. I added the first *code generation* stage, which takes the checked abstract syntax tree and writes a C program.

The pipeline now has all of these steps working together:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST] -> [C code]
```

That C code can then be passed to a C compiler to become machine code. **meg** is still a transpiler at this point, just as I planned at the beginning, but seeing source code travel through the whole pipeline is a big moment for me.

## How do I use the compiler now?

The `meg` command can now accept an input file and write generated output to either standard output or a file. The default output is C, and I also kept an option to print the AST when I want to inspect what the parser created.

For example, this Meghan program:

```meg
fn main() -> i64 {
    let answer: i64 = 40 + 2;
    return answer;
}
```

can go through the lexer, parser, checker, and generator in one command. Before C is generated, the semantic checker still runs first. That means a program with an unknown variable or a type mismatch stops before it reaches the generated output.

## What does the C generator understand?

The generator can write the parts of the language that **meg** understands today. It emits integer and boolean values, names, unary and binary expressions, variable declarations, returns, blocks, `if` statements, and `while` loops.

Names in the generated C code use internal symbol IDs rather than copying the source name directly. That gives every checked declaration its own reliable C name, even when a nested scope uses the same source name again.

The generated program contains a `meg_main` function and a normal C `main` function that calls it. It also includes the C headers needed for `int64_t` and `bool`.

## What did I have to be careful about?

Division and remainder turned out to need a little extra care. Some edge cases in signed C division can have undefined behavior, including division by zero and the smallest 64-bit integer divided by negative one.

Instead of emitting those operations directly, the generator writes helper functions for them. The helpers stop the generated program with `abort()` when one of those invalid cases happens. It is a small detail, but it matters because the compiler should not quietly generate C with dangerous behavior hiding inside it.

The generator also puts parentheses around emitted unary and binary expressions. The parser already understands Meghan precedence, and the parentheses make sure the generated C preserves that exact structure.

## How do I know it works?

I added a pipeline test that starts with a small Meghan function, parses it, checks it, and sends it through the C generator. The test makes sure each stage succeeds together rather than only in isolation.

The full CTest suite passes with the new pipeline test included. That is especially useful now because the compiler stages are connected: a change in one layer can affect the next layer in a very real way.

## What is still unfinished?

This is the first C backend, not the final compiler. **meg** does not yet invoke a C compiler itself, generate object files, or have native machine-code backends.

The language is still deliberately small too. There are no user-defined functions, function arguments, or a larger type system yet. But the important bridge is in place: the checked tree can now become real source code in another language.

## What is next?

Next I want to keep improving this path from Meghan source to a runnable program. The C output gives me a practical foundation while I keep learning more about code generation, diagnostics, and eventually the native backends I want **meg** to have.

Thank you for listening to Episode 4 of my journey. The compiler has taken its first step from understanding a program to producing one, and I am excited to keep going. Take care, and I will see you very soon!