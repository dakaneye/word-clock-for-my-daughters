// firmware/lib/audio/src/audio.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag SD/driver-i2s into the native build.
// Same pattern as the wifi_provision / buttons / display / rtc / ntp
// adapters.
#ifdef ARDUINO

// Compile-time guard: this module targets the Arduino-ESP32 framework
// specifically (uses <driver/i2s.h>, <SD.h>). If a future toolchain
// change defines ARDUINO but on a different arch, fail loud instead
// of silently compiling to nothing useful.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "audio adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include "audio.h"

namespace wc::audio {

// Real implementation lands in Task 5.
void begin(const BirthConfig& /*birth*/) {}
void loop()                             {}
void play_lullaby()                     {}
void stop()                             {}
bool is_playing()                       { return false; }

} // namespace wc::audio

#endif // ARDUINO
