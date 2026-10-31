# Changelog

All notable changes to this library. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The library has no
release tags yet, so entries are dated.

## 2026-10-30

### Changed

- **`escape` sizes its result exactly and copies printable blocks eight bytes at
  a time.** The old loop zero-filled a `4n + 2` buffer and wrote one byte at a
  time. A per-quote 256-entry length table drives a measuring pass, so the result
  is resized once, and the writer bulk-copies eight-byte blocks of plain
  printable bytes (a SWAR test) and escapes other blocks byte by byte. Output is
  identical to the previous implementation.

  Apple silicon, Apple clang 17, `-O3`, against the previous implementation:

  | workload | speedup |
  |---|---|
  | printable text | 3.8-6x (4 KB: 5.5 us to 0.91 us; `absl::CEscape`, a different format, 2.3 us) |
  | 10% non-printable | 3.5x |
  | all binary | 1.45x |

### Added

- `test/diff_test.cc` compares `escape()` byte for byte with a frozen copy of the
  previous implementation (`test/escape_old.cc`) on random and edge inputs in
  every quote mode.
- `bench/bench.cc` (`-DESCAPE_BUILD_BENCH=ON`).
