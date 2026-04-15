# FastLED on ESP32 — Peripheral Choice and WiFi Coexistence

## Why this matters

Our clock runs WiFi (for NTP time sync) AND drives 25 WS2812B LEDs. WS2812B is extremely timing-sensitive — each bit is ~1.25µs. If the ESP32's CPU is distracted by WiFi interrupts during a LED refresh, the LEDs can show wrong colors or flicker. This doc confirms FastLED's approach handles this.

## Verified facts

### Which peripheral FastLED uses

FastLED on ESP32 classic supports **three** backends:

1. **RMT (Remote Control) peripheral** — primary, recommended. Offloads WS2812B timing to dedicated hardware so the CPU is free to run WiFi.
2. **I²S peripheral** — for parallel WS2812B output (up to 24 strips). Overkill for our single-strip 25-LED clock.
3. **SPI** — alternative; fast and stable.

### FastLED includes two RMT driver implementations

- **RMT4** — FastLED's custom optimized driver. From the FastLED README: **"RMT4 significantly outperforms Espressif's generic RMT5 wrapper"** in interrupt overhead and timing precision. Has a **"dual-buffer design [that] prevents flickering under Wi-Fi load."**
- **RMT5** — Espressif's IDF wrapper. Newer but less optimized for LED timing.

**For our project: FastLED's RMT4 is the right choice.** It's the default in recent FastLED versions. No firmware configuration beyond `#include <FastLED.h>` and standard `FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)`.

### WiFi coexistence

Explicitly addressed in the FastLED documentation: **"RMT handles interrupts better"** when optimizing for WiFi performance. The dual-buffer design means one buffer is being transmitted while the next is being filled — WiFi interrupts can preempt the CPU without corrupting in-flight LED data.

**Conclusion:** FastLED + WiFi on ESP32 classic is a solved problem, as long as we use RMT (default).

### GPIO pin requirements

No hard restriction on which GPIO drives the LED data line — RMT can route to any GPIO. Our choice of GPIO 13 is fine.

(On newer ESP32 variants like ESP32-P4, there's a strict GPIO39-54 requirement for the RGB LCD mode, but this does NOT apply to classic ESP32.)

## Firmware implications

In `firmware/src/display.cpp` (Phase 2), the FastLED setup call:

```cpp
#include <FastLED.h>
#include "pinmap.h"

#define NUM_LEDS 25
CRGB leds[NUM_LEDS];

void setup_display() {
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128); // cap to 50% for USB-C 1.5A headroom (see usb-c-cc-resistors.md)
    FastLED.show();
}
```

The `GRB` color order matches WS2812B (they transmit green first).

The `setBrightness(128)` is ~50% — justified by the USB-C current limit research. Fine-tune during Phase 2 bring-up.

## Key takeaway for the pinout

No changes. GPIO 13 for LED data is fine; FastLED's RMT4 driver handles timing; WiFi coexistence is solved.

## Source

FastLED README and documentation:
https://github.com/FastLED/FastLED/blob/master/README.md

Fetched 2026-04-15.
