# The Meghan Compiler: Episode 6

Hello again, this is Kenneth Looney, and welcome back to my journey of building **Meghan**, also called the **meg** compiler. In Episode 5, I gave the language `for` loops and a JavaScript output target. This time I make a much bigger change to the language itself: **meg** can now work with more than just one integer size.

## Why does integer size matter?

Until now, Meghan mostly lived in the world of `i64`, a signed 64-bit integer. That is a good place to begin, but real programs need to say more clearly what kind of number they are working with.

I added signed integer types `i8`, `i16`, `i32`, and `i64`, along with unsigned types `u8`, `u16`, `u32`, and `u64`. A `u8` is useful for a small value from zero to 255, while a `u64` can represent a much larger non-negative value.

This is not only about adding names to the language. Each type has its own range, its own arithmetic behavior, and its own responsibility all the way through the compiler.

```meg
let small: i8 = 42;
let maximum: u64 = 18446744073709551615;
```

## How does the compiler understand them?

The lexer now recognizes every new type name, and the parser accepts them for variable declarations and function return types. I also changed integer literal parsing so a literal can first reach the full `u64` range instead of stopping at the signed 64-bit limit.

The *semantic checker* is where the meaning becomes real. It knows which types are signed, which are unsigned, and whether an integer literal fits the type I asked for.

For example, 127 fits in an `i8`, but 128 does not. The checker catches that before either code generator gets involved. Negative values are also checked carefully, because the smallest signed number has one more value on the negative side than it does on the positive side.

The checker still makes assignments, comparisons, and arithmetic honest. Integer operands have to agree on their type, and an assignment or return value has to match its declared type. That keeps the language simple while I am still learning where more flexible conversions should eventually belong.

## What happens when Meghan generates code?

The C backend now maps Meghan integers to C's fixed-width types, such as `int16_t` and `uint32_t`. It also emits the right literal form for values that need the unsigned 64-bit range.

JavaScript needs more help because its normal number type cannot represent every integer exactly. Meghan already uses JavaScript `BigInt`, and now the generated code also keeps track of the declared integer width.

Unsigned arithmetic wraps back into its width, like an actual unsigned integer. Signed arithmetic checks whether the result stays in range and reports an overflow if it does not. Division and remainder also distinguish signed and unsigned rules, including division by zero and the signed minimum-value edge case.

That feels important to me. A code generator is not finished when it prints code that looks similar. It has to preserve the meaning of the source language, even when C and JavaScript make different choices by default.

## How do I know it is working?

I expanded the lexer, parser, checker, and pipeline tests for the new integer family. The tests cover valid small, medium, and unsigned values, values that are too large for their declared type, and the generated C and JavaScript forms.

I also added a program that uses every unsigned type from `u8` through `u64`. Seeing the largest `u64` literal travel through parsing, checking, and both output targets makes this update feel much more real than adding type names alone.

## What is next?

Meghan now has a stronger foundation for numbers, but I still have plenty to learn about conversions, mixed-width expressions, and the rest of a useful type system. I want to keep tightening the language rules and the tests while making sure both output targets continue to agree.

Thank you for listening to Episode 6 of my journey. **meg** understands a much wider range of integers now, and I am excited to see where that opens the door next. Take care, and I will see you very soon!
