// firmware/lib/display/include/display/renderer.h
#pragma once

#include <cstdint>

#include "birthday.h"
#include "display/rgb.h"

namespace wc::display {

struct RenderInput {
    // Date + wall-clock time for palette, birthday, dim window, and
    // time_to_words. Caller guarantees ranges:
    //   month  ∈ [1, 12]
    //   day    ∈ [1, 31]
    //   hour   ∈ [0, 23]
    //   minute ∈ [0, 59]
    //   year   ≥ 2023 (earliest plausible wall-clock for either
    //                  daughter's lifetime; pre-2023 renders
    //                  cleanly but holiday behavior is unspecified).
    // The renderer does NOT re-validate; bad inputs trigger
    // undefined visual output but never crash. Bounds-checking is
    // the `rtc` module's responsibility.
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;

    // Monotonic millis for rainbow phase. Wraps at ~49 days; rainbow
    // math handles the wrap correctly.
    uint32_t now_ms;

    // From wifi_provision::seconds_since_last_sync(). UINT32_MAX
    // means "never synced this session"; treated as stale (>24 h).
    uint32_t seconds_since_sync;

    // Per-kid birthday, sourced from config_{emory,nora}.h in
    // main.cpp before calling render().
    BirthdayConfig birthday;
};

// Stale-sync threshold. Above this value, amber replaces warm white
// on lit time words (holidays and birthday decor still win).
inline constexpr uint32_t STALE_SYNC_THRESHOLD_S = 86'400;  // 24 h

// Pure function. Given the inputs, returns the final per-LED Frame
// ready to push to the strip (dim multiplier already applied).
Frame render(const RenderInput& in);

} // namespace wc::display
