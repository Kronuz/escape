# Architecture

The internals of `escape`: a single pass over a byte buffer that writes a
printable, C-string-style escaped representation into a preallocated string. For
usage see `README.md`; for the repo map and invariants see `AGENTS.md`.

## Shape

One compiled function and two header-only wrappers:

```
  escape.hh   declares escape(const void*, size_t, char) + string/array wrappers
  escape.cc   the single-pass escaping loop
```

The wrappers in the header just forward into the one real function:

```cpp
// std::string / std::string_view -> (data, size)
template <typename T, typename = /* enable_if string or string_view */>
inline std::string escape(T&& s, char quote = '\'') {
    return escape(s.data(), s.size(), quote);
}

// rvalue string-literal array -> (data, N - 1), dropping the trailing NUL
template<typename T, std::size_t N>
inline std::string escape(T (&&s)[N], char quote = '\'') {
    return escape(s, N - 1, quote);
}
```

So there is exactly one place the escaping logic lives, `escape(const void* p,
size_t size, char quote)` in `escape.cc`, and everything else is sugar over it.

## The single pass

The output is allocated once at its exact size and written through a raw
pointer, never reallocating mid-loop, and without zero-filling a worst-case
buffer:

1. A 256-entry table (one per quote mode: none, `'`, `"`) gives each byte's output
   length: 1 (copied), 2 (`\n`, `\r`, `\t`, `\\`, the active quote) or 4 (`\xNN`).
2. A first pass sums those lengths, so `ret.resize(n)` is exact (plus the quotes).
3. A second pass writes the result. It tests eight bytes at a time (one 64-bit
   load, SWAR) for "none below 0x20 or at/above 0x7f, no backslash, no quote";
   a clean block is copied with one 8-byte store, and any other block goes byte by
   byte through the table, so escape-heavy input pays one extra test per eight bytes.

The opening quote (if any) is written before the loop, the closing quote after.
For a byte that is not copied, the rules, in priority order, are:

1. if a quote character is active and the byte equals it, write `\` then the
   quote;
2. else if the byte is below `0x20` or at/above `0x7f`, write `\x` and then two
   lowercase hex digits via `chars::char_repr`;
3. else write the byte verbatim.

`char_repr` (from char-classify) takes a `char**` and advances it by two, which
is exactly the write-pointer idiom the loop already uses, so the hex fallback
drops straight in.

## The quote argument

`quote` is asserted to be one of four values: `'\0'`, `'\1'`, `'\''`, or `'"'`.

- `'\0'` means *no surrounding quotes*. The function writes no opening or closing
  quote, and because the "is this byte the quote char?" test is guarded by
  `quote != '\0'`, no character is treated as a quote to escape either. You get
  the bare escaped body.
- `'\1'` is a sentinel that is rewritten to `'\''` right after the buffer is
  sized (`if (quote == '\1') { quote = '\''; }`). From there it is
  indistinguishable from passing `'\''`. It exists so a caller can spell
  "single-quote mode" without writing a literal quote character.
- `'\''` and `'"'` are the two ordinary quote characters: that character wraps
  the output and is backslash-escaped wherever it appears inside.

Anything else trips the assertion. The escaping of `\n` / `\r` / `\t` / `\\` and
the `\xNN` fallback happen regardless of the quote mode.

## Why this shape

The whole function is one allocation and one linear pass, which is what you want
for something that runs on the hot path of logging and error formatting. Sizing
the result exactly (one cheap counting pass) avoids both reallocation and the
zero-fill of a worst-case buffer, and block-wise copying makes printable text run at
memory speed. The
escaping rules match C string literals so the output is both human-readable and,
for the common cases, paste-able. The hex fallback is ASCII-oriented by design:
bytes at or above `0x7f` are escaped rather than passed through, so the result is
plain 7-bit ASCII no matter what went in, which is the right default for a
terminal or a log file. Leaning on `chars::char_repr` for the hex keeps the hex
rendering identical to the rest of the Kronuz family instead of reimplementing a
nibble-to-hex table here.
