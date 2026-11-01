# Changelog

All notable changes to this library. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The library has no
release tags yet, so entries are dated.

## 2026-11-01

### Added

- `bench/bench.cc` has rows for 2, 4, 7 and 15 printable bytes (either side of the 8-byte step and the
  16-byte block), 1 B without a quote, 4 B under the double quote, 3 B under quote `\1`, and for 2 B, 4 B,
  16 B and 4 KB that all need an escape under the double quote (the earlier rows missed
  regressions on these inputs). The frozen original and `escape` are sampled in turn instead of one after the
  other, so a drift of the host hits both, and a column A/A gives how far apart two sets of samples of `escape`
  are: the noise of the row.

### Changed

- **Input of fewer than 8 bytes is escaped by the entry point itself, a byte at a time.** The entry point used
  to save the registers of the block loops and reserve the stack for the table of a custom quote before it
  looked at the size, which cost 11% to 16% on 1 to 4 bytes. Now `escape()` tests the size first and, for the
  quotes with a shared table, takes the byte loops of char-classify's `chars_escape.hh`
  (`escape_measure_scalar`, `escape_emit_scalar`, with the tables of `escape_table_shared`) in one pass when the
  worst case, four bytes for each, fits a small string, and in two otherwise. Everything else goes to
  `escape_blocks`, out of line, which holds the table scratch and the block loops. Output is identical:
  `test/diff_test.cc` compares it with the frozen original for 1 to 70 bytes in every quote mode. Reverting the
  closing quote of either short path, the room for the quotes, the trim of the one-pass path, its worst case or
  the table chosen for the double quote each fails a test. char-classify is pinned at the commit that has the
  scalar functions.

### Measured

Against the frozen original (`test/escape_old.cc`), three runs of `escape_bench` each, `-O3`, Apple clang 17; A/A
is under 7% on every row but one (12% on the i9 at 256 B with 10% control bytes):

| workload | M4 Pro | i9-9980HK |
|---|---:|---:|
| printable 1 B | 1.02x-1.06x | 0.97x-1.02x |
| printable 1 B, no quote | 0.97x-1.03x | 1.01x-1.03x |
| printable 2 B | 1.14x-1.20x | 1.05x-1.10x |
| printable 4 B | 1.2x-1.4x | 1.17x-1.19x |
| printable 4 B, quote `"` | 1.27x-1.37x | 1.12x-1.13x |
| printable 3 B, quote `\1` | 1.10x-1.14x | 1.06x-1.12x |
| printable 7 B | 3.3x-3.7x faster | 2.9x-3.0x faster |
| printable 15 B | 2.9x-3.7x faster | 2.8x faster |
| binary 2 B, quote `"` | 1.02x-1.05x | 1.03x-1.04x |
| binary 4 B, quote `"` | 1.16x-1.28x | 1.11x-1.15x |
| binary 16 B, quote `"` | 1.08x-1.32x faster | 1.01x-1.06x faster |
| binary 4 KB, quote `"` | 3.4x-3.5x faster | 3.1x-3.2x faster |

One printable byte takes about 6.5 ns (M4 Pro) and 15.7 ns (i9) with either. No row is below 0.97x. The rows
from 256 B up are within the noise of the run before the change (printable 4 KB on the M4 Pro: 8.1x to 10.4x
faster than the original then and now). The 2026-10-31 table compares with the previous 8-byte kernel, which is
not in the repository.

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
  `absl::CEscape` was 2.3x faster. The 64 KB mixed row is now 3.7x faster than
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
