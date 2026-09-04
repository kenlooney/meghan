# The Meghan Compiler: Giving Functions a Voice

Hello again, this is Kenneth Looney, and I am back with the next episode in my journey of building **Meghan**, also called the **meg** compiler.

Episode 2 took **meg** toward lower-level programming with freestanding-compatible C output, then added typed pointers and references. The compiler could work with addresses, but every Meghan program still revolved around one function. This time I wanted to make the language feel more like a real programming language by giving it multiple functions that can call one another.

## Why add more than one function?

One function is enough for a small demonstration, but it puts a hard limit on how a program can be organized. I wanted to split work into named pieces and let `main` call those pieces instead of putting every expression into one large body.

The first example is deliberately simple:

```meg
fn main() -> i64 {
	return answer();
}

fn answer() -> i64 {
	return forty() + 2;
}

fn forty() -> i64 {
	return 40;
}
```

This gives **meg** a call chain. `main` calls `answer`, and `answer` calls `forty`. The result is still small, but the compiler now has to understand several function declarations, connect each call to the right declaration, and preserve the return type at every step.

## What changed inside the compiler?

The AST used to contain one function directly inside the program. It now owns a linked list of functions. A function carries its source name, return type, body, and the symbol that the checker assigns to it.

Calls are represented as their own expression kind. They remember the source name while parsing, and semantic checking later attaches the matching function symbol. This is the same general idea used for variables: the parser records the shape, and the checker resolves what each name means.

The checker builds a function symbol table before checking the bodies. That ordering is important because it allows a function to call another function declared later in the source. In other words, this works too:

```meg
fn main() -> i64 {
	return answer();
}

fn answer() -> i64 {
	return 42;
}
```

The call from `main` does not have to wait for the checker to encounter `answer` in source order. The declarations are collected first, then the bodies are checked against the complete set of known functions.

## What does the checker protect?

A call to an unknown function is a semantic error. So is a duplicate function declaration. A call expression receives the return type declared by its target function, which means the existing type rules continue to work when a call appears inside an expression or a return statement.

I also added boolean-returning functions. A function can now return `bool` and be used as the condition of an `if`:

```meg
fn main() -> i64 {
	if is_ready() {
		return answer();
	} else {
		return 0;
	}
}

fn is_ready() -> bool {
	return true;
}

fn answer() -> i64 {
	return 42;
}
```

The parser can recognize the call syntax, but the checker decides whether the called function exists and whether its return type fits the surrounding expression. That division of responsibility is becoming a dependable pattern in **meg**.

## How do the backends name the functions?

The C generator gives generated functions internal names such as `meg_main` and `meg_f_1`. The ordinary hosted C mode still emits a small `main` wrapper that calls `meg_main`. The freestanding mode can expose the generated entry function without assuming the hosted runtime.

The JavaScript generator uses the same checked function structure and emits JavaScript functions with matching generated names. Calls use those generated names as well, so source names can be resolved consistently without relying on target-language naming rules.

This is another place where the backends share meaning but not necessarily spelling. The checker decides which function a call refers to; each generator turns that resolved relationship into valid target code.

## Do all integer widths work through calls?

I added an example that defines functions returning every supported integer width: signed `i8`, `i16`, `i32`, and `i64`, along with unsigned `u8`, `u16`, `u32`, and `u64`.

The caller stores each result in a variable of the matching type and checks the values. This makes the feature more than a collection of `i64` examples. It checks that a function's declared return type travels through the checker and remains visible to both code generators.

JavaScript continues to use `BigInt` for integer values, applying the declared width and signedness when function results are emitted. C uses the corresponding fixed-width types such as `int8_t` and `uint64_t`.

## How did I test the function feature?

The new `function_tests` test covers parsing multiple functions, resolving a forward call, generating C and JavaScript calls, and preserving all supported integer return widths. It also checks that an unknown function is rejected by semantic analysis.

