// firmware/lib/ntp/include/ntp/schedule.h
#pragma once

#include <cstdint>

namespace wc::ntp {

// Pure. Capped exponential backoff for the Nth consecutive failure.
// Caller is expected to pass N >= 1 (the adapter only calls this
// after consecutive_failures += 1).
//
//   N=1 -> 30'000  ms (30 s)
//   N=2 -> 60'000  ms (1 m)
//   N=3 -> 120'000 ms (2 m)
//   N=4 -> 240'000 ms (4 m)
//   N=5 -> 480'000 ms (8 m)
//   N>=6 -> 1'800'000 ms (30 m)  // cap
uint32_t next_backoff_ms(uint32_t consecutive_failures);

// Pure. Returns the millis() deadline at which the next sync should
// fire after a successful sync. = success_ms + 24h +
// uniform(-30m, +30m), where the offset is jitter_sample mod
// 3'600'000 - 1'800'000.
uint64_t next_deadline_after_success(uint64_t success_ms,
                                     uint32_t jitter_sample);

} // namespace wc::ntp
