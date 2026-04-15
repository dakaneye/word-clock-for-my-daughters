// firmware/lib/core/src/birthday.cpp
#include "birthday.h"

namespace wc {

BirthdayState birthday_state(uint8_t now_month, uint8_t now_day,
                              uint8_t now_hour, uint8_t now_minute,
                              const BirthdayConfig& cfg) {
    BirthdayState s{};
    s.is_birthday = (now_month == cfg.month) && (now_day == cfg.day);
    s.is_birth_minute = s.is_birthday
                     && (now_hour == cfg.hour)
                     && (now_minute == cfg.minute);
    return s;
}

} // namespace wc
