#pragma once

#include <cstdint>

#include "display/rgb.h"
#include "word_id.h"

namespace wc::display {

// Full rainbow cycle period.
inline constexpr uint32_t RAINBOW_PERIOD_MS = 60'000;

// Rainbow color for a decor word at monotonic time now_ms.
// Valid only for w ∈ {HAPPY, BIRTH, DAY, NAME}; caller guarantees.
// Returns full-saturation, full-value RGB (dim multiplier is the
// caller's responsibility — see renderer::render).
// Period: RAINBOW_PERIOD_MS per full 360° cycle.
// Phase offsets: HAPPY=0°, BIRTH=90°, DAY=180°, NAME=270°.
// Implementation MUST use
// (now_ms % RAINBOW_PERIOD_MS) * 360 / RAINBOW_PERIOD_MS to keep the
// intermediate product from overflowing uint32_t every ≈3.3 h.
Rgb rainbow(WordId w, uint32_t now_ms);

} // namespace wc::display
