# The Meghan Compiler: Episode 2

Hello again, this is Kenneth Looney, and welcome back to my journey of building **Meghan**, also called the **meg** compiler. In the last episode I finished the project foundation and the first *lexer*. This time I have gone one step further: **meg** now has *expressions*, an *abstract syntax tree*, and a real *parser*!

## What happens after tokens?

In the intro I said I wanted to understand what code means at the machine level. The lexer was the first step because it turned characters into tokens, but tokens by themselves do not tell me how a program is put together.

That is where the parser comes in. It reads the tokens and checks whether they follow the structure that **meg** expects. Then it builds a tree that represents the program, so later compiler stages can work with meaning instead of individual characters.

## What is an abstract syntax tree?

An *abstract syntax tree*, or AST, is a structured version of the source program. It keeps the important relationships in the code. For example, in `1 + 2 * 3`, the multiplication needs to happen before the addition, and the tree needs to preserve that.

I added expression nodes for integer values, boolean values, names, unary operations, and binary operations. The AST also has statement nodes for variable declarations, returns, expression statements, blocks, `if`, and `while`.

The parser currently expects a function shaped like this:

```meg
fn main() -> i64 {
    return 1 + 2 * 3;
}
```

That is not just text anymore. It becomes a program containing a function, a block, a return statement, and a binary expression with the correct nesting.

## How does the parser understand expressions?

The interesting part for me was operator precedence. If I read `1 + 2 * 3` from left to right without any rules, I could accidentally group it as `(1 + 2) * 3`. That would change the result completely.

The parser uses precedence levels while it builds the expression tree. Multiplication, division, and remainder bind more tightly than addition and subtraction. Comparisons and equality have their own levels, and assignment is handled at the lower end.

Parentheses can override those rules, and the parser also supports the unary minus and bang operators. So an expression like this can retain the structure the programmer intended:

```meg
-(answer + 1) * 2
```

This was one of those moments where the compiler stopped feeling like a collection of character checks and started feeling like a language.

## What can a first program contain?

The parser recognizes the first statement forms for **meg**. A function body can contain blocks, `let` declarations with `i64` or `bool` types, returns, expression statements, conditional branches, and `while` loops.

It also keeps source spans as it builds the AST. That means the pieces of the tree still know where they came from in the original source, which gives future diagnostics a useful place to point.

I added AST printing too. It prints a simple readable representation of the parsed function and its statements. This gives me a way to inspect the tree while the compiler is still too young to generate C or machine code.

## What went wrong?

The difficult part was not only recognizing valid expressions. The parser also needs to recover when something is missing, such as a closing parenthesis, a semicolon, or a block brace. It reports an error through the existing diagnostic system and keeps enough state to finish parsing where possible.

Integer values introduced another boundary. The lexer identifies an integer literal, but the parser is responsible for turning those source bytes into an `i64` value and detecting values that are too large. That separation between recognizing syntax and interpreting it was a useful thing to learn.

There is still plenty that is intentionally unfinished. The AST can describe names and types, but there is no type checker or name resolver yet. The parser can build a program, but **meg** still does not compile that program into C.

## How do I know it works?

I added separate tests for the AST and the parser. The parser test checks that a small function with `return 1 + 2 * 3` becomes a program with a block, a return statement, and a binary expression. The AST test checks construction, printing, and cleanup.

All of the tests pass, including the earlier smoke, source, lexer, and command-line tests. That gives me confidence that the new parser layer is connected to the existing foundation without breaking it.

## What did I learn?

I learned that parsing is where the compiler starts building a model of what the programmer meant. The lexer tells me that something is an identifier or an operator. The parser starts telling me how those things relate to one another.

I also learned that memory ownership matters early. Every expression and statement in the tree is allocated, and the AST needs to clean up nested expressions, statement lists, branches, and loops correctly. Even before code generation, the compiler has to take care of the structures it creates.

This is still a small parser, but it is a big change in the project. **meg** can now move from source text to tokens and from tokens to a structured program:

```meg
[Meghan source] -> [tokens] -> [AST]
```

## What is next?

The next question is what those names and types mean together. That points toward name resolution and type checking, where **meg** can start deciding whether a parsed program makes sense instead of only whether it has the right shape.

Thank you for listening to Episode 2 of my journey. The compiler is growing one layer at a time, and I am excited to see where it takes me next. Take care, and I will see you very soon!
