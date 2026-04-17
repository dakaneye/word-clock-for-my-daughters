// firmware/lib/display/include/display/palette.h
#pragma once

#include <cstdint>

#include "display/rgb.h"
#include "holidays.h"
#include "word_id.h"

namespace wc::display {

// Per-entry maximum RGB sum, enforced by test_palette_power_budget.
// See the spec §Palette power budget for derivation.
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
