// firmware/lib/audio/src/fire_guard.cpp
#include "audio/fire_guard.h"

namespace wc::audio {

bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing) {
    if (!time_known)               return false;
    if (already_playing)           return false;
    if (now.year  == last_fired_year) return false;
    if (now.month != birth.month)  return false;
    if (now.day   != birth.day)    return false;
    if (now.hour  != birth.hour)   return false;
    if (now.minute != birth.minute) return false;
    return true;
}

} // namespace wc::audio
