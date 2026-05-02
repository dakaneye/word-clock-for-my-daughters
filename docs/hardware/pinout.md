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
| 0 | — | (strapping, unused) | — | Must be HIGH at boot for run mode. Dev board has built-in pullup + BOOT button. On PCB: 10kΩ pullup to 3.3V. **Do not leave floating.** |
| 1 | TX | UART0 TX | Debug | USB serial console — reserved |
| 2 | — | (strapping, unused) | — | On-board LED on many dev boards; leave unused to avoid conflicts |
| 3 | RX | UART0 RX | Debug | USB serial console — reserved |
| 4 | — | (reserved) | — | Free for future use |
| 5 | OUT | SD CS | microSD | VSPI CS. Strapping pin — pull HIGH at boot to prevent boot loop |
| 12 | — | (strapping, unused) | — | Controls flash voltage (must be LOW at boot for 3.3V flash). Default pulldown is correct. Reserved — don't drive HIGH during boot. |
| 13 | OUT | LED_DATA | WS2812B | Data line into the LED strip. 3.3V→5V level may need a 74HCT245 or similar on PCB; test on breadboard first. |
| 14 | IN | BUTTON_AUDIO | Buttons | Play/stop audio. INPUT_PULLUP in firmware. |
| 15 | — | (strapping, unused) | — | HSPI CS default. Leave unused — HSPI not used here. |
| 16 | — | (reserved) | — | Free for future use |
| 17 | — | (reserved) | — | Free for future use |
| 18 | OUT | SD CLK | microSD | VSPI SCLK |
| 19 | IN | SD MISO | microSD | VSPI MISO |
| 21 | I/O | I²C SDA | RTC | Default I²C bus for DS3231. 4.7kΩ pullup — see Power section caveat. |
| 22 | OUT | I²C SCL | RTC | Default I²C bus for DS3231. 4.7kΩ pullup — see Power section caveat. |
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

- **Input:** USB-C from the back panel via a captive Micro-USB-to-USB-C cable (3-6 ft) permanently plugged into the ESP32 module's on-board micro-USB port; the USB-C end exits through a back-panel cable hole at the bottom-right. The hole is 16 mm (drilled out from the as-cut 6 mm during assembly) so the USB connector overmold can thread through; strain relief comes from an internal cable P-clip screwed to the back-panel interior 2-3 cm in from the hole, not the grommet. No panel-mount adapter, no pigtail, no breakout board. The CP2102 bridge on the module handles enumeration for firmware flashing; the module's 5V pin feeds the main +5V rail. The Cermant USB-C breakout that originally appeared on the schematic was removed before submission; the rework record is archived at `docs/archive/hardware/2026-04-17-usb-c-breakout-removal.md`. Power negotiation is now whatever the module's micro-USB input does (no PD chip anywhere; relies on the 5V/2A charger delivering full rated current).
- **Power budget (worst case):**
  - WS2812B 25 LEDs full white: 1.5A @ 5V = 7.5W
  - ESP32 WiFi active peak: ~500 mA @ 5V (after 3.3V reg) = 2.5W
  - MAX98357A at full 3W audio: ~600 mA @ 5V = 3W
  - **Combined peak: ~2.6A / 13W** — exceeds a strict-spec USB-C source.
- **Mitigation:** real use case avoids peak coincidence (bedtime dim = 10% LEDs; audio rare). Typical draw <700 mA. If sag is observed, cap LED brightness in firmware to ~60% or add per-LED current limit.
- **5V rail:** powers WS2812B strip directly, plus MAX98357A.
- **3.3V rail:** from ESP32 dev board's onboard regulator (AMS1117-3.3). Powers DS3231, microSD breakout, MAX98357A logic, button pullups.
- **Decoupling:** 100nF ceramic on every IC's Vcc pin (added on PCB, not breadboard).
- **Bulk cap:** 1000µF electrolytic near the LED strip's 5V input (smooths WS2812B current spikes).
- **LED data line:** 300Ω series resistor in-line with DIN recommended (protects the first LED from voltage spikes on startup).
- **I²C pullups:** 4.7kΩ SDA + SCL — **verify the DS3231 breakout doesn't already have pullups** before adding them on the PCB. Stacking pullups drives the bus too hard. Adafruit DS3231 boards include pullups; cheaper clones vary.

## Mechanical — bottom-side component heights

Measured from the bottom face of the PCB outward. Drives the back-panel standoff
height (≥ tallest feature) and the total enclosure-depth budget.

