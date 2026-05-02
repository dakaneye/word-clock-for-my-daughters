// Diffuser-stack bleed test.
//
// Holds the first WS2812B on the strip at a fixed color + brightness so
// the printed `enclosure/3d/out/light_channel_test_pocket.stl` can be
// taped over the LED and the lateral light spread observed through the
// diffuser stack (opal acrylic ± diffusion film) laid on top.
//
// Default: white at full brightness — the worst case for visible bleed.
// To find the brightness threshold below which bleed disappears, drop
// BRIGHTNESS to 192 / 128 / 64 / 32 and re-flash.

#include <Arduino.h>
#include <FastLED.h>

constexpr int     PIN_DATA      = 13;          // matches 05_fastled wiring
constexpr int     NUM_LEDS      = 35;           // full real-clock LED count;
                                                // covers any sub-strip wired
                                                // on the bench so all physical
                                                // LEDs get an explicit OFF
                                                // frame from this sketch
constexpr int     LIT_INDEX     = 0;            // which LED to keep lit
constexpr uint8_t BRIGHTNESS    = 255;          // 0-255; 255 = max bleed
constexpr CRGB    COLOR         = CRGB::White;  // white = all 3 channels = worst

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(500);
  FastLED.addLeds<WS2812B, PIN_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  // All LEDs in the chain → off, then turn on just one. This overrides any
  // residual state left over from a previous sketch (e.g. 05_fastled's RGB
  // cycle) on LEDs we're not actively addressing.
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[LIT_INDEX] = COLOR;
  FastLED.show();
  Serial.printf("Bleed test: LED %d on (white, brightness %u); other %d LEDs forced off\n",
                LIT_INDEX, BRIGHTNESS, NUM_LEDS - 1);
}

void loop() {
  // Steady on; no animation. Just sit and let the LED glow.
  delay(1000);
}
