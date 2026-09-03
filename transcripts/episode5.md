# The Meghan Compiler: Episode 5

Hello again, this is Kenneth Looney, and welcome back to my journey of building **Meghan**, also called the **meg** compiler. In Episode 4, the compiler could turn a checked Meghan program into C code. This time I pushed the language forward with `for` loops and gave **meg** another way to generate runnable-looking output: JavaScript.

## Why add another output language?

The original plan for **meg** starts with C, and that is still an important part of the project. But JavaScript gives me another useful target to experiment with, especially because it is easy to run with a modern JavaScript runtime such as Node.js.

I added a new `--emit js` option to the command line. The compiler now lets me choose between C, JavaScript, and the AST when I want to inspect the different stages:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST] -> [C or JavaScript]
```

The semantic checker runs before either code generator. That means the new backend still receives a program that has already been checked instead of having to make all of those decisions again.

## What does the JavaScript output look like?

The JavaScript generator emits a `meg_main` function and sets the process exit code from its result. Meghan's `i64` values become JavaScript `BigInt` values, so integer literals receive the `n` suffix. That is important because ordinary JavaScript numbers do not represent every 64-bit integer exactly.

The generator also translates Meghan's equality operators to strict JavaScript comparisons. Variable names use the checked symbol IDs, just as they do in the C output, so declarations remain unambiguous even when scopes contain the same source name.

Division and remainder need care in this backend too. JavaScript throws for division by zero with `BigInt`, but the smallest 64-bit integer divided by negative one is another edge case that Meghan needs to define consistently. I added helper functions so the generated JavaScript checks those invalid operations before doing the calculation.

## How do `for` loops work now?

I added `for` as a new language construct. A loop has an initializer, a boolean condition, a step expression, and a block for its body:

```meg
for (let i: i64 = 0; i < 4; i = i + 1) {
    sum = sum + i;
}
```

The lexer recognizes the new keyword, and the parser stores each part of the loop in the AST. The checker gives the loop its own scope, checks the initializer and step, and makes sure the condition is a boolean. The loop variable belongs to that loop, so it should not unexpectedly escape after the loop finishes.

Both backends can now emit the same loop structure. C receives a normal `for` statement, while JavaScript receives a `for` statement using `let`. This is a satisfying kind of feature because it is not just syntax: the new construct has to survive every stage of the compiler pipeline.

## What went wrong?

One of the useful failures is a loop with an integer condition instead of a boolean condition. The parser can understand the shape of that program, but the checker correctly rejects it. That reminded me again that parsing and meaning are separate jobs.

I also had to keep the special integer behavior consistent between C and JavaScript. Adding a second backend makes differences between target languages much easier to notice. A generator cannot simply print operators and hope the target language means exactly the same thing.

## How do I know it works?

I expanded the tests with a valid `for` loop and an invalid one. The invalid case checks both the non-boolean condition and the use of the loop variable outside its scope.

The lexer, parser, checker, AST, pipeline, `for` loop, and CLI tests pass with these changes. One older smoke test still expects the previous compiler version, so it fails after the version bump to 2. That is a small release-maintenance detail to fix before I push, but it does not change the loop or code-generation work in this episode.

## What is next?

Now **meg** has two source-generation targets, but it still does not invoke those target compilers itself. I want to keep improving the command-line workflow and add more tests around generated output, while continuing toward the larger goal of freestanding and hosted backends of its own.

Thank you for listening to Episode 5 of my journey. The language can now repeat work with a `for` loop and travel toward either C or JavaScript, which gives me a lot more to explore. Take care, and I will see you very soon!