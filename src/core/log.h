#pragma once

#include <cstdio>
#include <format>
#include <print>
#include <string_view>

/// Colour escapes are always emitted, even when not writing to a terminal.
namespace Log {

namespace detail {
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view GREEN_BOLD = "\033[1;32m";
constexpr std::string_view YELLOW_BOLD = "\033[1;33m";
constexpr std::string_view RED_BOLD = "\033[1;31m";
} // namespace detail

template<typename... Args> void info(std::format_string<Args...> fmt, Args&&... args) {
    std::println(stdout, "{}[INFO] {}{}", detail::GREEN_BOLD, detail::RESET, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args> void warn(std::format_string<Args...> fmt, Args&&... args) {
    std::println(stdout, "{}[WARN] {}{}", detail::YELLOW_BOLD, detail::RESET, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args> void error(std::format_string<Args...> fmt, Args&&... args) {
    std::println(stderr, "{}[ERROR] {}{}", detail::RED_BOLD, detail::RESET, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace Log
