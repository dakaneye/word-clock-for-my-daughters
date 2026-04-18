// firmware/lib/ntp/src/schedule.cpp
#include "ntp/schedule.h"

namespace wc::ntp {

uint32_t next_backoff_ms(uint32_t consecutive_failures) {
    // Lookup table avoids the shift-by-(-1) UB hazard if a caller
    // ever passes 0 in violation of the contract. Index = N - 1.
    static constexpr uint32_t CURVE[] = {
        30'000u,    // N=1: 30 s
        60'000u,    // N=2: 1 m
        120'000u,   // N=3: 2 m
        240'000u,   // N=4: 4 m
        480'000u,   // N=5: 8 m
        1'800'000u, // N>=6: cap at 30 m
    };
    constexpr uint32_t MAX_IDX = (sizeof(CURVE) / sizeof(CURVE[0])) - 1;

    // Clamp underflow defensively: N=0 is a contract violation, but
    // returning the floor (30 s) is safer than UB.
    uint32_t idx = (consecutive_failures == 0) ? 0u
                                                : (consecutive_failures - 1u);
    if (idx > MAX_IDX) idx = MAX_IDX;
    return CURVE[idx];
}

uint64_t next_deadline_after_success(uint64_t success_ms,
                                     uint32_t jitter_sample) {
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint32_t JITTER_SPAN_MS = 3'600'000u;  // 1h total span
    constexpr uint32_t JITTER_HALF_MS = 1'800'000u;  // ±30 min
    uint32_t jitter = jitter_sample % JITTER_SPAN_MS;
    return success_ms + TWENTY_FOUR_HOURS_MS + jitter - JITTER_HALF_MS;
}

} // namespace wc::ntp
