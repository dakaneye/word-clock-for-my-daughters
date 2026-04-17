// firmware/lib/rtc/include/rtc/advance.h
#pragma once

#include "rtc/date_time.h"

namespace wc::rtc {

// Caller guarantees valid input ranges:
//   year  >= 2023, month ∈ [1, 12], day ∈ [1, 31],
//   hour  ∈ [0, 23], minute ∈ [0, 59], second ∈ [0, 59].
// Wrap math trusts these and does not re-validate — invalid input
// yields unspecified but non-crashing output. The only legitimate
// runtime source is wc::rtc::now(), which produces valid ranges by
// construction via localtime_r.

// Pure. Returns a new DateTime with hour advanced by 1
// (wrapping 23 -> 0). Minute, second, year, month, day UNCHANGED.
// No date carry on hour wrap.
// Consumers: rtc adapter's advance_hour() read-modify-write path.
DateTime advance_hour_fields(DateTime in);

// Pure. Returns a new DateTime with "minute button" semantics:
//
//   raw = in.minute + 5
//   if raw >= 60:
//       raw -= 60
//       hour = (in.hour + 1) % 24     // carry to hour
//   out.minute = (raw / 5) * 5        // floor to 5-min block
//   out.second = 0                    // reset
//   year/month/day preserved          // NO day carry
//
// Matches Mega reference's clockAdvanceMinute5.
DateTime advance_minute_fields(DateTime in);

} // namespace wc::rtc
