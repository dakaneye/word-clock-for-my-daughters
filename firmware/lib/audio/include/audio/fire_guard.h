// firmware/lib/audio/include/audio/fire_guard.h
#pragma once

#include <cstdint>

namespace wc::audio {

struct BirthConfig {
    uint8_t month;    // 1-12
    uint8_t day;      // 1-31
    uint8_t hour;     // 0-23
    uint8_t minute;   // 0-59
};

struct NowFields {
    uint16_t year;    // full year (e.g. 2030)
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23
    uint8_t  minute;  // 0-59
};

// Pure. Returns true iff ALL auto-fire gates pass.
// See audio spec §Birthday auto-fire guards for the gate list.
bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing);

} // namespace wc::audio
