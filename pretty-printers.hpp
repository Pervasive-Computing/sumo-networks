#pragma once

#include <string>
#include <vector>
#include <fmt/core.h>

#include "ansi-escape-codes.hpp"

auto pprint_string(const std::string& s) -> void {
    fmt::print("{}\"{}\"{}\n", escape_codes::color::fg::green, s, escape_codes::reset);
}

auto pprint_bool(bool b) -> void {
    fmt::print("{}{}{}\n", b ? escape_codes::color::fg::green : escape_codes::color::fg::red, b ? "true" : "false", escape_codes::reset);
}

auto pformat_string(const std::string& s) -> std::string {
    return fmt::format("{}\"{}\"{}", escape_codes::color::fg::green, s, escape_codes::reset);
}


auto pformat(const std::string& s) -> std::string {
    return pformat_string(s);
}


// template <typename T>
// auto pprint(T&& t) -> void {}

// template <>
auto pprint(const std::string& s) -> void {
    pprint_string(s);
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
