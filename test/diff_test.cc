// Differential test: escape() must match the frozen pre-optimization
// implementation (escape_old, test/escape_old.cc) byte for byte on random and
// edge inputs, in every quote mode.
#include <cstdio>
#include <random>
#include <string>

#include "escape.hh"

std::string escape_old(const void* p, size_t size, char quote);

static int failures = 0;

static void check(const std::string& in, char quote) {
	std::string a = escape(in.data(), in.size(), quote);
	std::string b = escape_old(in.data(), in.size(), quote);
	if (a != b && ++failures < 10) {
		std::fprintf(stderr, "MISMATCH size=%zu quote=%d\n  new=%s\n  old=%s\n", in.size(), quote, a.c_str(), b.c_str());
	}
}

int main() {
	std::mt19937_64 rng(777);
	const char quotes[] = {'\0', '\1', '\'', '"'};
	for (int iter = 0; iter < 20000; ++iter) {
		size_t n = rng() % 200;
		if (iter % 50 == 0) { n = rng() % 5000; }
		int mode = iter % 4;
		std::string s(n, '\0');
		for (auto& c : s) {
			unsigned r = static_cast<unsigned>(rng());
			switch (mode) {
				case 0: c = static_cast<char>(0x20 + r % 95); break;
				case 1: c = (r % 10 == 0) ? static_cast<char>(r >> 8) : static_cast<char>(0x20 + (r >> 8) % 95); break;
				case 2: c = static_cast<char>(r >> 8); break;
				default: {
					const char sp[] = {'\n', '\r', '\t', '\\', '\'', '"', 'a', 0x7f, 0x1f, 0x20, 0x7e, '\0', char(0x80), char(0xff)};
					c = sp[(r >> 8) % sizeof(sp)];
				}
			}
		}
		for (char q : quotes) { check(s, q); }
	}
	// Every byte value at every position of 1..17 byte windows (covers the 8-byte block edges).
	for (int c = 0; c < 256; ++c) {
		for (size_t n = 1; n <= 17; ++n) {
			for (size_t pos = 0; pos < n; ++pos) {
				std::string s(n, 'k');
				s[pos] = static_cast<char>(c);
				for (char q : quotes) { check(s, q); }
			}
		}
	}
	check("", '\'');
	check("", '\0');
	if (failures != 0) {
		std::fprintf(stderr, "%d mismatches\n", failures);
		return 1;
	}
	std::printf("escape differential OK\n");
	return 0;
}
