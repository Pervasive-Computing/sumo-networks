#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <type_traits>

#include <fmt/core.h>
#include <fmt/color.h>

#include "ansi-escape-codes.hpp"

[[nodiscard]]
auto pformat(const std::string& s) -> std::string {
    return fmt::format("{}\"{}\"{}", escape_codes::color::fg::green, s, escape_codes::reset);
}

[[nodiscard]]
auto pformat(const char* s) -> std::string {
    return pformat(std::string(s));
}

auto pprint(const std::string& s) -> void {
    fmt::println("{}", pformat(s));
}

auto pprint(const char* s) -> void {
    pprint(std::string(s));
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
[[nodiscard]]
auto pformat(const T f) -> std::string {
    return fmt::format(fg(fmt::color::orange), "{}", f);
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
auto pprint(const T f) -> void {
    fmt::println("{}", pformat(f));
}

template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
[[nodiscard]]
auto pformat(const T i) -> std::string {
    return fmt::format(fg(fmt::color::blue), "{}", i);
}

template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
auto pprint(const T i) -> void {
    fmt::println("{}", pformat(i));
}

[[nodiscard]]
auto pformat(const bool b) -> std::string {
    return fmt::format(b ? fg(fmt::color::green) : fg(fmt::color::red) ,"{}", b);
}

auto pprint(const bool b) -> void {
    fmt::println("{}", pformat(b));
}

[[nodiscard]] 
auto pformat(const std::filesystem::path& p) -> std::string {
    if (! std::filesystem::exists(p)) {
        return fmt::format(fmt::emphasis::bold | fg(fmt::color::red), "{}", p.string());
    }

    if (std::filesystem::is_directory(p)) {
        return fmt::format(fmt::emphasis::bold | fg(fmt::color::blue), "{} {}", p.string(), "");
    }
    else if (std::filesystem::is_regular_file(p)) {
        return fmt::format(fmt::emphasis::bold | fg(fmt::color::green), "{}", p.string());
    }
    else {
        return fmt::format(fmt::emphasis::bold | fg(fmt::color::red), "TODO: not implemented yet for {}\n", p.string());
    }
}

auto pprint(const std::filesystem::path& p) -> void {
    fmt::println("{}", pformat(p));
}


template <typename T>
auto pprint(const std::vector<T>& v, const int indent_by = 4, const int depth = 0) -> void {

    fmt::print("{}{{{}\n", escape_codes::markup::bold, escape_codes::reset);
    for (const auto& e : v) {
        fmt::println("{}{},", std::string((depth + 1) * indent_by, ' '), pformat(e));
    }
    fmt::print("{}}}{}\n", escape_codes::markup::bold, escape_codes::reset);
}

// template <>
// auto pprint<bool>(bool b) -> void {
//     pprint_bool(b);
// }
