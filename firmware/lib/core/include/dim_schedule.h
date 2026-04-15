// firmware/lib/core/include/dim_schedule.h
#pragma once

#include <cstdint>

namespace wc {

// Returns brightness multiplier [0.0, 1.0] for the given 24-hour local time.
// Spec: between 19:00 (7 PM) and 08:00 (8 AM) the clock runs at 10%; full bright otherwise.
// Boundaries: 19:00 is dim; 07:59 is dim; 08:00 is full.
float brightness(uint8_t hour24, uint8_t minute);

} // namespace wc
