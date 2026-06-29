// Smoke test for the standalone escape library.
//
// Exercises escape.hh / escape.cc: turning an arbitrary byte buffer into a
// printable, C-string-style escaped representation, optionally wrapped in a
// quote character. Covers the named escapes (\n \r \t \\), quote escaping under
// each quote mode ('\'', '"', '\0' for no quotes, '\1' aliased to '\''), the
// \xNN hex fallback for control and high bytes, pass-through of printable ASCII,
// the empty input, and the string / string_view / array overloads.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

#include "escape.hh"


// ---------------------------------------------------------------------------
// Default quote: single quote. Printable ASCII passes through, wrapped in '...'.
// ---------------------------------------------------------------------------

static void test_default_quote() {
	// Plain printable text passes through untouched, surrounded by single quotes.
	assert(escape(std::string("hello")) == "'hello'");

	// The empty string is just the two quotes.
	assert(escape(std::string("")) == "''");

	// An embedded single quote (the active quote char) is backslash-escaped.
	assert(escape(std::string("it's")) == "'it\\'s'");

	// A double quote is NOT the active quote char here, so it passes through.
	assert(escape(std::string("say \"hi\"")) == "'say \"hi\"'");

	std::printf("escape default-quote OK: pass-through, empty, quote escaping\n");
}


// ---------------------------------------------------------------------------
// The named escapes: newline, carriage return, tab, backslash.
// ---------------------------------------------------------------------------

static void test_named_escapes() {
	assert(escape(std::string("\n")) == "'\\n'");
	assert(escape(std::string("\r")) == "'\\r'");
	assert(escape(std::string("\t")) == "'\\t'");
	assert(escape(std::string("\\")) == "'\\\\'");

	// A mix in one go, in order: a\tb\nc -> 'a\tb\nc'.
	assert(escape(std::string("a\tb\nc")) == "'a\\tb\\nc'");

	std::printf("escape named-escapes OK: \\n \\r \\t \\\\\n");
}


// ---------------------------------------------------------------------------
// The \xNN hex fallback for control bytes (< 0x20) and high bytes (>= 0x7f).
// char_repr writes LOWERCASE hex, so the output is \x1b, \xff, etc.
// ---------------------------------------------------------------------------

static void test_hex_fallback() {
	// ESC (0x1b) is a control byte with no named escape -> \x1b (lowercase).
	assert(escape(std::string("\x1b")) == "'\\x1b'");

	// NUL (0x00) inside the buffer -> \x00. Build the input via (ptr, size) so the
	// embedded NUL is part of the data, not a string terminator.
	const char nul[] = {'\x00'};
	assert(escape(nul, 1) == "'\\x00'");

	// DEL (0x7f) is >= 0x7f, so it is escaped too.
	assert(escape(std::string("\x7f")) == "'\\x7f'");

	// A high byte (0xff) -> \xff, lowercase.
	const char high[] = {static_cast<char>(0xff)};
	assert(escape(high, 1) == "'\\xff'");

	// Mixed: printable 'A' (0x41) stays, 0x1b becomes \x1b.
	const char mixed[] = {'A', '\x1b', 'B'};
	assert(escape(mixed, 3) == "'A\\x1bB'");

	std::printf("escape hex-fallback OK: \\x00 \\x1b \\x7f \\xff (lowercase)\n");
}


// ---------------------------------------------------------------------------
// Quote modes: '"' as the active quote, '\0' for no surrounding quotes, '\1'
// aliased to '\''.
// ---------------------------------------------------------------------------

static void test_quote_modes() {
	// Double-quote mode: wrap in "...", and a literal " in the data is escaped,
	// while a single quote now passes through.
	assert(escape(std::string("a\"b'c"), '"') == "\"a\\\"b'c\"");

	// No-quote mode ('\0'): no surrounding quotes, and neither ' nor " is treated
	// as the quote char, so both pass through. The escapes for \n etc. still apply.
	assert(escape(std::string("it's \"x\"\n"), '\0') == "it's \"x\"\\n");
	assert(escape(std::string("hello"), '\0') == "hello");

	// '\1' is a sentinel that behaves exactly like the single-quote mode.
	assert(escape(std::string("it's"), '\1') == escape(std::string("it's"), '\''));

	std::printf("escape quote-modes OK: '\"', '\\0' (no quotes), '\\1' (alias of ')\n");
}


// ---------------------------------------------------------------------------
// Overloads: rvalue char[] array, std::string, std::string_view, (ptr, size).
// ---------------------------------------------------------------------------

static void test_overloads() {
	// The array overload binds an rvalue char array and drops the trailing NUL
	// (N - 1). A named array is an lvalue and does not match it, so move it.
	char arr[] = "ab\tc";
	assert(escape(std::move(arr)) == "'ab\\tc'");

	// std::string_view overload.
	std::string_view sv = "x\ny";
	assert(escape(sv) == "'x\\ny'");

	// Raw (pointer, size) overload, the primitive everything else forwards to.
	const char* raw = "z\t";
	assert(escape(raw, 2) == "'z\\t'");

	// std::string and string_view of the same bytes agree.
	std::string s = "p\rq";
	assert(escape(s) == escape(std::string_view(s)));

	std::printf("escape overloads OK: literal array, string, string_view, (ptr,size)\n");
}


int main() {
	test_default_quote();
	test_named_escapes();
	test_hex_fallback();
	test_quote_modes();
	test_overloads();
	std::printf("all escape tests passed\n");
	return 0;
}