| Ref | Part | Package | Est. height (mm) | Source |
|---|---|---|---:|---|
| C2 | 1000µF / 10V electrolytic | `CP_Radial_D10.0mm_P5.00mm` | **16** | common D10 radial cap spec; spec already cites this |
| U1 | ESP32-WROOM-32 dev module (52.16×28.47 mm PCB, 1.14 mm thick) | `ESP32_38Pin` on 2.54 mm headers | **12.26 (measured)** | Full stack end-to-end caliper-measured 2026-04-20. Slightly under the 13 mm estimate. |
| J_AMP1 | 1×7 header + MAX98357A breakout (GODIYMODULES clone, 17.66×18.73 mm PCB) | `PinHeader_1x07_P2.54mm_Vertical` + daughter | **~11.3** (no terminal, measured) / **~19** (screw-terminal) | 8.5 mm header + 1.6 mm breakout PCB + 1.23 mm chip side *(caliper-measured 2026-04-20; total breakout + component = 2.83 mm)*. **+8 mm if using screw-terminal variant** — decision: **skip the terminal**, solder JST-PH2.0 speaker leads direct to `+` / `−` pads. |
| J_RTC1 | 1×6 header + DS3231 ZS-042 (21.83×38.28 mm PCB, 1.09 mm thick) | `PinHeader_1x06_P2.54mm_Vertical` + daughter | **12.6 (measured)** | Full stack end-to-end (male pin tip through PCB to top of CR2032 holder) caliper-measured 2026-04-20. Module ships with 1×6 male header pre-soldered — no header to install. ~2.4 mm shorter than original 15 mm estimate. |
| J_SD1 | 1×6 header + HW-125 microSD breakout (24.22×42.03 mm PCB, 1.54 mm thick) | `PinHeader_1x06_P2.54mm_Vertical` + daughter | **7.27 (measured)** | Caliper-measured 2026-04-20; the pre-soldered 1×6 pin connector is the tallest feature (taller than the SD-card slot). ~5.7 mm shorter than original 13 mm estimate. Module ships with pin header pre-soldered — no header to install. |
| SW1-3 | 6 mm tact switches | `SW_PUSH_6mm` | **~8** | 5 mm body + 3 mm plunger |
| C1, C3-C6 | 100 nF ceramic disc | `C_Disc_D5.0mm_W2.5mm_P5.00mm` | **~7** | 5 mm disc + leads |
| U2 | 74HC245 DIP-20 | `DIP-20_W7.62mm` | **~4** | direct solder; add ~5 mm if using a socket |
| R1 | 300Ω 0207 axial (horizontal) | `R_Axial_DIN0207` | **~3** | horizontal orientation |

**Tallest feature (all caliper-measured 2026-04-20):** DS3231 at 12.6 mm and ESP32 module at 12.26 mm tie as the tallest installed daughterboards. All four measured under their original estimates — ESP32 12.26 mm (−0.74), MAX98357A 11.33 mm (−0.67), DS3231 12.6 mm (−2.4), HW-125 7.27 mm (−5.73). The screw-terminal-equipped MAX98357A variant would still be ~19 mm but we're skipping the screw terminal.

**Confidence:** [HIGH] for all four daughterboards — caliper-verified 2026-04-20.

**Headroom in standoff budget:** with all daughterboards measured, the 20 mm standoff recommendation has ~7.4 mm of headroom against the tallest (DS3231 at 12.6 mm). **Decision window open:** the standoff could drop from 20 mm to 15 mm and grow the light channel from 18 mm to ~23 mm for measurably better LED diffusion. Trade-off: tighter depth tolerance (2.4 mm margin vs 7.4 mm) and no assembly slack for thicker-than-expected components. Re-decide after diffuser-stack validation on the breadboard (Step 5 outcome).

**Back-panel standoff recommendation:** **20 mm** (off-the-shelf brass M3 standoff size; clears the MAX98357A screw-terminal variant with ~1 mm margin; clears all other components with 5+ mm margin).

**Enclosure-depth budget** (48 mm frame, per `enclosure/scripts/render_frame.py`):

| Layer | Thickness (mm) |
|---|---:|
| Face (wood, glued to top of frame) | 3.2 |
| Diffuser (paper or frosted PETG) | ~0.5 |
| Light channel | ~18 (remainder — see below) |
| PCB | 1.6 |
| Standoff (air gap for bottom-side components) | 20 |
| Back panel (wood) | 3.2 |
| Sum + ~1.5 mm assembly slack | 48 |

