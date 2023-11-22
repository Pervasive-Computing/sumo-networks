#pragma once

#include <chrono>

// t is in microseconds
auto humantime(const long t) -> std::string {
    using namespace std::chrono;
    const auto h = duration_cast<hours>(microseconds(t));
    const auto m = duration_cast<minutes>(microseconds(t) - h);
    const auto s = duration_cast<seconds>(microseconds(t) - h - m);
    const auto ms = duration_cast<milliseconds>(microseconds(t) - h - m - s);
    const auto us = duration_cast<microseconds>(microseconds(t) - h - m - s - ms);

    std::string result = "";
    if (h.count() > 0) {
        result += std::to_string(h.count()) + "h ";
    }
    if (m.count() > 0) {
        result += std::to_string(m.count()) + "m ";
    }
    if (s.count() > 0) {
        result += std::to_string(s.count()) + "s ";
    }
    if (ms.count() > 0) {
        result += std::to_string(ms.count()) + "ms ";
    }
    if (us.count() > 0) {
        result += std::to_string(us.count()) + "us ";
    }
    return result;
}
