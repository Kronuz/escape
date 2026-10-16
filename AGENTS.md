# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
escape.hh        Declares escape(const void*, size_t, char) + string/string_view and rvalue-array wrappers. Header. Verbatim from Xapiand.
escape.cc        The single-pass escaping loop. Compiled; the library is NOT header-only. Verbatim from Xapiand.
test/test.cc     Runnable smoke test: pass-through, named escapes, \xNN fallback, quote modes, overloads.
CMakeLists.txt   STATIC library `escape` (+ alias escape::escape); FetchContent dep char-classify; CTest test `escape`.
LICENSE          MIT, Copyright (c) 2015-2019 Dubalu LLC.
README.md        What it is, install, quick start.
docs/API.md      Usage and the quoting rules.
ARCHITECTURE.md  Internal design of the single pass and the quote argument.
```

The escaping logic lives in `escape.cc` and must be compiled and linked. The
header only declares the primitive and two thin wrappers that forward into it.

## Build and run the test

```sh
cmake -B build -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
cmake --build build
ctest --test-dir build
```

The first configure fetches char-classify over the network (FetchContent), so it
needs connectivity. Expected output ends with `all escape tests passed`, exit 0.
The test target is `escape_test`; the registered CTest name is `escape`. The test
target is only added when this repo is the top-level project (CMakeLists.txt), so
consumers vendoring it via `FetchContent` won't build it.

## Dependency

The one dependency is
[char-classify](https://github.com/Kronuz/char-classify), pulled via
`FetchContent` with `GIT_TAG main` and linked `PUBLIC`. `escape.cc` includes
`"chars.hh"` and calls `chars::char_repr` to render the two-char lowercase hex of
a byte for the `\xNN` fallback. Because the link is `PUBLIC`, a consumer that
links `escape::escape` gets char-classify's include path transitively and does
not declare it separately.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC` to stay uniform with
  char-classify and the rest of the family.
- **One real dependency: char-classify.** Don't add others, and don't reach for
  `<cctype>` or a hand-rolled hex table; the hex rendering goes through
  `chars::char_repr` so it matches the family.
- **Filenames are stable.** The header and source keep their original Xapiand
  names (`escape.hh`, `escape.cc`) so a consumer that already `#include`s the
  header just needs this repo on the include path. Don't rename them.
- Tabs for indentation, double quotes in code, no em dashes in prose.

## Load-bearing invariants

- **The output is sized for the worst case, then written without bounds checks.**
  `size * 4 + 2` reserves room for every byte becoming `\xNN` plus two quotes, so
  the loop writes with a bare `*d++ =` and the final `ret.resize(d - buff)`
  shrinks to the real length. If you add an escape sequence longer than four
  characters per input byte, you must widen that reservation or the writes run
  off the end.
- **The `quote` argument is asserted to one of four values.** `'\0'` (no
  surrounding quotes, and no character treated as a quote to escape), `'\1'`
  (rewritten to `'\''`), `'\''`, or `'"'`. Anything else trips the assertion.
  Keep the assert and the `'\1'` -> `'\''` rewrite.
- **`'\0'` mode escapes nothing as a quote.** The "is this byte the quote char?"
  test is guarded by `quote != '\0'`, so in no-quote mode both `'` and `"` pass
  through. Don't drop that guard.
- **High bytes (>= 0x7f) and control bytes (< 0x20) are escaped, not passed
  through.** This is what keeps the output plain 7-bit ASCII. The `\xNN` hex is
  lowercase, because that's what `chars::char_repr` writes.

## How to extend

- **Add a named escape (e.g. `\a`, `\b`, `\f`, `\v`).** The `switch` in
  `escape.cc` already has these commented out. Uncomment or add a `case` that
  writes the two-byte sequence. Each stays within the four-bytes-per-input-byte
  budget, so no buffer-size change is needed.
- **Always extend the smoke test.** `test/test.cc` is the only executable check.
  When you add or change an escape rule, assert the exact output string for a
  representative input, and include an embedded-NUL or high-byte case if it's
  relevant.

## Traps

- **The rvalue-array overload only binds rvalue arrays.** `escape("literal")`
  does NOT compile: a string literal is an lvalue, and the array overload is
  `T (&&s)[N]`. Pass a `std::string` / `std::string_view`, or `std::move` a named
  `char[]`. The test uses `std::move(arr)` for exactly this reason. This is the
  original Xapiand signature; don't "fix" it here.
- **Embedded NULs are real data.** The primitive takes `(pointer, size)`, so a
  `\x00` inside the buffer is escaped like any other control byte. Build such
  inputs with an explicit size, not from a C string, or the NUL terminates early.
- **`char_repr` advances the write pointer by two.** It takes a `char**` and
  writes two lowercase hex chars. The worst-case buffer sizing already accounts
  for it; if you change how the hex is written, recheck the `size * 4 + 2`
  reservation.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `escape.hh` and `escape.cc` were
copied verbatim, license headers and `#include "chars.hh"` intact; the only delta
is the build wiring (FetchContent pulls char-classify instead of finding it in
Xapiand's tree). There was no source decoupling: the files had no `log.h`, opts,
or config coupling. Keep it that way; any change here should be reconcilable with
upstream as a plain edit.
