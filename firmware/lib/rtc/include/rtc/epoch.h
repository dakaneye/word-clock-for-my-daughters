// firmware/lib/rtc/include/rtc/epoch.h
#pragma once

#include <cstdint>
#include "rtc/date_time.h"

namespace wc::rtc {

// Caller guarantees valid input ranges:
//   year  >= 1970, month ∈ [1, 12], day ∈ [1, 31],
//   hour  ∈ [0, 23], minute ∈ [0, 59], second ∈ [0, 59].
// Input fields MUST represent a UTC date-time; no TZ conversion
// happens here.
//
// Pure. Returns seconds since 1970-01-01 00:00:00 UTC, computed via
// Howard Hinnant's days_from_civil algorithm — no libc dependency,
// no TZ state access, safe to call from any task.
//
// Implementation note: the intermediate day-count fits in int32_t
// across the daughters' 2026-2106 lifespan; the output is uint32_t
// and overflows in 2106 (same ceiling as RTClib's DateTime(uint32_t)
// constructor, so widening here would not help the chip).
uint32_t utc_epoch_from_fields(DateTime utc);

} // namespace wc::rtc
