// firmware/lib/ntp/src/ntp.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag NTPClient/WiFi into the native build.
// Same pattern as the wifi_provision/buttons/display/rtc adapters.
#ifdef ARDUINO

// Compile-time guard: this module targets the Arduino-ESP32 framework
// specifically (uses <WiFi.h>, <NTPClient.h>). If a future toolchain
// change defines ARDUINO but on a different arch, fail loud instead
// of silently compiling to nothing useful.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "ntp adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include "ntp.h"

namespace wc::ntp {

// Real implementation lands in Task 6.
void begin() {}
void loop()  {}

} // namespace wc::ntp

#endif // ARDUINO
