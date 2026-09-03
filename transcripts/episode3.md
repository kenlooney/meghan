# The Meghan Compiler: Episode 3

Hello again, this is Kenneth Looney, and welcome back to my journey of building **Meghan**, also called the **meg** compiler. In Episode 2, I got source text through the lexer and parser and turned it into an *abstract syntax tree*. This time I asked a bigger question: does the program actually make sense?

## What comes after parsing?

A parser can tell me whether the pieces of a program are arranged in the right order. It can recognize a `let`, a `return`, an `if`, or an expression. But a syntactically correct program can still contain mistakes.

For example, a program might use a variable that was never declared, add a boolean to an integer, or return a value of the wrong type. The parser cannot answer those questions by itself. That is the job of a *semantic checker*.

## What does semantic checking mean?

Semantic checking is where **meg** starts checking the meaning behind the structure. I added a checker that walks the AST after parsing and records what it learns about each expression and variable.

The first two value types are `i64` and `bool`. Integer expressions produce `i64`, boolean expressions produce `bool`, and names get their type from the variable they refer to.

So this is valid:

```meg
fn main() -> i64 {
    let answer: i64 = 42;
    return answer;
}
```

But the checker can now reject a program when the initializer does not match the declared type, or when `main` tries to return a boolean instead of an `i64`.

## How does **meg** remember variables?

The checker now has a symbol table. When it sees a declaration, it creates a *symbol* containing the variable's source name, type, and an internal ID. When it later sees that name in an expression, it looks up the symbol and attaches it to the AST node.

The lookup follows nested scopes. A variable declared in an outer block can be visible inside a nested block, while a variable declared inside that nested block should not escape back out. This is the first time the compiler has a concept of context that reaches beyond one individual expression.

The checker also detects duplicate variables in the same block and reports unknown variables. Those are simple rules, but they are important rules. A compiler needs to know not only what a name looks like, but which declaration that name belongs to.

## What type rules are here now?

The checker has a small but real set of rules. Arithmetic operators such as `+`, `-`, `*`, `/`, and `%` require two `i64` operands and produce an `i64` result.

The comparison operators also require `i64` operands, but they produce a `bool`. Equality and inequality require both operands to have the same type and also produce a `bool`.

Unary minus requires an integer, while unary `!` requires a boolean. Conditions in `if` and `while` statements must be boolean, and assignments must target a variable whose type matches the value being assigned.

Here is a small example where the checker has useful work to do:

```meg
fn main() -> i64 {
    let ready: bool = true;
    return ready;
}
```

The shape of this program is understandable to the parser, but the checker reports that `main` must return `i64`. That is a semantic error, not a syntax error.

## What went wrong?

The interesting part was learning that checking cannot be one rule applied everywhere. Assignment is allowed as a statement, but it is not allowed to hide inside another expression. The checker needs to know the context in which it is examining an expression.

Return checking has a similar detail. It is not enough to see a return statement somewhere in the tree. **meg** now checks whether the function can reach the end without returning, including the branches of an `if`. A return in both the `then` and `else` paths can guarantee a result, while a return in only one path cannot.

There is still a lot missing. The checker does not yet perform full code generation, it does not support user-defined functions, and the type system is intentionally very small. This is the beginning of semantic analysis, not the finished language.

## How do I know it works?

I added checker tests beside the source, lexer, AST, and parser tests. One test checks a valid declaration and return, and verifies that the declaration is connected to a symbol.

The tests also cover an unknown variable and an invalid assignment. In both cases, the checker reports an error and increments its error count. The complete test suite now passes, including the new checker test.

## What did I learn?

I learned that the AST is more than a convenient tree for printing. It is the place where different compiler stages can leave information for the stages that follow. The parser creates the structure, and the checker enriches it with types and symbol references.

I also learned that scopes make a language feel much more real. Once variables can be declared, looked up, and limited to blocks, the compiler has to track the surroundings of every expression. That is a new kind of complexity, but it is exactly the kind of complexity I wanted to understand when I started this project.

The pipeline now has another important stage:

```meg
[Meghan source] -> [tokens] -> [AST] -> [checked AST]
```

The program still does not become C or machine code, but **meg** can now distinguish between a program that has the right shape and one that also follows the language's current rules.

## What is next?

The next step will be to keep growing the language and its semantic model. I want to build on these symbols and types before eventually reaching the first **Meghan** to C output.

Thank you for listening to Episode 3 of my journey. Every new stage gives me another piece of the compiler to understand, and I am excited to keep going. Take care, and I will see you very soon!
