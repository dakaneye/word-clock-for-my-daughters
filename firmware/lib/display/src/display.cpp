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
    // 5 V / 3 A USB-C wall adapter, minus headroom for ESP32 (~300 mA
    // peak with WiFi) + MAX98357A audio amp (~500 mA during playback)
    // + safety margin = 1700 mA cap on the LED strip. FastLED auto-
    // scales brightness proportionally if a frame would exceed this,
    // so worst-case "all 63 LEDs lit at warm white" (~2.58 A nominal)
    // dims to ~66% to fit. Typical time displays (~25-40 LEDs lit)
    // never hit the cap. For 5 V / 2 A adapters, drop this to 1200 mA.
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1700);
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
