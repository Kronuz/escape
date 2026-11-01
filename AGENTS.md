# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
escape.hh        Declares escape(const void*, size_t, char) + string/string_view and rvalue-array wrappers. Header. Verbatim from Xapiand.
escape.cc        The entry point: quote handling, exact sizing, the quotes. The escaping kernel is char-classify's chars_escape.hh. Compiled; the library is NOT header-only.
test/test.cc     Runnable smoke test: pass-through, named escapes, \xNN fallback, quote modes, overloads.
test/diff_test.cc  Byte-for-byte comparison with test/escape_old.cc, the frozen pre-table implementation (never edit it).
bench/bench.cc   Benchmark vs the frozen implementation and absl::CEscape when installed (ESCAPE_BUILD_BENCH); edge rows for sizes around the 8-byte step and the 16-byte block and for all-escape bytes under the double quote.
CMakeLists.txt   STATIC library `escape` (+ alias escape::escape); FetchContent dep char-classify; CTest tests `escape`, `escape_diff` and their portable-backend twins `escape_swar`, `escape_diff_swar`.
LICENSE          MIT, Copyright (c) 2026 Germán Méndez Bravo and (c) 2015-2020 Dubalu LLC.
CHANGELOG.md     Behavior and performance changes, dated.
README.md        What it is, install, quick start.
docs/API.md      Usage and the quoting rules.
ARCHITECTURE.md  Internal design of the two passes and the quote argument.
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
`FetchContent` at the commit `CMakeLists.txt` pins and linked `PUBLIC`. `escape.cc`
includes `"chars_escape.hh"`: the length table, the measuring pass and the writer,
shared with `repr`, which use `chars_simd.hh` for the block tests and
`chars::char_repr` for the `\xNN` fallback. Because the link is `PUBLIC`, a consumer that
links `escape::escape` gets char-classify's include path transitively and does
not declare it separately.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC` to stay uniform with
  char-classify and the rest of the family.
- **One real dependency: char-classify.** Don't add others, and don't reach for
  `<cctype>` or a hand-rolled hex table or escaping loop; the kernel and the hex
  rendering come from char-classify so `escape` and `repr` match each other and the family.
- **Filenames are stable.** The header and source keep their original Xapiand
  names (`escape.hh`, `escape.cc`) so a consumer that already `#include`s the
  header just needs this repo on the include path. Don't rename them.
- Tabs for indentation, double quotes in code, no em dashes in prose.

## Load-bearing invariants

- **The output is sized exactly, from the same table that writes it.** `escape.cc`
  measures with `chars::escape_measure`, calls `ret.resize(n)` once and writes with
  `chars::escape_emit`, which stores through a bare pointer inside the measured room
  (an `assert` checks that exactly `n` bytes were written). The rules live in
  char-classify's `chars_escape.hh`; change an escape there, in the table, the writer
  and the vector test together (its AGENTS.md says how), not here.
- **`test/diff_test.cc` pins the behavior.** It compares `escape()` with the frozen
  table-less implementation (`test/escape_old.cc`) on random and edge inputs in every
  quote mode.
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

- **Add a named escape (e.g. `\a`, `\b`, `\f`, `\v`).** It is a change to the kernel in
  char-classify: give the byte length 2 and its letter in `rep` in `make_escape_table`
  (control bytes are already flagged by the block tests, so nothing else decides which
  blocks need the table), then bump this repo's pin. The two-byte count in
  `escape_measure` lists the named escapes, so it changes with them. A named escape
  changes the output, so `test/diff_test.cc` fails until it allows the changed bytes.
- **Always extend the smoke test.** When you add or change an escape rule, assert
  the exact output string for a representative input in `test/test.cc`, and
  include an embedded-NUL or high-byte case if it's relevant.

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
  writes two lowercase hex chars. The kernel's measuring pass counts those bytes (length 4
  per `\xNN`); if you change how the hex is written, recheck the table there.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). `escape.hh` is the upstream header;
`escape.cc` keeps the upstream behavior and output with the table-driven writer in
`CHANGELOG.md`. The build wiring pulls char-classify through FetchContent instead
of finding it in Xapiand's tree. Neither file has `log.h`, opts, or config
coupling. Keep it that way; any change here should be reconcilable with upstream
as a plain edit.
