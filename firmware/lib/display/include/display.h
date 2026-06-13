// firmware/lib/display/include/display.h
//
// ESP32-only public API. Depends on FastLED + Arduino.h. Include
// this ONLY from translation units that compile under the Arduino
// toolchain (the emory / nora PlatformIO envs). Native tests
// include the pure-logic headers under display/ instead — never
// this header.
#pragma once

#include <Arduino.h>
#include "display/rgb.h"

namespace wc::display {

// Initialize FastLED (addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, LED_COUNT))
// and set the 1700 mA runtime power ceiling (sized for a 3 A USB-C
// supply). Idempotent.
void begin();

// Push a rendered Frame to the LED strip (copies the Frame into the
// internal FastLED CRGB buffer and calls FastLED.show()). The
// adapter owns no color policy — it pushes exactly what render()
// returned.
void show(const Frame& frame);

} // namespace wc::display
