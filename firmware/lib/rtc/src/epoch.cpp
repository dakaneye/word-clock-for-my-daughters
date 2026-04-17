// firmware/lib/rtc/src/epoch.cpp
//
// Pure logic — no Arduino, no hardware, safe to compile in native env.
// Howard Hinnant's days_from_civil:
//   http://howardhinnant.github.io/date_algorithms.html#days_from_civil
// Returns number of days since the civil epoch 1970-01-01 for a given
// (year, month, day) triple. Tested in test_rtc_epoch against the
// host's timegm() at 10 spread-out dates to catch any transcription
// bug.
#include "rtc/epoch.h"

namespace wc::rtc {

namespace {
int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
    y -= m <= 2;
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}
}  // namespace

uint32_t utc_epoch_from_fields(DateTime utc) {
    int32_t days = days_from_civil(
        static_cast<int32_t>(utc.year),
        static_cast<uint32_t>(utc.month),
        static_cast<uint32_t>(utc.day));
    return static_cast<uint32_t>(days) * 86400u
         + static_cast<uint32_t>(utc.hour)   * 3600u
         + static_cast<uint32_t>(utc.minute) * 60u
         + static_cast<uint32_t>(utc.second);
}

} // namespace wc::rtc
