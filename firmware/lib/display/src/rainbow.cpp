#include "display/rainbow.h"

#include "display/detail/hsv.h"

namespace wc::display {

namespace {

uint16_t phase_offset_deg(WordId w) {
    switch (w) {
        case WordId::HAPPY: return 0;
        case WordId::BIRTH: return 90;
        case WordId::DAY:   return 180;
        case WordId::NAME:  return 270;
        default:            return 0;  // caller contract prohibits
    }
}

} // namespace

Rgb rainbow(WordId w, uint32_t now_ms) {
    // Mod FIRST to keep the multiply under uint32_t overflow
    // (60'000 × 360 = 21'600'000 ≪ UINT32_MAX). Without the mod,
    // now_ms × 360 overflows every UINT32_MAX/360 ≈ 3.3 h.
    const uint32_t phase_frac =
        (now_ms % RAINBOW_PERIOD_MS) * 360U / RAINBOW_PERIOD_MS;
    const uint16_t hue = static_cast<uint16_t>(
        (phase_frac + phase_offset_deg(w)) % 360U);
    return detail::hsv_to_rgb_full_sat(hue, 255);
}

} // namespace wc::display
