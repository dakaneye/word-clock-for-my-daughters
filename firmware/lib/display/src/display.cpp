// firmware/lib/display/src/display.cpp
//
// Guarded with #ifdef ARDUINO — PlatformIO LDF's deep+ mode would
// otherwise compile this TU in the native test build where
// FastLED.h doesn't exist. Same pattern as
// firmware/lib/wifi_provision/src/{nvs_store,dns_wrapper,web_server,
// wifi_provision}.cpp and firmware/lib/buttons/src/buttons.cpp.
#ifdef ARDUINO

#include <Arduino.h>
#include <FastLED.h>

#include "display.h"
#include "display/rgb.h"
#include "pinmap.h"       // PIN_LED_DATA

namespace wc::display {

namespace {

CRGB leds[LED_COUNT];
bool started = false;

} // namespace

void begin() {
    if (started) return;
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, LED_COUNT);
    // Runtime layer-2 defense on the palette power budget. If a
    // palette tune ever emits a frame that would pull more than
    // 1.8 A on the strip, FastLED scales brightness down
    // automatically — belt-and-suspenders for the build-time
    // test_palette_power_budget invariant.
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1800);
    FastLED.clear();
    FastLED.show();
    started = true;
}

void show(const Frame& frame) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        leds[i] = CRGB(frame[i].r, frame[i].g, frame[i].b);
    }
    FastLED.show();
}

} // namespace wc::display

#endif  // ARDUINO
