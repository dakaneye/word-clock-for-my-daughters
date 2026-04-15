# Hardware research notes

Per-component research for the daughters' clocks hardware. Each file cites its source and captures the specific facts that inform `docs/hardware/pinout.md` and future PCB design.

When a spec is challenged ("are you sure GPIO 0 must be HIGH?"), the chain of evidence is: pinout.md → this directory → original manufacturer doc.

## Components researched

| Component / topic | File | Last verified |
|---|---|---|
| MAX98357A (I²S amp) | [max98357a.md](max98357a.md) | 2026-04-15 |
| WS2812B (addressable RGB LED) | [ws2812b.md](ws2812b.md) | 2026-04-15 |
| DS3231 ZS-042 (RTC breakout module) | [ds3231-zs042.md](ds3231-zs042.md) | 2026-04-15 |
| HW-125 (microSD SPI breakout) | [hw125-microsd.md](hw125-microsd.md) | 2026-04-15 |
| ESP32-WROOM-32 GPIO boot behavior | [esp32-gpio-boot-behavior.md](esp32-gpio-boot-behavior.md) | 2026-04-15 |
| USB-C CC resistors without PD | [usb-c-cc-resistors.md](usb-c-cc-resistors.md) | 2026-04-15 |
| FastLED on ESP32 — peripheral choice + WiFi | [fastled-esp32.md](fastled-esp32.md) | 2026-04-15 |

## Deferred until Phase 2 bring-up reveals questions

Nothing currently blocking schematic or PCB work. Add here as new questions emerge.
