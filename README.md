# escape

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/escape/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/escape/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

C-style escaping of byte buffers for C++20: backslash and \xNN escapes in one exact-size allocation.
A byte comes out the way a C string literal would write it: `\n`, `\r`, `\t`,
`\\`, the active quote, and `\xNN` for anything else outside printable ASCII. A
table gives each byte's output length, so the result is sized once, and clean
blocks of 16 bytes are copied whole. Reach for it to quote untrusted or binary
data in an error message or a log line.

## ✨ Highlights

- **C-literal escapes.** `\n`, `\r`, `\t`, `\\`, the active quote, and lowercase
  `\xNN`; the output is plain 7-bit ASCII.
- **Size-based.** Embedded NULs are written as `\x00`, never taken as the end.
- **Quotes your way.** Single (the default), double, or none (`'\0'`); only the
  active quote is escaped.
- **One allocation per call.** The output is measured with the same table that
  writes it.
- **Vector block scan.** 16 bytes per step with NEON or SSE2, 8 with the portable
  backend, through [char-classify](https://github.com/Kronuz/char-classify)'s
  escaping kernel, shared with [repr](https://github.com/Kronuz/repr).

## 🚀 Quick start

```cpp
#include "escape.hh"

#include <cstdio>
#include <string>

int main() {
	std::string data("it's\t\x1b\0", 7);                  // a tab, ESC and a NUL
	std::puts(escape(data).c_str());                        // 'it\'s\t\x1b\x00'
	std::puts(escape(data, '"').c_str());                   // "it's\t\x1b\x00"
	std::puts(escape(std::string_view("a\\b"), '\0').c_str());   // a\\b
}
```

`escape("literal")` does not compile: the array overload binds only rvalue
arrays. Pass a `std::string` or a `std::string_view`.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; CMake fetches
[char-classify](https://github.com/Kronuz/char-classify) and compiles one source
file (`escape.cc`). CI builds it on Linux (x86_64, arm64), macOS (arm64,
x86_64), and FreeBSD, plus ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(escape
  GIT_REPOSITORY https://github.com/Kronuz/escape.git
  GIT_TAG        f44a2396743594fc289a039d2cd60a272aefe3c2)  # or main for the tip
FetchContent_MakeAvailable(escape)

target_link_libraries(my_app PRIVATE escape::escape)
```

```cpp
#include "escape.hh"
```

## 🎯 When to use it

Use it to show bytes you cannot trust to be printable as one quoted C-style
string.

Not for:

- **Unescaping.** There is no inverse here.
- **Long buffers in logs.** [repr](https://github.com/Kronuz/repr) writes the
  same escapes and adds head-and-tail truncation and an all-hex mode.
- **Other formats.** JSON, shell and URLs have their own rules; use their
  escapers.
- **The fewest nanoseconds on tiny inputs.** `absl::CEscape` is faster up to 8
  bytes on the M4 Pro and up to about 40 on the i9.

## ⚡ Performance

Time per call from `escape_bench`, the best of seven samples of at least 8 ms, `-O3`, Apple clang 17.
Each machine column holds two times, `escape` / [`absl::CEscape`](https://abseil.io/docs/cpp/guides/strings)
from [abseil](https://github.com/abseil/abseil-cpp/releases/tag/20260817.0) 20260817.0 (Homebrew).
`absl::CEscape` writes octal escapes where `escape` writes `\xNN`, so its output differs and it is
a speed reference only. Rows are the input: printable text of the given length, text with 10% random
bytes or 10% control characters, and bytes from `0x80` up (every byte needs an escape).

| input | Apple M4 Pro: `escape` / `absl::CEscape` | Intel Core i9-9980HK: `escape` / `absl::CEscape` |
|---|---:|---:|
| printable 16 B | 8 ns / 10 ns | 21 ns / 16 ns |
| printable 256 B | 47 ns / 183 ns | 159 ns / 382 ns |
| printable 4 KB | 422 ns / 2.2 us | 890 ns / 4.1 us |
| printable 64 KB | 7.0 us / 35.5 us | 23.0 us / 64.9 us |
| 10% random bytes, 64 KB | 13.2 us / 35.4 us | 38.8 us / 62.7 us |
| 10% control bytes, 4 KB | 839 ns / 2.2 us | 1.7 us / 3.9 us |
| bytes from `0x80` up, 4 KB | 1.7 us / 2.3 us | 3.1 us / 4.0 us |

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DESCAPE_BUILD_BENCH=ON
cmake --build build --target escape_bench
./build/escape_bench
```

From 256 bytes up `escape` is ahead on every row.

## 📚 Documentation

- [docs/API.md](docs/API.md): the overloads and the quoting rules.
- [ARCHITECTURE.md](ARCHITECTURE.md): the two passes and the quote argument.
- [CHANGELOG.md](CHANGELOG.md): what changed, with numbers.
- [AGENTS.md](AGENTS.md): repository map, invariants, and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#escape): the catalog entry.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

`escape_diff` compares every output with the frozen original, and the `*_swar`
tests run both again on the portable backend. Add `-DESCAPE_BUILD_BENCH=ON` to
build `escape_bench`; with abseil installed it adds the `absl::CEscape` column.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the
catalog's [rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and Copyright (c) 2015-2020 Dubalu
LLC. See [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).
