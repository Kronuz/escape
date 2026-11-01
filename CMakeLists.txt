cmake_minimum_required(VERSION 3.14)
project(escape LANGUAGES CXX)

# --- Dependency: char-classify ---
# escape.cc includes chars_escape.hh, char-classify's escaping kernel (the length table,
# the measuring pass and the writer shared with repr), which in turn uses chars_simd.hh
# and chars::char_repr. The headers come from char-classify via FetchContent, pinned to a
# commit like the rest of the family.
include(FetchContent)
FetchContent_Declare(
  char_classify
  GIT_REPOSITORY https://github.com/Kronuz/char-classify.git
  GIT_TAG        65c1ff5c4052d9108ea7630bc4327be236ec1bd8
)
FetchContent_MakeAvailable(char_classify)

# --- Library target ---
# escape is not header-only: escape.cc holds the entry point (quote handling and the exact
# sizing) and must be compiled and linked. The header (escape.hh) only declares the function
# and the two convenience wrappers.
add_library(escape escape.cc)
target_include_directories(escape PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# C++20 to stay uniform with the sibling libraries. The standard is PUBLIC so
# consumers compile with it too, matching char-classify (also C++20).
target_compile_features(escape PUBLIC cxx_std_20)

# char-classify is part of the public surface: escape.cc includes "chars_escape.hh", so
# consumers building against escape see that include path too. Link it PUBLIC.
target_link_libraries(escape PUBLIC char_classify::char_classify)

# Alias so consumers can link `escape::escape` whether they add this via
# add_subdirectory or FetchContent.
add_library(escape::escape ALIAS escape)

# Only build the test when this repo is the top-level project, so consumers
# (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(escape_test test/test.cc)
  target_link_libraries(escape_test PRIVATE escape)
  add_test(NAME escape COMMAND escape_test)

  # Differential test against the frozen pre-optimization implementation.
  add_executable(escape_diff_test test/diff_test.cc test/escape_old.cc)
  target_link_libraries(escape_diff_test PRIVATE escape)
  add_test(NAME escape_diff COMMAND escape_diff_test)

  # The same two tests with the portable block backend forced (CHARS_NO_SIMD); the default
  # one is NEON or SSE2, so this is the only run of the 8-byte SWAR path there.
  add_library(escape_swar STATIC escape.cc)
  target_include_directories(escape_swar PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_features(escape_swar PUBLIC cxx_std_20)
  target_link_libraries(escape_swar PUBLIC char_classify::char_classify)
  target_compile_definitions(escape_swar PRIVATE CHARS_NO_SIMD=1)

  add_executable(escape_test_swar test/test.cc)
  target_link_libraries(escape_test_swar PRIVATE escape_swar)
  add_test(NAME escape_swar COMMAND escape_test_swar)

  add_executable(escape_diff_test_swar test/diff_test.cc test/escape_old.cc)
  target_link_libraries(escape_diff_test_swar PRIVATE escape_swar)
  add_test(NAME escape_diff_swar COMMAND escape_diff_test_swar)

  # Benchmark (not run by ctest): old vs new, plus absl::CEscape when abseil is
  # installed. The comparator is optional and never a dependency of the library.
  option(ESCAPE_BUILD_BENCH "Build bench/bench.cc" OFF)
  if(ESCAPE_BUILD_BENCH)
    add_executable(escape_bench bench/bench.cc test/escape_old.cc)
    target_link_libraries(escape_bench PRIVATE escape)
    find_package(absl QUIET)
    if(absl_FOUND)
      target_compile_definitions(escape_bench PRIVATE HAVE_ABSL=1)
      target_link_libraries(escape_bench PRIVATE absl::strings)
    endif()
  endif()
endif()
