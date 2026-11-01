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

#include "chars_escape.hh"       // for chars::escape_measure, chars::escape_emit


// How escape works (see ARCHITECTURE.md; the kernel is char-classify's chars_escape.hh,
// shared with repr()):
//
// 1. A 256-entry table gives the output length of each byte (1 = copied as is,
//    2 = backslash escape, 4 = \xNN), per quote mode.
// 2. A first pass sums the lengths of the bytes, so the result is allocated once at its
//    exact size.
// 3. A second pass writes it. Blocks of plain printable bytes are tested with SIMD (SWAR
//    where there is none) and bulk-copied; the bytes that need an escape go through the table.

std::string escape(const void* p, size_t size, char quote) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const auto* b = static_cast<const unsigned char *>(p);
	const unsigned char* const e = b + size;
	if (quote == '\1') { quote = '\''; }

	chars::EscapeTable custom;
	const chars::EscapeTable& t = chars::escape_table(quote, custom);  // custom: not reachable under the assert

	const std::size_t n = (quote != '\0' ? 2 : 0) + chars::escape_measure(b, e, t);
	std::string ret;
	ret.resize(n);
	char* d = &ret[0];
	if (quote != '\0') { *d++ = quote; }
	d = chars::escape_emit(d, b, e, t);
	if (quote != '\0') { *d++ = quote; }
	assert(static_cast<std::size_t>(d - &ret[0]) == n);
	return ret;
}