Light-channel depth of ~18 mm is well above the minimum for WS2812B diffusion (roughly 1× LED spacing ≈ 14 mm), so pockets will mix to uniform light before reaching the diffuser. If the MAX98357A variant turns out to be the 12 mm non-screw-terminal kind, the standoff can drop to 15 mm and the light channel can grow to ~23 mm for even better diffusion — decide after caliper pass.

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

## Datasheet-verified peripheral compatibility

Cross-checked 2026-04-15 against Adafruit NeoPixel Überguide, MAX98357A datasheet, DS3231 ZS-042 docs, and HW-125 microSD module docs.

| Peripheral | Vin range | 3.3V logic compatible? | Notes |
|---|---|---|---|
| **MAX98357A** | 2.5-5.5V (use 5V for full output) | ✅ yes — I²S tolerates 3.3-5V | Default GAIN = 9dB (floating). SD pin has internal 100kΩ pulldown; Adafruit breakout adds 1MΩ pullup to Vin → stereo-averaged output. Leave SD unconnected. |
| **DS3231 (ZS-042 module)** | 2.3-5.5V | ✅ yes — I²C is open-drain | **⚠️ Charging-circuit bomb — see Critical issues below.** Most ZS-042 clones have 4.7k-10k pullups onboard; verify with a multimeter before adding duplicates. |
| **MicroSD breakout (HW-125 style)** | 5V input OK | ✅ yes — onboard 3.3V regulator + 74LVC125A level shifter | Wire VCC to 5V, not 3.3V — the onboard regulator and level shifter need the higher input to work as designed. |
| **WS2812B LED strip** | 5V strict | **❌ no — needs level shifter** — see Critical issues below. | 60 mA / LED max; use 20 mA average for budgeting. |

## Critical issues to resolve BEFORE powering anything up

These are hardware modifications or additions required for safe operation. Do them in this order.

### ⚠️ 1. DS3231 ZS-042 trickle-charge circuit — skip the removal, mitigate with battery-swap cadence

**Decision (2026-04-20): we are NOT removing the 200Ω + 1N4148 trickle-charge resistor.** See `docs/hardware/assembly-plan.md` §Long-term maintenance for the CR2032 replacement cadence that makes this safe.

**The FUD-free picture:** The ZS-042 module has a 200Ω + 1N4148 trickle-charge circuit intended for rechargeable LIR2032 cells. We run a non-rechargeable CR2032 instead. The circuit only attempts to charge the battery when Vbat drops below (Vcc − 0.7V) — because the 1N4148 is reverse-biased otherwise.

At the design's **3.3V VCC** (DS3231 ties to the ESP32 module's 3V3 pin, not the 5V rail), the effective charge voltage is 3.3 − 0.7 = **2.6V**. A fresh CR2032 sits at 3.0V. The diode is reverse-biased → **zero charge current flows**. The circuit is functionally inert as long as the battery is above end-of-life voltage.

**What actually creates risk:** letting the CR2032 discharge below ~2.6V before replacement (it would then start charging, with leak/vent potential over years). Mitigation is replacement schedule, not hardware modification — see assembly-plan.md.

**Do NOT wire VCC to 5V.** On 5V the math inverts: 5.0 − 0.7 = 4.3V applied to the CR2032 (above its 3.0V) → active charging while powered → trickle-charge circuit engages → real risk. Our `pinmap.h` / pinout table ties DS3231 VCC to 3V3 intentionally.

### 🔴 2. WS2812B: level shifter is required, not optional

Adafruit's NeoPixel Überguide explicitly warns: **"a 3.3V signal may not work reliably."** The WS2812B logic HIGH threshold is 0.7 × Vdd = 3.5V at 5V supply. ESP32 outputs 3.3V max — 200 mV below threshold.

**Sometimes it works** (strip length, ambient temperature, LED batch tolerance all affect it). Sometimes only the first 10 LEDs light correctly. Sometimes colors drift. Relying on it is a bad idea for a keeper clock that must work for decades.

**Action:** include a **74HCT245** octal bus transceiver (or 74HCT125 quad buffer, same thing for this purpose) on the data line, powered from 5V, input from ESP32 GPIO13. The HCT family has TTL-compatible input thresholds — reliably reads 3.3V as HIGH and outputs 5V HIGH. Cost: ~$1. Not on the current wishlist; add to next order or rob from another project.

