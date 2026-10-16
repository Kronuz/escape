# escape API reference

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
