# The Meghan Compiler: A First Step Toward Freestanding Code

Hello again, this is Kenneth Looney, and I am back with the next episode in my journey of building **Meghan**, also called the **meg** compiler.

Episode 1 grew **meg** from a tokenizer into a compiler pipeline that can parse Meghan source, check it, and generate either C or JavaScript. I also added `for` loops and the family of signed and unsigned integer types. When I left off, the long-term goal of freestanding and hosted backends was still ahead of me.

This time I started moving in that direction. The new feature is not a complete native backend yet, but it changes the kind of C that **meg** can produce. I can now ask it for output intended to live without the usual hosted C runtime.

## What does freestanding mean here?

Hosted C gives a program a familiar environment. It can include the standard library, provide a normal `main` function, and rely on facilities such as `abort()` when something goes wrong.

A freestanding environment makes fewer promises. There may be no operating system, no standard library, and no runtime that will quietly provide those services. The program needs an entry point supplied by the environment, and any platform-specific behavior has to be supplied explicitly.

That difference is exactly what I wanted to make visible in the generated output. **meg** still generates C, but it now has two C modes:

```meg
[Meghan source] -> [checked AST] -> [hosted C or freestanding C]
```

The regular C path keeps its `meg_main` function and the ordinary C `main` wrapper. The new path emits a `meg_entry` function instead. That name is deliberately not pretending to be a universal firmware entry point. It is a clear hook for the platform startup code that I will need to understand and provide later.

## What changed in the command line?

The `meg` executable now accepts `--emit freestanding` alongside the existing C, JavaScript, and AST modes:

```text
meg -i program.meg --emit freestanding -o program.c
```

The command still runs the same lexer, parser, and semantic checker first. That matters to me because changing the output mode should not create a second language pipeline with different rules. The same checked program is handed to the selected generator.

The code generator now shares its normal C emission logic with the freestanding path, while changing the pieces that depend on the runtime environment. The generated freestanding C includes the fixed-width integer and boolean headers it needs, but it does not include `<stdlib.h>`.

## What happens when an operation is invalid?

Episode 1 ended with a detail that became important here. The C generator protects division and remainder from division by zero and from the signed 64-bit minimum-value edge case. Hosted output used `abort()` for those invalid operations.

That is not a good assumption for freestanding output. Instead, the generated code declares an external `meg_panic()` function and routes invalid operations through a small trap helper. The environment that embeds this output can provide `meg_panic()` in whatever way makes sense for its platform.

```c
extern void meg_panic(void);
```

After calling it, the helper stays in an infinite loop. There is no operating-system exit function to call, and returning from an invalid arithmetic operation would allow execution to continue as if nothing had happened.

This is a small generated-C detail, but it taught me something important: a backend is defined just as much by the services it refuses to assume as by the code it emits.

## How do I know the new mode is really different?

I added a focused freestanding test. It parses and checks a Meghan program with a loop and integer arithmetic, generates freestanding C, and inspects the result.

The test confirms that the output has an `int64_t meg_entry(void)` function and declares `meg_panic()`. It also confirms that the output has no `<stdlib.h>`, no `abort()`, and no ordinary `int main(void)` wrapper.

The full Windows Debug test suite now passes all ten tests, including the new freestanding test. That gives me a useful checkpoint while this episode remains a work in progress: the new output mode is not just a command-line branch, it is covered all the way through generation.

## What is still missing?

There is an important boundary here. **meg** does not yet generate machine code, an object file, a linker script, or a complete platform startup routine. The freestanding C output still needs a C toolchain and an environment that supplies its entry point and `meg_panic()` implementation.

So this is not the finished freestanding backend I imagined in the introduction. It is the first practical step toward one. I am learning where the language compiler ends and where a target environment begins.

The next features can build on this split between hosted and freestanding output. I want to keep adding real language behavior while making the generated program's runtime assumptions more explicit. There is a lot more to learn before **meg** can stand on its own in a freestanding environment, but now the generated code is at least pointing in that direction.

## What is next?

I will keep updating this episode as the next feature work takes shape, and I will save the final version for the podcast once there is enough of the journey to share. For now, **meg** has taken its first step from ordinary generated C toward code that can belong to a smaller, more deliberate environment.

Thank you for joining me again. Take care, and I will see you very soon!
