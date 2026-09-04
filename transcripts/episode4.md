# The Meghan Compiler: Strings Move Beyond the Lexer

Hello again, this is Kenneth Looney, and I am back with the next episode in my journey of building **Meghan**, also called the **meg** compiler.

Episode 3 ended with a small but important piece of groundwork. **meg** could recognize ordinary ASCII strings, UTF-8-prefixed strings, and UTF-16-prefixed strings in the lexer, but the rest of the compiler did not know what they meant yet.

So we continued with adding string literals. This time the tokens started moving farther through the compiler pipeline, from the lexer into the abstract syntax tree and the semantic checker.

## How does a string become part of the AST?

The parser now recognizes string tokens as expressions. I added an `EXPR_STRING` expression kind that stores the literal's source span and its encoding form.

The AST can distinguish the three spellings that the lexer already knows about:

```meg
"plain ASCII"
u8"caf\u00e9"
u"snowman: \u2603"
```

The ordinary and `u8` forms are represented as the string form, while the `u` form is represented as UTF-16. For this first step, the AST keeps the original `SourceSpan` instead of allocating a decoded character buffer.

That choice keeps ownership simple. The source object already owns the original source text, so the AST can refer to the literal without creating another allocation that it has to release.

## What does the AST printer do now?

The AST printer has an `EXPR_STRING` case. It writes the original literal span back out, including its prefix, quotes, and source spelling.

This gives me a useful checkpoint before generating target code. I can parse a program and inspect its AST to confirm that the string reached the tree without having to decide yet whether C should receive bytes, an array, or a pointer to some storage.

The cleanup path also understands the new expression shape. At the moment there is no string buffer to free because the AST stores only the source span. When strings eventually own decoded data, that ownership rule will need to grow with them.

## How does the checker know what a string is?

The checker now has two value types: `TYPE_STRING` and `TYPE_USTRING`. An `EXPR_STRING` receives one of those types based on its encoding.

That means the checker can now distinguish a normal or UTF-8 string from a UTF-16 string, even though both are still only expressions and not complete variables or runtime values.

The type also gives the existing semantic rules something concrete to reject. Strings are not integers, so they cannot be used in arithmetic:

```meg
fn main() -> i64 {
	return "hello" + 1;
}
```

The parser accepts the shape of this expression, but the checker rejects it with the integer-only arithmetic diagnostic. This is exactly the kind of separation I want from the pipeline: syntax can be recognized first, and meaning can be enforced afterward.

A UTF-16 literal returned from an `i64` function is rejected for a different reason. The checker recognizes it as `TYPE_USTRING`, then sees that it does not match the function's declared return type.

## Why not generate C and JavaScript yet?

Because the representation decision is still ahead of me. A UTF-8 string could become a byte sequence or a C string, while UTF-16 needs a sequence of 16-bit code units. JavaScript has its own Unicode and UTF-16 behavior, but that does not automatically define what a Meghan string should mean.

For now, the careful boundary is:

```text
[string token] -> [EXPR_STRING] -> [checked string type]
```

The C and JavaScript generators still do not emit string expressions. Declared `string` and `ustring` variables are also not supported yet because the parser's type-name list still contains the numeric and boolean types.

## How did I test this step?

I added checker tests for string expressions. One test confirms that ordinary string arithmetic is rejected and that the expression becomes an error type. Another confirms that a UTF-16 literal receives `TYPE_USTRING` before the surrounding `i64` return mismatch is reported.

The lexer, parser, AST, and checker tests now cover progressively more of the string path. The focused Windows tests passed, and the complete Linux test suite also passed under WSL. The compiler is not finished with strings, but each layer now has a clear responsibility and a testable boundary.

## What is next?

The next string step is to let the parser accept `string` and `ustring` in declarations and function return types. Then the checker can validate string initializers, assignments, and returns before either backend commits to a runtime representation.

After that, I will need to decide how owned string data works and how C and JavaScript should represent the two encodings. For now, **meg** has moved strings one important stage beyond the lexer, and I have a much clearer place to continue from.

Thank you for joining me for this next part of the journey. Take care, and I will see you very soon!
