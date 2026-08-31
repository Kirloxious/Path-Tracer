#pragma once

#include <format>
#include <iostream>
#include <string_view>

// Minimal single-header logger with ANSI colour output.
// Log::info / Log::warn  →  stdout  (green / yellow)
// Log::error             →  stderr  (red)
// Colour is always emitted; pipe through `strip-ansi` if you need plain text.
namespace Log {

namespace detail {
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view GREEN_BOLD = "\033[1;32m";
constexpr std::string_view YELLOW_BOLD = "\033[1;33m";
constexpr std::string_view RED_BOLD = "\033[1;31m";
} // namespace detail

template<typename... Args> void info(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << detail::GREEN_BOLD << "[INFO] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

template<typename... Args> void warn(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << detail::YELLOW_BOLD << "[WARN] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

template<typename... Args> void error(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << detail::RED_BOLD << "[ERROR] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

} // namespace Log
