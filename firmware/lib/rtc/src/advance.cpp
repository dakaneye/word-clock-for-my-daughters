// firmware/lib/rtc/src/advance.cpp
//
// Pure logic — no Arduino, no hardware. Mirrors the Mega reference's
// clockAdvanceHour / clockAdvanceMinute5 wrap semantics:
//   hour   — (h+1) % 24, no date carry
//   minute — add 5, carry to hour at 60, no date carry, second -> 0,
//            then floor to 5-min block
// See rtc/advance.h for the caller-validity contract.
#include "rtc/advance.h"

namespace wc::rtc {

DateTime advance_hour_fields(DateTime in) {
    in.hour = static_cast<uint8_t>((in.hour + 1) % 24);
    return in;
}

DateTime advance_minute_fields(DateTime in) {
    uint8_t raw = static_cast<uint8_t>(in.minute + 5);
    if (raw >= 60) {
        raw = static_cast<uint8_t>(raw - 60);
        in.hour = static_cast<uint8_t>((in.hour + 1) % 24);
    }
    in.minute = static_cast<uint8_t>((raw / 5) * 5);
    in.second = 0;
    return in;
}

} // namespace wc::rtc
