#pragma once

#include <format>
#include <utility>
#include <version>

#ifdef __cpp_lib_print
#  include <print>   // C++23: std::println (GCC 14+, Clang 18+)
#else
#  include <iostream>
#endif

/// println to stdout; falls back to std::format + cout on pre-C++23 stdlibs.
template <class... Args>
void logln(std::format_string<Args...> fmt, Args&&... args) {
#ifdef __cpp_lib_print
    std::println(fmt, std::forward<Args>(args)...);
#else
    std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
#endif
}
