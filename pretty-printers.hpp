#pragma once

#include <string>
#include <fmt/core.h>

#include "ansi-escape-codes.hpp"

auto pprint_string(const std::string& s) -> void {
    fmt::print("{}\"{}\"{}\n", escape_codes::color::fg::green, s, escape_codes::reset);
}

auto pprint_bool(bool b) -> void {
    fmt::print("{}{}{}\n", b ? escape_codes::color::fg::green : escape_codes::color::fg::red, b ? "true" : "false", escape_codes::reset);
}


template <typename T>
auto pprint(T&& t) -> void {}

template <>
auto pprint<const std::string&>(const std::string& s) -> void {
    pprint_string(s);
}

// template <>
// auto pprint<bool>(bool b) -> void {
//     pprint_bool(b);
// }
