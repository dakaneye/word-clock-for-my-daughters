// firmware/lib/display/include/display/palette.h
#pragma once

#include <cstdint>

#include "display/rgb.h"
#include "holidays.h"
#include "word_id.h"

namespace wc::display {

// Per-word maximum RGB channel sum, enforced by test_palette_power_budget.
// This is a per-color brightness/saturation sanity bound, NOT a total-strip
// power guarantee: total LED current is enforced at runtime by
// FastLED.setMaxPowerInVoltsAndMilliamps(5, 1700) in display.cpp (sized for
// the 3 A USB-C supply across all 63 LEDs). The original "derived for 35
// LEDs" power rationale no longer applies with that runtime cap in place;
// 700 is retained as the per-word ceiling.
inline constexpr uint16_t PALETTE_MAX_RGB_SUM = 700;

// Returns the RGB color a given word should be under a given palette.
// Implementation MUST switch(p) over named Palette enumerators (not
// array-indexed lookup) so a future Palette enum addition triggers
// -Wswitch-enum at compile time. Within a palette: word receives
// palette_colors[static_cast<uint8_t>(w) % size()].
Rgb color_for(Palette p, WordId w);

// Convenience constants.
Rgb warm_white();   // {255, 170, 100}
Rgb amber();        // {255, 120, 30}

} // namespace wc::display
