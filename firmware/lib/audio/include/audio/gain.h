// firmware/lib/audio/include/audio/gain.h
//
// Pure-logic Q8 PCM gain scalar. No Arduino, no I/O, no platform
// dependencies — compiled into both the ESP32 adapter (applied in
// the pump loop before i2s_write) and the native test binary.
#pragma once

#include <cstdint>
#include <cstddef>

// Default software gain (Q8). Unity = 256. Per-kid configs may
// override via -D or #define BEFORE including this header. Kept
// here so the module compiles cleanly even if a config header
// hasn't been updated yet.
#ifndef CLOCK_AUDIO_GAIN_Q8
#define CLOCK_AUDIO_GAIN_Q8 256
#endif

static_assert(CLOCK_AUDIO_GAIN_Q8 >= 0 && CLOCK_AUDIO_GAIN_Q8 <= 256,
              "CLOCK_AUDIO_GAIN_Q8 must be in [0, 256]; values >256 would clip at INT16 peak");

namespace wc::audio {

// Pure. In-place Q8 scale of signed 16-bit PCM.
// Contract: gain_q8 ∈ [0, 256]. gain_q8 == 256 is unity (no-op shortcut).
void apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8);

} // namespace wc::audio
