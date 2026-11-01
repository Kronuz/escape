# Changelog

All notable changes to this library. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The library has no
release tags yet, so entries are dated.

## 2026-10-31

### Changed

- **`escape` tests sixteen bytes at a time on x86 as well as arm64.** The measuring
  and writing loops moved into char-classify's `chars_escape.hh`, shared with `repr`,
  which tests a 16-byte block with NEON or SSE2 (8 bytes with the portable
  fallback). A clean block is one 16-byte store; a block with up to five escapes is
  written from the table; a block with more goes byte by byte. Output is identical to
  the previous implementation.

  The 2026-10-30 gain (3.5x for 10% non-printable input) was measured on Apple silicon
  only. On x86 the 8-byte SWAR kernel gained nothing on 64 KB mixed input, and
  `absl::CEscape` was 2.3x faster (audit CR5). The 64 KB mixed row is now 3.7x faster than
  that kernel on the i9 and 1.6x faster than `absl::CEscape`.

  Speedup over the previous kernel (minimum over three rounds of nine interleaved
  samples, `-O3`, Apple clang 17; a ratio under 1.00x is slower):

  | workload | M4 Pro (arm64) | i9-9980HK (x86) |
  |---|---:|---:|
  | printable 1 B | 1.11x | 1.00x |
  | printable 8 B | 1.00x | 1.03x |
  | printable 16 B | 0.99x | 1.10x |
  | printable 256 B | 1.64x | 1.73x |
  | printable 4 KB | 2.16x | 2.96x |
  | printable 64 KB | 2.10x | 2.42x |
  | mixed 10% 256 B | 1.53x | 1.47x |
  | mixed 10% 64 KB | 2.13x | 3.70x |
  | 1% control bytes, 4 KB | 2.09x | 2.73x |
  | 10% control bytes, 4 KB | 2.02x | 2.30x |
  | 50% control bytes, 4 KB | 2.25x | 1.84x |
  | binary 64 B | 1.35x | 1.17x |
  | binary 4 KB | 1.87x | 1.71x |

  Against `absl::CEscape` (another format), `escape` is faster by 1.3x (binary 4 KB)
  to 5.3x (printable 4 KB) on both machines for inputs of 256 bytes and up, and slower
  for inputs of a few bytes (up to 8 on the M4 Pro, 0.77x at 1 B; up to about 40 on the
  i9, 0.57x at 1 B).
  Other hosts (GitHub runners on Apple silicon, Neoverse, AMD EPYC and Intel Xeon)
  agree in direction: 1.3x to 3.1x faster than `absl::CEscape` on 64 KB mixed input.

### Added

- `test/diff_test.cc` compares windows of every length from 1 to 70 bytes against the
  frozen copy, so the vector loop and its tail are exercised (the earlier cases stopped
  at 17 bytes), and pairs of adjacent escapes. `escape_swar` and `escape_diff_swar` run
  the same tests with `CHARS_NO_SIMD`.
- `bench/bench.cc` rows cover 1% to 50% control bytes and all-binary input.

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
