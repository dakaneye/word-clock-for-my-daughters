#include <Arduino.h>
#include <FastLED.h>

constexpr int PIN_DATA = 13;
constexpr int NUM_LEDS = 35;

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(500);
  FastLED.addLeds<WS2812B, PIN_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  Serial.println("Chain walker: lighting one LED at a time, idx 0..34");
}

void loop() {
  // All LEDs on, low brightness, dim red.
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  Serial.println("all on (red)");
  delay(2000);
}
