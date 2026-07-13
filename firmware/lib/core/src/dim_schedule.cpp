// firmware/lib/core/src/dim_schedule.cpp
#include "dim_schedule.h"

namespace wc {

float brightness(uint8_t hour24, uint8_t /*minute*/) {
    // Dim window: [19:00, 08:00). hour >= 19 OR hour < 8.
    // 0.25 chosen on the assembled clock (2026-07-12): the original 0.10
    // was hard to read in a lit evening room through the face + film.
    if (hour24 >= 19 || hour24 < 8) return 0.25f;
    return 1.0f;
}

} // namespace wc
