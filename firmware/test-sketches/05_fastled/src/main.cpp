#include <Arduino.h>
#include <FastLED.h>

// 63-LED bench test for the production board (D1-D64, D7 unused = 63 in chain).
// Three phases, looping:
//   1. D1 alone in RED  — GRB color-order check (display_checks.md §3);
//                         green here means the color order is wrong.
//   2. Chain walk       — one dim-white LED at a time, D1..D63; a gap or
//                         stuck spot pinpoints a bad joint/LED by index.
//   3. All-on dim red   — whole-chain data integrity at low current.
// Power-capped for laptop-USB bench work; lift the cap only on the 3 A supply.

constexpr int PIN_DATA = 13;
constexpr int NUM_LEDS = 63;

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(500);
  FastLED.addLeds<WS2812B, PIN_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 600);
  Serial.println("63-LED bench test: D1-red -> chain walk -> all-on dim red");
}

void loop() {
  FastLED.clear();
  leds[0] = CRGB::Red;
  FastLED.show();
  Serial.println("phase 1: D1 only — must glow RED (green = color-order bug)");
  delay(8000);

  Serial.println("phase 2: chain walk D1..D63, one dim-white LED at a time");
  for (int i = 0; i < NUM_LEDS; i++) {
    FastLED.clear();
    leds[i] = CRGB(64, 64, 64);
    FastLED.show();
    delay(150);
  }

  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  Serial.println("phase 3: all 63 on (dim red)");
  delay(8000);
}
