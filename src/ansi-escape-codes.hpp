#pragma once

namespace escape_codes {
constexpr const char *reset = "\033[0m";
namespace color::fg {
constexpr const char *red = "\033[31m";
constexpr const char *green = "\033[32m";
constexpr const char *yellow = "\033[33m";
constexpr const char *blue = "\033[34m";
constexpr const char *magenta = "\033[35m";
constexpr const char *cyan = "\033[36m";
} // namespace color::fg

namespace color::bg {
constexpr const char *red = "\033[41m";
constexpr const char *green = "\033[42m";
constexpr const char *yellow = "\033[43m";
constexpr const char *blue = "\033[44m";
constexpr const char *magenta = "\033[45m";
constexpr const char *cyan = "\033[46m";
} // namespace color::bg

namespace markup {
constexpr const char *bold = "\033[1m";
constexpr const char *underline = "\033[4m";
constexpr const char *italic = "\033[3m";
constexpr const char *strike = "\033[9m";
constexpr const char *blink = "\033[5m";
constexpr const char *reverse = "\033[7m";
constexpr const char *hidden = "\033[8m";
} // namespace markup

} // namespace escape_codes
