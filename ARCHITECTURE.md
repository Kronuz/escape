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

The function sizes the output buffer up front and writes through a raw pointer,
never reallocating mid-loop:

```cpp
size = size * 4 + 2;  // worst case: every byte becomes "\xNN", plus two quotes
std::string ret;
ret.resize(size);
char* d = &ret[0];
```

Four bytes per input byte is the worst case (`\xNN`), and the `+ 2` covers the
opening and closing quote. Because the buffer is large enough for the worst case,
the loop can write with a bare `*d++ =` and never bounds-check. At the end it
shrinks the string to what was actually written: `ret.resize(d - buff)`.

The opening quote (if any) is written before the loop, the closing quote after.
Inside the loop a `switch` on the byte handles the named escapes (`\n`, `\r`,
`\t`, `\\`), and the `default` arm handles the rest in priority order:

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
for the worst case up front trades a little slack memory (freed by the final
`resize`) for a loop with no reallocation and no per-byte capacity check. The
escaping rules match C string literals so the output is both human-readable and,
for the common cases, paste-able. The hex fallback is ASCII-oriented by design:
bytes at or above `0x7f` are escaped rather than passed through, so the result is
plain 7-bit ASCII no matter what went in, which is the right default for a
terminal or a log file. Leaning on `chars::char_repr` for the hex keeps the hex
rendering identical to the rest of the Kronuz family instead of reimplementing a
nibble-to-hex table here.
