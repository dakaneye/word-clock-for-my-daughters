// firmware/lib/ntp/src/ntp.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag NTPClient/WiFi into the native build.
// Same pattern as the wifi_provision/buttons/display/rtc adapters.
#ifdef ARDUINO

#include "ntp.h"

namespace wc::ntp {

// Real implementation lands in Task 6.
void begin() {}
void loop()  {}

} // namespace wc::ntp

#endif // ARDUINO
