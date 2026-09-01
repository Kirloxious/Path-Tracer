#pragma once

/**
 * @file log.h
 * @brief Minimal single-header logger with ANSI colour output.
 */

#include <format>
#include <iostream>
#include <string_view>

/**
 * @brief Formatted, colourised logging on top of std::format.
 *
 * `info` / `warn` go to stdout (green / yellow); `error` goes to stderr (red). Colour escapes
 * are always emitted — pipe through `strip-ansi` if you need plain text. Every function takes
 * a compile-time-checked `std::format_string`, so a mismatched placeholder is a build error
 * rather than a runtime surprise.
 */
namespace Log {

namespace detail {
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view GREEN_BOLD = "\033[1;32m";
constexpr std::string_view YELLOW_BOLD = "\033[1;33m";
constexpr std::string_view RED_BOLD = "\033[1;31m";
} // namespace detail

/**
 * @brief Writes a green `[INFO]` line to stdout.
 * @tparam Args Deduced argument types for @p fmt.
 * @param fmt   Compile-time-checked std::format string.
 * @param args  Values substituted into @p fmt.
 */
template<typename... Args> void info(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << detail::GREEN_BOLD << "[INFO] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

/**
 * @brief Writes a yellow `[WARN]` line to stdout.
 * @tparam Args Deduced argument types for @p fmt.
 * @param fmt   Compile-time-checked std::format string.
 * @param args  Values substituted into @p fmt.
 */
template<typename... Args> void warn(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << detail::YELLOW_BOLD << "[WARN] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

/**
 * @brief Writes a red `[ERROR]` line to stderr.
 * @tparam Args Deduced argument types for @p fmt.
 * @param fmt   Compile-time-checked std::format string.
 * @param args  Values substituted into @p fmt.
 */
template<typename... Args> void error(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << detail::RED_BOLD << "[ERROR] " << detail::RESET << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

} // namespace Log
