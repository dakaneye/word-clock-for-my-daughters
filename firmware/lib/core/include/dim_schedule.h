// firmware/lib/core/include/dim_schedule.h
#pragma once

#include <cstdint>

namespace wc {

// Returns brightness multiplier [0.0, 1.0] for the given 24-hour local time.
// Spec: between 19:00 (7 PM) and 08:00 (8 AM) the clock runs at 10%; full
// bright otherwise. Boundaries: 19:00 is dim; 07:59 is dim; 08:00 is full.
//
// `minute` is currently unused — the dim window is hour-granular — but is
// kept in the signature for symmetry with time_to_words() and to allow
// minute-precise boundaries later without a call-site change. Its
// independence within an hour is pinned by test_dim_schedule.
float brightness(uint8_t hour24, uint8_t minute);

} // namespace wc
