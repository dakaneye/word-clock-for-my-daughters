// firmware/lib/rtc/include/rtc/date_time.h
#pragma once

#include <cstdint>

namespace wc::rtc {

// Local wall-clock fields (output of now(), input to advance wrap
// math). UTC is an internal detail of the adapter and never escapes
// the rtc module's public API except via the uint32_t epoch argument
// to set_from_epoch().
struct DateTime {
    uint16_t year;    // full year, e.g. 2030. No 2-digit encoding.
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23
    uint8_t  minute;  // 0-59
    uint8_t  second;  // 0-59
};

constexpr bool operator==(const DateTime& a, const DateTime& b) {
    return a.year == b.year && a.month == b.month && a.day == b.day
        && a.hour == b.hour && a.minute == b.minute
        && a.second == b.second;
}

constexpr bool operator!=(const DateTime& a, const DateTime& b) {
    return !(a == b);
}

} // namespace wc::rtc
