// firmware/lib/core/include/birthday.h
#pragma once

#include <cstdint>

namespace wc {

struct BirthdayConfig {
    uint8_t month;   // 1-12
    uint8_t day;     // 1-31
    uint8_t hour;    // 0-23 (birth minute trigger hour)
    uint8_t minute;  // 0-59
};

struct BirthdayState {
    bool is_birthday;      // today is the kid's birthday (month+day match)
    bool is_birth_minute;  // ALSO currently at the exact birth hour:minute
};

BirthdayState birthday_state(uint8_t now_month, uint8_t now_day,
                              uint8_t now_hour, uint8_t now_minute,
                              const BirthdayConfig& cfg);

} // namespace wc
