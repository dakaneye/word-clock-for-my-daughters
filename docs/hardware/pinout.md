# ESP32 Pinout — Daughters' Clocks

**Status:** v0 — source of truth for KiCad schematic (activity B), firmware config (Phase 2), and PCB layout (activity D). Subject to minor revisions if breadboard bring-up reveals pin conflicts or noise issues.

**Target module:** ESP32-WROOM-32 (classic ESP32, not S2/S3/C3). AITRIP 3-pack dev board from the Phase 2 parts order.

## ESP32 GPIO rules (for reference while reading this)

- **GPIO 6-11:** reserved for internal SPI flash. NEVER use.
- **GPIO 0, 2, 5, 12, 15:** strapping pins — have constraints at boot. Usable but with care.
- **GPIO 1, 3:** UART0 TX/RX — keep free for USB serial logging/flashing.
- **GPIO 34-39:** input-only, no internal pullup/pulldown.
- **GPIO 25, 26:** DAC-capable (unused here — we use I²S digital audio instead).

## Signal assignments

| GPIO | Direction | Signal | Subsystem | Notes |
|---:|---|---|---|---|
| 0 | — | (strapping, unused) | — | Reserve for boot; leave floating or tie HIGH via pullup |
| 1 | TX | UART0 TX | Debug | USB serial console — reserved |
| 2 | — | (strapping, unused) | — | On-board LED on many dev boards; leave unused to avoid conflicts |
| 3 | RX | UART0 RX | Debug | USB serial console — reserved |
| 4 | — | (reserved) | — | Free for future use |
| 5 | OUT | SD CS | microSD | VSPI CS. Strapping pin — pull HIGH at boot to prevent boot loop |
| 12 | — | (strapping, unused) | — | Must be LOW at boot (default pulldown). Avoid. |
| 13 | OUT | LED_DATA | WS2812B | Data line into the LED strip. 3.3V→5V level may need a 74HCT245 or similar on PCB; test on breadboard first. |
| 14 | IN | BUTTON_AUDIO | Buttons | Play/stop audio. INPUT_PULLUP in firmware. |
| 15 | — | (strapping, unused) | — | HSPI CS default. Leave unused — HSPI not used here. |
| 16 | — | (reserved) | — | Free for future use |
| 17 | — | (reserved) | — | Free for future use |
| 18 | OUT | SD CLK | microSD | VSPI SCLK |
| 19 | IN | SD MISO | microSD | VSPI MISO |
| 21 | I/O | I²C SDA | RTC | Default I²C bus for DS3231. 4.7kΩ pullup on PCB. |
| 22 | OUT | I²C SCL | RTC | Default I²C bus for DS3231. 4.7kΩ pullup on PCB. |
| 23 | OUT | SD MOSI | microSD | VSPI MOSI |
| 25 | OUT | I²S LRC (WS) | Audio | MAX98357A word-select (LRCLK) |
| 26 | OUT | I²S BCLK | Audio | MAX98357A bit clock |
| 27 | OUT | I²S DIN | Audio | MAX98357A serial data in |
| 32 | IN | BUTTON_HOUR | Buttons | Hour-set button. INPUT_PULLUP. |
| 33 | IN | BUTTON_MINUTE | Buttons | Minute-set button. INPUT_PULLUP. |
| 34 | — | (reserved) | — | Input-only; free for future use |
| 35 | — | (reserved) | — | Input-only; free for future use |
| 36 | — | (reserved) | — | Input-only; free for future use |
| 39 | — | (reserved) | — | Input-only; free for future use |

## Subsystem summary

| Bus | Pins | Peripheral(s) |
|---|---|---|
| UART0 | 1 (TX), 3 (RX) | USB-serial debug |
| I²C | 21 (SDA), 22 (SCL) | DS3231 RTC |
| VSPI | 18 (CLK), 19 (MISO), 23 (MOSI), 5 (CS) | microSD card breakout |
| I²S | 25 (LRC), 26 (BCLK), 27 (DIN) | MAX98357A amplifier |
| GPIO | 13 (LED DATA), 14/32/33 (3 buttons) | WS2812B, tact switches |

## Power

- **Input:** USB-C 5V @ up to 2A via USB-C breakout board (5.1kΩ CC resistors)
- **5V rail:** powers WS2812B strip directly (~1.5A worst case for 25 LEDs full white)
- **3.3V rail:** from ESP32 dev board's onboard regulator (AMS1117-3.3). Powers DS3231, microSD breakout, MAX98357A logic, button pullups
- **Decoupling:** 100nF ceramic on every IC's Vcc pin (added on PCB, not breadboard)
- **Bulk cap:** 1000µF electrolytic near the LED strip's 5V input (smooths the WS2812B current spikes)

## Firmware constants

When Phase 2 firmware modules land, these macros should be defined in a shared header (e.g., `firmware/configs/pinmap.h`) and imported by every module that touches hardware:

```cpp
#pragma once

// WS2812B LED strip
#define PIN_LED_DATA     13

// DS3231 RTC (I²C default pins)
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22

// microSD card (VSPI)
#define PIN_SD_CS         5
#define PIN_SD_MOSI      23
#define PIN_SD_MISO      19
#define PIN_SD_CLK       18

// MAX98357A I²S amplifier
#define PIN_I2S_BCLK     26
#define PIN_I2S_LRC      25
#define PIN_I2S_DIN      27

// Tact switches (INPUT_PULLUP)
#define PIN_BUTTON_HOUR   32
#define PIN_BUTTON_MINUTE 33
#define PIN_BUTTON_AUDIO  14
```

## Known risks to validate during breadboard bring-up

1. **WS2812B 3.3V→5V level shift.** ESP32 outputs 3.3V logic HIGH, WS2812B expects 5V logic HIGH (threshold is 0.7×VCC = 3.5V at 5V VCC). Often works without a level shifter, but marginal. If the first LED behaves erratically or colors drift, add a 74HCT245 or a simple diode + pullup on the data line. Test on breadboard — cheap insurance.
2. **GPIO 5 (SD CS) boot strap.** Some microSD breakouts have an external pullup on CS which is fine, but check at boot that the ESP32 isn't stuck in download mode.
3. **I²S + SD on the same binary.** Simultaneous audio playback and SD reads can cause timing glitches. Test early. May need DMA buffering or reading the MP3 into RAM before playback.
4. **USB-C CC resistors.** Cermant board ships with 5.1kΩ CC resistors which correctly negotiate 5V/default current from a USB-C source. Verify with a multimeter on the first board before committing to five on a PCB.

## Revision log

- **v0 (2026-04-15):** Initial assignment based on ESP32 standard pins, Adafruit ESP32 Audio BFF reference, and the parts in the Phase 2 order. Locked pending breadboard validation (Phase 2 activity R).
