// firmware/lib/audio/src/gain.cpp
#include "audio/gain.h"

namespace wc::audio {

void apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8) {
    if (gain_q8 == 256) return;  // unity shortcut
    for (size_t i = 0; i < n; ++i) {
        samples[i] = static_cast<int16_t>(
            (static_cast<int32_t>(samples[i]) * gain_q8) >> 8
        );
    }
}

} // namespace wc::audio