The rest of the test suite had to move with the AST change as well. AST cleanup, parser tests, checker tests, loop tests, pointer tests, and the version smoke test all now work with a program containing a function list instead of a single function field.

This was a useful reminder that a language feature is rarely isolated. Adding calls changed the tree, ownership, name resolution, diagnostics, and both backends. The focused test proves the new behavior, while the full suite checks that the earlier compiler stages still agree with the new program structure.

## What is still missing?

The new functions do not have parameters yet. Calls currently take no arguments, so functions communicate through their return values and the language's existing local operations. There is also no recursion or function-pointer model to discuss yet.

That boundary is useful. I now have multiple named functions and calls, but I can still see exactly what the next function milestone needs to add: parameter declarations, argument checking, parameter scopes, and target-language calling conventions.

## How do values move between functions?

I did not have to wait long to answer that question. The next feature adds function parameters and call arguments to **meg**.

A function can now declare typed parameters, and a call can provide matching arguments:

```meg
fn main() -> i64 {
	let value: i64 = 40;
	let pointer: *i64 = &value;
	let first: i64 = increment(pointer);
	let second: i64 = add(first, value);
	return second;
}

fn increment(value: *i64) -> i64 {
	*value = *value + 1;
	return *value;
}

fn add(left: i64, right: i64) -> i64 {
	return left + right;
}
```

This example connects the new feature with the pointer work from episode 2. `increment` receives a pointer, changes the value through that pointer, and returns the updated value. `add` receives two ordinary integer values. The language can now move both values and typed addresses across a function boundary.

## What does the compiler check now?

Parameters become symbols in the function's own scope. That means the function body can use a parameter like any other checked name, while duplicate parameter names are rejected instead of silently shadowing one another.

The checker compares each argument with the corresponding parameter. The types must match exactly, including the underlying value type and whether a parameter is a value, pointer, or reference. A call with too few or too many arguments is rejected, as is a call that passes an `i64` where a `bool` or an `*i64` is expected.

The checker also keeps the earlier rule that `main` cannot declare parameters. The entry function still has to match the compiler's current program-start contract.

This makes function calls a proper semantic boundary. Parsing can build a list of parameters and arguments, but only checking can decide whether the two lists have compatible lengths, types, and scopes.

## How do the backends lower parameters?

The C generator emits typed parameters using the same fixed-width and pointer forms already used for local variables. A call becomes a C call with its generated arguments in order.

JavaScript receives generated function parameters and arguments as well. Its existing `BigInt` normalization continues to apply to integer values, while pointer and reference arguments retain the behavior provided by the JavaScript reference representation.

The AST and AST printer now preserve parameter declarations and call argument lists too. That gives me a way to inspect not only the function body, but also the values crossing its boundary before either backend emits target code.

## How did I test the new boundary?

The new `parameter_tests` test checks parsing parameter lists and argument lists, parameter symbols, AST printing, and C and JavaScript generation. It also checks the failures I expect the checker to catch: wrong argument counts, mismatched argument types, duplicate parameter names, and parameters on `main`.

The example uses an `i8` function as well as `i64`, pointer, and reference parameters. That matters because parameter support has to preserve the same type rules across all of the language features that already exist.

This is another satisfying compiler boundary. The parser builds the lists, the checker connects each argument to its parameter, and the generators lower the checked relationship into their own calling syntax.

## What is still missing?

Functions can now receive values, pointers, and references, but the language still does not have variadic calls, default arguments, function pointers, or a broader calling-convention system. The entry function remains parameterless, and the current memory model is still limited to typed local values and references.

## What is next?

For now, this is a strong new step for **meg**. The compiler can take a source file with several functions, resolve calls even when declarations come later, preserve boolean and fixed-width integer return types, and generate C or JavaScript from the checked program.

Now **meg** can also carry typed values across function boundaries. There is still plenty to learn about calling conventions and memory, but functions are no longer isolated blocks that can only return a result. They can receive the data they need to do their work.

Thank you for joining me for this next part of the journey. Take care, and I will see you very soon!
