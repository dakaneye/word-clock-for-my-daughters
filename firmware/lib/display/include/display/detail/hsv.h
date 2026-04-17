#pragma once

#include <cstdint>
#include "display/rgb.h"

namespace wc::display::detail {

// Standard 6-sector HSV→RGB at full saturation (S implicitly = 255).
// hue_deg ∈ [0, 360). v ∈ [0, 255] scales output.
// Integer math with truncating division — at sector midpoints
// (hue = 30°, 90°, 150°, 210°, 270°, 330°) the midpoint channel
// lands at 127 or 128 depending on whether it's the ascending or
// descending component. Tests tolerate ±7 LSB to absorb this and
// future tuning.
inline Rgb hsv_to_rgb_full_sat(uint16_t hue_deg, uint8_t v) {
    const uint8_t sector = static_cast<uint8_t>(hue_deg / 60U);        // 0..5
    const uint16_t f_num = hue_deg - static_cast<uint16_t>(sector) * 60U; // 0..59
    const uint8_t up = static_cast<uint8_t>(
        (static_cast<uint32_t>(v) * f_num) / 60U);
    const uint8_t dn = static_cast<uint8_t>(v - up);
    switch (sector) {
        case 0: return Rgb{v,  up, 0 };  // red → yellow
        case 1: return Rgb{dn, v,  0 };  // yellow → green
        case 2: return Rgb{0,  v,  up};  // green → cyan
        case 3: return Rgb{0,  dn, v };  // cyan → blue
        case 4: return Rgb{up, 0,  v };  // blue → magenta
        case 5: return Rgb{v,  0,  dn};  // magenta → red
        default: return Rgb{0, 0, 0};    // unreachable for hue_deg < 360
    }
}

} // namespace wc::display::detail
