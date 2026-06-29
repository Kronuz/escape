cmake_minimum_required(VERSION 3.14)
project(escape LANGUAGES CXX)

# --- Dependency: char-classify ---
# escape leans on chars::char_repr to render the two-char lowercase hex of a
# byte, so it needs the char-classify header on its include path. Pull it in via
# FetchContent and track its tip with GIT_TAG main, like the rest of the family.
include(FetchContent)
FetchContent_Declare(
  char_classify
  GIT_REPOSITORY https://github.com/Kronuz/char-classify.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(char_classify)

# --- Library target ---
# escape is not header-only: the escaping loop lives in escape.cc and must be
# compiled and linked. The header (escape.hh) only declares the function and the
# two convenience wrappers.
add_library(escape escape.cc)
target_include_directories(escape PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# C++20 to stay uniform with the sibling libraries. The standard is PUBLIC so
# consumers compile with it too, matching char-classify (also C++20).
target_compile_features(escape PUBLIC cxx_std_20)

# char-classify is part of the public surface: escape.cc includes "chars.hh", so
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
endif()
