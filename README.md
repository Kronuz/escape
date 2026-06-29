# escape

A small C++20 utility that turns an arbitrary byte buffer into a printable,
C-string-style escaped representation, extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One function, `escape`, plus two convenience wrappers. Hand it raw bytes (a
pointer and a size, a `std::string`, a `std::string_view`, or a string-literal
array) and it returns a `std::string` you can safely print or log: control
bytes, high bytes, and the active quote character are escaped, everything else
passes through. It is the function you reach for when you want to show untrusted
or binary data in an error message or a log line without it mangling your
terminal.

It renders the way a C string literal would:

- `\n`, `\r`, `\t`, and `\\` get their named escapes.
- The active quote character (default `'`) is backslash-escaped inside the
  string.
- Any byte below `0x20` or at/above `0x7f` that has no named escape becomes
  `\xNN`, two lowercase hex digits.
- Everything else (printable ASCII) passes through unchanged.

By default the result is wrapped in single quotes (`'...'`). You can pick a
different quote character, or pass `'\0'` to get no surrounding quotes at all.

## Install

This is not header-only: the escaping loop lives in `escape.cc` and must be
compiled and linked. The header (`escape.hh`) only declares the function and the
two wrappers.

CMake with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  escape
  GIT_REPOSITORY https://github.com/Kronuz/escape.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(escape)

target_link_libraries(your_target PRIVATE escape::escape)
```

That pulls in its one dependency,
[char-classify](https://github.com/Kronuz/char-classify), transitively (it is
linked `PUBLIC`), so you don't declare it yourself. `CMakeLists.txt` requests
`cxx_std_20` `PUBLIC`, so consumers compile with C++20 too. Then:

```cpp
#include "escape.hh"
```

The header keeps its original Xapiand filename, so a codebase that already
`#include "escape.hh"` just needs this repo on its include path.

## Usage

```cpp
#include "escape.hh"

escape(std::string("hello"));        // "'hello'"
escape(std::string("it's"));         // "'it\\'s'"   (the active quote escaped)
escape(std::string("a\tb\nc"));      // "'a\\tb\\nc'"
escape(std::string("\x1b"));         // "'\\x1b'"     (ESC -> \xNN, lowercase)

// Pick the quote character.
escape(std::string("a\"b'c"), '"');  // "\"a\\\"b'c\""  (only " is escaped now)

// No surrounding quotes (and no character is treated as the quote char).
escape(std::string("hello"), '\0');  // "hello"

// Raw bytes, including embedded NULs, via (pointer, size).
const char buf[] = {'A', '\x00', 'B'};
escape(buf, 3);                      // "'A\\x00B'"
```

The convenience overloads forward to the primitive `escape(const void* p,
std::size_t size, char quote)`: one takes a `std::string` or `std::string_view`,
the other takes an rvalue string-literal array (and drops the trailing NUL). The
`quote` argument accepts `'\0'` (no quotes), `'\''`, `'"'`, or `'\1'` (a sentinel
that behaves exactly like `'\''`); other values trip an assertion.

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The test covers pass-through of printable text, the empty string, the named
escapes (`\n \r \t \\`), quote escaping under each quote mode (`'`, `"`, `\0`,
`\1`), the `\xNN` hex fallback for control and high bytes (asserting lowercase
hex and embedded NULs), and the string / string_view / array / (ptr, size)
overloads. It prints `all escape tests passed` and exits 0.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `escape.hh` /
`escape.cc` were copied verbatim, license headers and `#include "chars.hh"`
intact; the only change is the build wiring (this repo's `CMakeLists.txt` pulls
char-classify via `FetchContent` instead of finding it in Xapiand's tree). No
source decoupling was needed: the files had no `log.h`, opts, or config coupling
to begin with.

Its one dependency is [char-classify](https://github.com/Kronuz/char-classify),
whose `chars::char_repr` renders the two-char lowercase hex of a byte for the
`\xNN` fallback.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and [AGENTS.md](AGENTS.md)
for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE).
