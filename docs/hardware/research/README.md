# Hardware research notes

Per-component research for the daughters' clocks hardware. Each file cites its source and captures the specific facts that inform `docs/hardware/pinout.md` and future PCB design.

When a spec is challenged ("are you sure GPIO 0 must be HIGH?"), the chain of evidence is: pinout.md → this directory → original manufacturer doc.

## Components researched

| Component | File | Last verified |
|---|---|---|
| MAX98357A (I²S amp) | [max98357a.md](max98357a.md) | 2026-04-15 |
| WS2812B (addressable RGB LED) | [ws2812b.md](ws2812b.md) | 2026-04-15 |
| DS3231 ZS-042 (RTC breakout module) | [ds3231-zs042.md](ds3231-zs042.md) | 2026-04-15 |
| HW-125 (microSD SPI breakout) | [hw125-microsd.md](hw125-microsd.md) | 2026-04-15 |

## Still-needed research

- ESP32-WROOM-32 boot behavior for every GPIO pin we use (to confirm no unintended output during boot, e.g. on GPIO 13 = LED data)
- USB-C CC-resistor spec behavior without PD (confirm 5V/1.5A vs 5V/3A delivered to our breakout)
- FastLED timing requirements on ESP32 — does it use RMT peripheral, and does that conflict with anything else?

Added to the pinout doc's risks section; actual research pending as Phase 2 develops.
