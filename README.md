# escape

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/escape/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/escape/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Escape a byte buffer the way a C string literal would, in C++20: `\n`, `\t`,
`\\`, the quote, and `\xNN` for everything else unprintable, so untrusted or
binary data is safe to print and log.

![escape: raw bytes in, one escaped literal out](docs/escape.svg)

## ✨ Highlights

- **C-literal escapes**: `\n`, `\r`, `\t`, `\\`, the active quote, and lowercase
  `\xNN`.
- **Size-based**, so embedded NULs are shown.
- **Quotes your way**: single, double, or none.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it compiles one source file.
CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus
ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(escape
  GIT_REPOSITORY https://github.com/Kronuz/escape.git
  GIT_TAG        main)
FetchContent_MakeAvailable(escape)

target_link_libraries(my_app PRIVATE escape::escape)
```

## 🚀 Quick start

```cpp
#include "escape.hh"

#include <cstdio>
#include <string>

int main() {
	std::puts(escape(std::string("it's\t\x1b")).c_str());   // 'it\'s\t\x1b'
	std::puts(escape(std::string("hello"), '\0').c_str());  // hello
}
```

## 🎯 When to use it

- ✅ Showing untrusted bytes in error messages and logs.
- ⚠️ [repr](https://github.com/Kronuz/repr) does the same and more (truncation,
  an all-hex mode); prefer it for new code.

## ⚡ Performance

`escape` sizes its result exactly (a table gives each byte's output length), then
tests the input a block at a time (16 bytes with NEON or SSE2, 8 with a portable
fallback): a clean block is copied whole and the escapes of any other block are
written from the table. The kernel is [char-classify](https://github.com/Kronuz/char-classify)'s
`chars_escape.hh`, shared with `repr`. `absl::CEscape` writes a different format, so
the benchmark uses it as a speed reference only.

```sh
cmake -B build -DESCAPE_BUILD_BENCH=ON && cmake --build build && ./build/escape_bench
```

Time per call, `-O3`, Apple clang 17, on an Apple M4 Pro and an Intel i9-9980HK
(minimum over interleaved samples; `escape_bench` prints the same rows):

| input | M4 Pro: `escape` / `absl::CEscape` | i9-9980HK: `escape` / `absl::CEscape` |
|---|---:|---:|
| printable 16 B | 8 ns / 10 ns | 21 ns / 16 ns |
| printable 256 B | 47 ns / 183 ns | 159 ns / 382 ns |
| printable 4 KB | 422 ns / 2.2 us | 890 ns / 4.1 us |
| printable 64 KB | 7.0 us / 35.5 us | 23.0 us / 64.9 us |
| mixed 10% 64 KB | 13.2 us / 35.4 us | 38.8 us / 62.7 us |
| 10% control bytes, 4 KB | 839 ns / 2.2 us | 1.7 us / 3.9 us |
| binary 4 KB | 1.7 us / 2.3 us | 3.1 us / 4.0 us |

`absl::CEscape` is faster for inputs of a few bytes (up to 8 on the M4 Pro, up to
about 40 on the i9). From 256 bytes up, `escape` is ahead on every row measured.

`ctest` checks the output byte for byte against a frozen copy of the previous
implementation, in the vector and the portable (`CHARS_NO_SIMD`) build.
[CHANGELOG.md](CHANGELOG.md) has the speedups over it.

## 📚 Documentation

- [docs/API.md](docs/API.md): the overloads and the quoting rules.
- [ARCHITECTURE.md](ARCHITECTURE.md): the escaping table.
- [AGENTS.md](AGENTS.md): repository map and traps.
- [CHANGELOG.md](CHANGELOG.md): what changed, and when.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) first. CI must
pass on every platform.

## 📄 License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE). Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).
