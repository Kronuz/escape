/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "escape.hh"

#include <cassert>               // for assert
#include <cstdint>               // for std::uint64_t
#include <cstring>               // for std::memcpy

#include "chars.hh"              // for chars::char_repr


// How escape works (see ARCHITECTURE.md; the same core as repr()):
//
// 1. A 256-entry table gives the output length of each byte (1 = copied as is,
//    2 = backslash escape, 4 = \xNN), per quote mode.
// 2. A first pass sums the lengths of the bytes, so the
//    result is allocated once at its exact size (no 4n+2 zero-fill).
// 3. A second pass writes it. Eight bytes at a time are tested with SWAR for
//    "all plain printable" and bulk-copied; anything else goes byte by byte
//    through the table.

namespace {

struct Tab {
	unsigned char len[256];  // output length of a byte
	unsigned char rep[256];  // second byte of a two-byte escape
};

constexpr Tab make_tab(char quote) {
	Tab t{};
	for (int i = 0; i < 256; ++i) {
		unsigned char c = static_cast<unsigned char>(i);
		t.len[i] = 1;
		t.rep[i] = c;
		if (c == '\n') { t.len[i] = 2; t.rep[i] = 'n'; }
		else if (c == '\r') { t.len[i] = 2; t.rep[i] = 'r'; }
		else if (c == '\t') { t.len[i] = 2; t.rep[i] = 't'; }
		else if (c == '\\') { t.len[i] = 2; }
		else if (quote != '\0' && c == static_cast<unsigned char>(quote)) { t.len[i] = 2; }
		else if (c < ' ' || c >= 0x7f) { t.len[i] = 4; }
	}
	return t;
}

constexpr Tab tab_none = make_tab('\0');
constexpr Tab tab_single = make_tab('\'');
constexpr Tab tab_double = make_tab('"');

constexpr std::uint64_t ones = 0x0101010101010101ULL;
constexpr std::uint64_t highs = 0x8080808080808080ULL;

constexpr std::uint64_t has_zero(std::uint64_t v) { return (v - ones) & ~v & highs; }

// True when any of the 8 bytes is not plain printable: < 0x20, >= 0x7f,
// a backslash or the quote. Exact as a boolean (carries only come from bytes
// that are flagged anyway).
inline bool dirty8(std::uint64_t x, std::uint64_t q8) {
	return (has_zero(x ^ (ones * '\\')) | has_zero(x ^ q8) |
		((x - ones * 0x20) & ~x & highs) | (((x + ones) | x) & highs)) != 0;
}

inline std::size_t measure(const unsigned char* b, const unsigned char* e, const Tab& t, std::uint64_t q8) {
	std::size_t n = 0;
	while (e - b >= 8) {
		std::uint64_t x;
		std::memcpy(&x, b, 8);
		if (!dirty8(x, q8)) {
			n += 8;
		} else {
			for (int i = 0; i < 8; ++i) { n += t.len[b[i]]; }
		}
		b += 8;
	}
	while (b != e) { n += t.len[*b++]; }
	return n;
}

inline char* emit_byte(char* d, unsigned char c, const Tab& t) {
	switch (t.len[c]) {
		case 1:
			*d++ = static_cast<char>(c);
			break;
		case 2:
			*d++ = '\\';
			*d++ = static_cast<char>(t.rep[c]);
			break;
		default:
			*d++ = '\\';
			*d++ = 'x';
			chars::char_repr(static_cast<char>(c), &d);
			break;
	}
	return d;
}

inline char* emit(char* d, const unsigned char* b, const unsigned char* e, const Tab& t, std::uint64_t q8) {
	while (e - b >= 8) {
		std::uint64_t x;
		std::memcpy(&x, b, 8);
		if (!dirty8(x, q8)) {
			std::memcpy(d, b, 8);
			d += 8;
		} else {
			for (int i = 0; i < 8; ++i) { d = emit_byte(d, b[i], t); }
		}
		b += 8;
	}
	while (b != e) { d = emit_byte(d, *b++, t); }
	return d;
}

}  // namespace


std::string escape(const void* p, size_t size, char quote) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* b = static_cast<const unsigned char *>(p);
	const unsigned char* const e = b + size;
	if (quote == '\1') { quote = '\''; }

	const Tab* t = &tab_none;
	Tab custom;
	if (quote == '\'') {
		t = &tab_single;
	} else if (quote == '"') {
		t = &tab_double;
	} else if (quote != '\0') {
		custom = make_tab(quote);  // not reachable under the assert
		t = &custom;
	}
	const std::uint64_t q8 = ones * static_cast<unsigned char>(quote);

	const std::size_t n = (quote != '\0' ? 2 : 0) + measure(b, e, *t, q8);
	std::string ret;
	ret.resize(n);
	char* d = &ret[0];
	if (quote != '\0') { *d++ = quote; }
	d = emit(d, b, e, *t, q8);
	if (quote != '\0') { *d++ = quote; }
	assert(static_cast<std::size_t>(d - &ret[0]) == n);
	return ret;
}
