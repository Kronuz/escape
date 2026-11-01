// escape benchmark: escape() against the frozen previous implementation
// (test/escape_old.cc) and, when abseil is available, absl::CEscape (whose
// output differs, so it is a speed reference only). Build with
// -DESCAPE_BUILD_BENCH=ON. Reports ns per call, the best of seven samples of at least
// 8 ms; the old and the new implementation are sampled in turn, so a drift of the host
// or of the clock hits both.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "escape.hh"
#ifdef HAVE_ABSL
#include "absl/strings/escaping.h"
#endif

std::string escape_old(const void* p, size_t size, char quote);

// One sample: ns per call over at least 8 ms.
template <typename F>
static double sample_ns(F&& f) {
	using clk = std::chrono::steady_clock;
	long iters = 0;
	auto t0 = clk::now();
	auto t1 = t0;
	do {
		for (int i = 0; i < 16; ++i) { f(); }
		iters += 16;
		t1 = clk::now();
	} while (t1 - t0 < std::chrono::milliseconds(8));
	return std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
}

template <typename F>
static double best_ns(F&& f) {
	double best = 1e18;
	for (int s = 0; s < 7; ++s) { best = std::min(best, sample_ns(f)); }
	return best;
}

// The best of seven samples of each, taken alternately, and of a second set of samples of
// `g`: `aa` is how far apart the two sets of `g` are, in percent (the noise of the row).
template <typename F, typename G>
static void best_pair(F&& f, G&& g, double& best_f, double& best_g, double& aa) {
	best_f = best_g = 1e18;
	double best_g2 = 1e18;
	for (int s = 0; s < 7; ++s) {
		best_f = std::min(best_f, sample_ns(f));
		best_g = std::min(best_g, sample_ns(g));
		best_g2 = std::min(best_g2, sample_ns(g));
	}
	aa = 100.0 * (best_g2 > best_g ? best_g2 - best_g : best_g - best_g2) / best_g;
}

static volatile size_t sink;

static std::string make(size_t n, int dirty_pct) {
	std::mt19937 rng(1);
	std::string s(n, 'a');
	for (auto& c : s) {
		c = (dirty_pct && static_cast<int>(rng() % 100) < dirty_pct) ? static_cast<char>(rng()) : static_cast<char>(0x20 + rng() % 95);
		if (dirty_pct == 100) { c = static_cast<char>(rng() | 0x80); }
	}
	return s;
}

// Printable text where `pct` percent of the bytes are control characters (every one needs an escape).
static std::string make_ctl(size_t n, int pct) {
	std::mt19937 rng(2);
	std::string s(n, 'a');
	for (auto& c : s) { c = (static_cast<int>(rng() % 100) < pct) ? static_cast<char>(rng() % 32) : static_cast<char>(0x20 + rng() % 95); }
	return s;
}

int main() {
	std::printf("%-26s %12s %12s %8s %12s %7s\n", "workload", "old ns", "new ns", "x", "absl ns", "A/A %");
	struct W { const char* name; size_t n; int dirty; int ctl; };
	const W ws[] = {
		{"printable 1 B", 1, 0, 0}, {"printable 8 B", 8, 0, 0},
		{"printable 16 B", 16, 0, 0}, {"printable 256 B", 256, 0, 0}, {"printable 4 KB", 4096, 0, 0},
		{"printable 64 KB", 65536, 0, 0}, {"mixed 10% 256 B", 256, 10, 0}, {"mixed 10% 64 KB", 65536, 10, 0},
		{"ctl 1% 4 KB", 4096, 0, 1}, {"ctl 10% 256 B", 256, 0, 10}, {"ctl 10% 4 KB", 4096, 0, 10},
		{"ctl 10% 64 KB", 65536, 0, 10}, {"ctl 50% 4 KB", 4096, 0, 50},
		{"binary 4 KB", 4096, 100, 0},
	};
	for (const auto& w : ws) {
		std::string s = w.ctl ? make_ctl(w.n, w.ctl) : make(w.n, w.dirty);
		double o = 0, n = 0, aa = 0;
		best_pair([&] { sink = escape_old(s.data(), s.size(), '\'').size(); },
			[&] { sink = escape(s.data(), s.size(), '\'').size(); }, o, n, aa);
		double a = 0;
#ifdef HAVE_ABSL
		a = best_ns([&] { sink = absl::CEscape(s).size(); });
#endif
		std::printf("%-26s %12.1f %12.1f %8.2f %12.1f %7.1f\n", w.name, o, n, o / n, a, aa);
	}

	// Sizes either side of the 8-byte step and the 16-byte block, where the writer changes path, the
	// other quote modes, and bytes that all need an escape.
	struct Edge { std::string name; std::string s; char quote; };
	std::vector<Edge> edges;
	for (size_t n : {2, 4, 7, 15}) {
		std::string s(n, 'a');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>('a' + i % 26); }
		edges.push_back({"printable " + std::to_string(n) + " B", s, '\''});
	}
	edges.push_back({"printable 1 B, no quote", "a", '\0'});
	edges.push_back({"printable 4 B, quote \"", "abcd", '"'});
	edges.push_back({"printable 3 B, quote \\1", "abc", '\1'});
	for (size_t n : {2, 4, 16, 4096}) {
		std::string s(n, '\0');
		for (size_t i = 0; i < n; ++i) { s[i] = static_cast<char>(i * 37); }
		edges.push_back({"binary " + std::to_string(n) + " B, quote \"", s, '"'});
	}
	for (const auto& e : edges) {
		double o = 0, n = 0, aa = 0;
		best_pair([&] { sink = escape_old(e.s.data(), e.s.size(), e.quote).size(); },
			[&] { sink = escape(e.s.data(), e.s.size(), e.quote).size(); }, o, n, aa);
		std::printf("%-26s %12.1f %12.1f %8.2f %12.1f %7.1f\n", e.name.c_str(), o, n, o / n, 0.0, aa);
	}
	return 0;
}
