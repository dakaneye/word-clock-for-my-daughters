// firmware/lib/core/src/dim_schedule.cpp
#include "dim_schedule.h"

namespace wc {

float brightness(uint8_t hour24, uint8_t /*minute*/) {
    // Dim window: [19:00, 08:00). hour >= 19 OR hour < 8.
    if (hour24 >= 19 || hour24 < 8) return 0.1f;
    return 1.0f;
}

} // namespace wc
