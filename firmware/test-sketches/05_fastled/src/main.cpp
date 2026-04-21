#include <Arduino.h>
#include <FastLED.h>

constexpr int PIN_DATA = 13;
constexpr int NUM_LEDS = 1;     // only light the first LED; the rest of the
                                // strip receives no data and stays dark

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(500);
  FastLED.addLeds<WS2812B, PIN_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(20);    // low brightness for bench work (~8%)
  Serial.println("FastLED cycle: R -> G -> B -> off, 2 Hz");
}

void loop() {
  leds[0] = CRGB::Red;    FastLED.show(); Serial.println("R");   delay(500);
  leds[0] = CRGB::Green;  FastLED.show(); Serial.println("G");   delay(500);
  leds[0] = CRGB::Blue;   FastLED.show(); Serial.println("B");   delay(500);
  leds[0] = CRGB::Black;  FastLED.show(); Serial.println("off"); delay(500);
}
