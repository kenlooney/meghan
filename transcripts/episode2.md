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

## Can **meg** work with addresses now?

The next feature took me closer to the kind of low-level programming that inspired this project. I added pointer and reference types to Meghan, along with the operators needed to take an address, create a reference, dereference it, and assign through it.

A pointer type uses `*` before the value type. A reference type uses the `ref` keyword:

```meg
let value: i64 = 40;
let pointer: *i64 = &value;
*pointer = *pointer + 1;
```

The reference form expresses the same connection with a different type form:

```meg
let value: i64 = 40;
let reference: ref i64 = ref value;
*reference = *reference + 2;
```

Both examples change the original `value`. This is the first time a Meghan expression can name a storage location indirectly instead of only reading or assigning a variable by name.

## What rules keep pointers meaningful?

Pointers and references are not just decorations attached to an integer type. The checker now tracks both the underlying value type and the type form. An `*i8` is different from an `*i64`, just as an `i8` is different from an `i64`.

The address and reference operators require an assignable value. That means **meg** will not let the program take the address of an arbitrary calculation. Dereferencing requires either a pointer or a reference, and the result becomes the underlying value type.

Assignments can still target an ordinary variable, but they can now also target a dereferenced pointer or reference. The types must match exactly. If an `i16` address is assigned to an `*i8`, the checker rejects the program before either C or JavaScript is generated.

This gave me another useful lesson about semantic checking. The parser can recognize `&value`, `ref value`, and `*pointer`, but only the checker can decide whether those operations are valid in context.

## How do two output languages represent references?

The C backend can use the target language's native pointer syntax. A pointer to an `i8` becomes an `int8_t *`, and taking an address emits `&`. Dereferencing and assigning through the pointer become ordinary C operations.

JavaScript does not have C-style typed pointers, so its generator uses a small reference object with `get` and `set` behavior. Reading through a reference calls `get()`, while writing through it calls `set(...)`. The setter also applies the same integer-width normalization that ordinary assignments use.

That means the two targets do not use identical representations, but they preserve the same source-level behavior. The checked AST remains shared between them, while each backend chooses the mechanism its target language can actually support.

## How did I test the new memory operations?

I added pointer tests covering every signed and unsigned integer width from `i8` through `u64`. The tests create both pointer and reference variables, write through them, read through them, and inspect the generated C and JavaScript for the expected representations.

There is also a negative test that tries to assign the address of an `i16` value to an `*i8`. The parser accepts the syntax, but the semantic checker rejects the mismatched pointer type. That is exactly the boundary I want: syntax can be recognized broadly, while unsafe or meaningless combinations are stopped before code generation.

This feature does not add heap allocation, pointer arithmetic, null pointers, or a complete memory model yet. It gives **meg** typed addresses tied to existing local values, which is a much smaller step, but a very important one for understanding how low-level features travel through a compiler.

## What is next?

I will keep updating this episode as the next feature work takes shape, and I will save the final version for the podcast once there is enough of the journey to share. For now, **meg** has taken its first step from ordinary generated C toward code that can belong to a smaller, more deliberate environment.

Thank you for joining me again. Take care, and I will see you very soon!
