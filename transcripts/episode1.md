# The Meghan Compiler: Episode 1

Hello again, this is Kenneth Looney, and this is the first real episode in my journey of building **Meghan**, also called the **meg** compiler. In the intro I said I was going to start with project setup, an initial *tokenizer*, and maybe more. Well, I definitely got that maybe more!

## Where do I even start?

The first step was getting a project that can actually build. I set up **meg** with CMake and C11, separated the compiler target from the language library, and added a test setup so I could check each piece as I went.

The project now builds on Windows and Ubuntu. That may sound like a small thing, but having the same project build in more than one environment gives me a much better foundation for everything that comes next.

The command-line program is still very small. Right now it can report its help and version information. It is not compiling Meghan source yet, but it gives the project a real executable to grow into.

## How can the compiler understand source code?

Before **meg** can understand a program, it needs to know where that program came from. I added a *source* module that can hold text from a file or from memory, along with its path and length.

I also added *source spans*. A span is just a small piece of source text with a starting position, a length, a line, and a column. This is how the compiler can eventually tell me something useful like where an error happened instead of only saying that something went wrong.

There is a diagnostic system alongside it. Diagnostics can report errors or warnings through a sink, and they can print the source location with the message. The source code is beginning to carry its own map now, which will matter more and more as the language grows.

## What is a tokenizer?

The first big language step was the *lexer*, or *tokenizer*. Its job is to read the raw characters and turn them into tokens that the later parts of the compiler can understand.

I gave **meg** its first token vocabulary. It knows identifiers, integer literals, keywords like `fn`, `let`, `return`, `if`, `else`, `while`, `true`, `false`, `i64`, and `bool`, along with punctuation, arithmetic operators, comparisons, assignment, and the function arrow.

It also skips whitespace and both line and block comments. Integer literals can be decimal, hexadecimal, or binary, and underscores can be used as separators. The lexer keeps track of lines and columns, including Windows-style line endings, which is exactly the kind of detail I would have ignored before starting this project.

Here is the basic idea:

```meg
[source characters] -> [tokens] -> [parser later]
```

The parser is not here yet. That is okay. The lexer is the first layer that gives the rest of the compiler something structured to work with.

## How do I know it works?

I wrote tests for the source module and for the lexer. The lexer test feeds it a small piece of Meghan-like text with a function keyword, an identifier, a hexadecimal integer, a comparison operator, a boolean value, and a comment between them.

The test checks the token kinds, source spans, line and column information, the end-of-file token, and the fact that no errors were reported. The full test command passes, so this first stage is doing what I expect it to do.

## What did I learn?

I started this thinking the tokenizer would mostly be about checking characters. It turns out that the bookkeeping around those characters is just as important. Line endings, comments, malformed numbers, source positions, and useful diagnostics all have to work together.

I also learned that building a compiler is a lot like building a path one layer at a time. I have not reached C output yet, and I am still far away from machine code, but **meg** now has a project structure, a testable language library, source tracking, diagnostics, and its first real understanding of Meghan text.

## What is next?

Next I want to see what comes after tokens. That means thinking about the parser and how these tokens become a structure that represents a program. There is still a lot for me to learn, but now I have a solid place to continue from.

Thank you for listening to the first episode of my journey. I will keep sharing what I build, what confuses me, and what I learn along the way. Take care, and I will see you very soon!