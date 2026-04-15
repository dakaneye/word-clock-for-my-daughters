// firmware/lib/core/include/holidays.h
#pragma once

#include <cstdint>

namespace wc {

enum class Palette : uint8_t {
    WARM_WHITE,
    MLK_PURPLE,
    VALENTINES,
    WOMEN_PURPLE,
    EARTH_DAY,
    EASTER_PASTEL,
    JUNETEENTH,
    INDIGENOUS,
    HALLOWEEN,
    CHRISTMAS,
};

// Returns the palette for a given date. Defaults to WARM_WHITE.
Palette palette_for_date(uint16_t year, uint8_t month, uint8_t day);

} // namespace wc
