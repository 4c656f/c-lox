# C-Lox Interpreter 🚀

An implementation of the Lox programming language Interpreter written in C. This interpreter is based on the book ["Crafting Interpreters"](https://craftinginterpreters.com/) by Robert Nystrom and tested via [Codecrafters](https://app.codecrafters.io/courses/interpreter).

## About Lox 📖

Lox is a dynamically-typed scripting language that supports object-oriented programming with classes and inheritance. This implementation follows the bytecode interpreter pattern.

## Running the Interpreter

interpret a Lox script:
```bash
./run <filename>.lox
```

## Example Lox Program 📝

```lox
// This is a simple Lox program
fun fib(n) {
  if (n <= 1) return n;
  return fib(n - 2) + fib(n - 1);
}

print fib(10); // Outputs: 55
```

## Features ✨

Here's what's currently implemented in this interpreter:

| Feature | Status |
|---------|---------|
| Scanner | ✅ |
| Basic Arithmetic | ✅ |
| Variables | ✅ |
| Control Flow (if/else) | ✅ |
| Loops (while, for) | ✅ |
| Functions | ✅ |
| Closures | ✅ |
| Standard Library | ✅ |
| GC | ✅ |
| Classes | 🚧 |
| Inheritance | 🚧 |
| Error Handling | 🚧 |

## Contributing 🤝

I will be glad to receive any of your questions/suggestions/contributions to this project! Feel free to open a PR or contact me via:

[Twitter](https://x.com/4c656f)

[Email](mailto:tarabrinleonid@gmail.com)

[Telegram](https://t.me/c656f)

---

For more information about the Lox language, visit [Crafting Interpreters](https://craftinginterpreters.com/).