Alternative workaround (lower quality): single diode in series on Vdd to drop NeoPixel supply to ~4.3V, lowering the HIGH threshold to ~3.0V. Works but adds heat and reduces brightness. Not recommended for production.

### 🔴 3. WS2812B bulk capacitor is REQUIRED before power

Adafruit: "connect capacitor before applying power." A 1000µF electrolytic across the LED strip's +5V and GND inputs absorbs the inrush when all LEDs power up white. Already listed in the Power section — calling it out again here because skipping it can fry individual LEDs on first power-up.

## Pre-power smoke test (breadboard-bring-up protocol)

Follow this sequence when the parts arrive. **Do not skip steps.** Each check takes ~1 minute; total ~15 minutes before first power-on.

1. **Visual inspection.** Every breakout on a clean table. Verify no shipping damage, no bent pins, no foreign material.
2. **Multimeter: USB-C breakout.** Plug USB-C, verify 5.0V ±0.25V between 5V pad and GND pad. Verify CC1 and CC2 pads each show ~5.1kΩ to GND (unplugged). If either reading is wildly off — different board batch. Stop.
3. **Multimeter: DS3231 module.** With no battery installed:
   - Locate the 200Ω charging resistor (series between Vcc and battery+). Remove it.
   - Verify SDA-to-Vcc and SCL-to-Vcc pullup resistance — typically 4.7k-10k. Note value for later; if present, **do not add external pullups to the PCB**.
   - Install a fresh CR2032 (+ side up).
4. **Multimeter: MAX98357A.** Verify GAIN pad (center) is not shorted to anything by default (floating → 9dB). Verify SD pad has a weak pulldown to GND (~100k) or weak pullup to Vin (1MΩ on Adafruit).
5. **ESP32 alone, no peripherals:**
   - Connect USB-C breakout to the ESP32's 5V and GND rails.
   - Apply USB-C. Measure 3.3V rail on the ESP32 — should read 3.30V ±0.1V.
   - Flash a blink sketch. Confirm the ESP32 runs. Disconnect power.
6. **Add RTC:**
   - Wire SDA, SCL, Vcc (3.3V), GND.
   - Flash an I²C scanner sketch. Expect to see address `0x68` (DS3231). Disconnect power.
7. **Add microSD breakout:**
   - Wire VSPI pins (CLK, MOSI, MISO, CS) and VCC to 5V, GND.
   - Insert a formatted FAT32 card.
   - Flash an SD read-test sketch. Confirm file listing. Disconnect power.
8. **Add MAX98357A:**
   - Wire I²S pins (BCLK, LRC, DIN), Vcc to 5V, GND, speaker via the JST-PH2.0 pigtail.
   - Flash a 440 Hz tone test. Confirm audible tone. Disconnect power.
9. **Add WS2812B (after level shifter):**
   - Wire 74HCT245: Vcc to 5V, GND to GND, A-input from GPIO13, B-output to strip DIN, OE/DIR tied for unidirectional.
   - Add 1000µF cap across strip +5V and GND, installed first.
   - Wire strip Vdd to 5V (short run, 18 AWG or better), GND to breadboard GND.
   - Flash a single-LED test at low brightness (e.g., 10/255). Confirm first LED lights the correct color. Then test 5 LEDs. Then the full 25.
10. **Full integration:** only after steps 1-9 pass independently, flash the Phase 2 firmware and run the full rig.

If any step fails, stop and debug **that step** before adding another peripheral.

## Revision log

- **v0 (2026-04-15):** Initial assignment based on ESP32-WROOM-32 standard pin conventions, Adafruit ESP32 Feather + MAX98357A reference designs, and the parts in the Phase 2 order. Locked pending breadboard validation (Phase 2 activity R).
- **v0.1 (2026-04-15, same day review):** Corrected GPIO 0 advice (was misleadingly said "leave floating"). Added power-budget analysis (worst case ~2.6A exceeds USB-C-without-PD spec). Added 300Ω series on LED data, I²C pullup-stacking caveat. Removed stale "Audio BFF" reference (that product is for QT Py, not classic ESP32).
- **v0.2 (2026-04-15, after datasheet review):** Added datasheet-verified compatibility table. **Critical findings:** (1) DS3231 ZS-042 module has a battery-charging circuit that must be disabled before installing a non-rechargeable CR2032 — risk of battery leak/explosion. (2) WS2812B level shifter (74HCT245) is REQUIRED, not optional — Adafruit explicitly warns 3.3V signals are unreliable. Added full pre-power smoke-test protocol. Raw research saved to `docs/hardware/research/`.
