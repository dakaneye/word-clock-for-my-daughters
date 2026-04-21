# Breadboard Bring-Up Guide

How to take the Phase 2 parts order and validate every subsystem on a
breadboard before committing to the final PCB. Step-by-step, one peripheral
at a time, with known-good test sketches.

**Read first:**
- `docs/hardware/pinout.md` — pin assignments, power budget, and the
  **pre-power smoke test protocol** (multimeter checks before any power
  is applied). The pin numbers in this guide reference pinout.md.
- `docs/hardware/usb-c-breakout-removal-guide.md` — context on how power
  flows on the final PCB (breadboard uses the breakout; PCB doesn't).
- `docs/hardware/phase-2-parts-list.md` — what each part is and why it's
  chosen. Every peripheral below assumes those parts specifically.

**Use this doc as** the bench procedure. Run each section in order;
each builds on the last.

---

## Breadboard primer (read this if it's been a while)

A breadboard is a plastic block full of holes with metal clips hidden
underneath. Pushing a component lead or jumper wire into a hole makes an
electrical contact. Which holes are electrically tied together depends
on their position, not on anything you do — the wiring is baked into
the board.

### What's connected to what

There are two kinds of hole groups:

1. **Power rails** (also called "buses") — the long strips that run
   along the outer edges, marked with a red `+` line and a blue/black
   `-` line. Every hole in the same colored column is a single long
   wire. Connect your power source to one hole and you've powered the
   entire column. These are for distributing +V and GND across the
   board.

2. **Terminal strips** — the main field in the middle, with the long
   center gutter running down the length. Each **row of 5 holes** on
   one side of the gutter is tied together. Rows are **independent of
   each other** (row 1 is not connected to row 2). The gutter
   **breaks** the connection, so the 5 holes on the left of the gutter
   are NOT connected to the 5 holes on the right of the same row.
   (That gutter is there so DIP chips — like the 74HCT245 in Step 5 —
   can straddle it with legs on each side in independent rows.)

### How to connect two things

Either put both leads in the **same group** of connected holes (same
power-rail column, or the same 5-hole row on one side of the gutter),
or run a **jumper wire** from one group to another. Jumper wires are
short pre-stripped wires that plug into two holes at once.

### How to power the board

Pick one source (here: the USB-C breakout's 5V/GND pads). Run two
jumpers from the source to the `+` and `-` columns of the rail you're
going to use. Now every 5V peripheral can grab power from any hole in
the `+` column and every GND can come from any hole in the `-` column.

Same trick for 3.3V: after the ESP32 brings up its onboard regulator,
jumper the ESP32's `3V3` pin to a **different** rail column so 5V and
3.3V don't collide. All `-` (GND) columns get bridged together with a
jumper — one shared ground for the whole board.

### Ground rules (literal)

- **One common GND.** Every peripheral's GND goes to the same `-` rail.
  Tie all `-` columns together with a jumper. 90% of "it's not working"
  on a breadboard is a missing GND connection.
- **Disconnect USB before rewiring.** Don't rearrange live wires — a
  stray lead brushing the wrong hole can instantly fry a regulator or
  a dev board.
- **Leads push straight in.** Friction holds them. If a lead is too
  thick or too thin to stay put, use a jumper or trim the lead.
- **Double-check polarity before powering up.** Reversed +/- on the
  rail will cook polarized parts (electrolytic caps, ICs, the ESP32
  itself). The smoke test in `pinout.md` catches this before power
  goes on.

---

## Workspace setup

### Breadboard layout (the salvaged dual-bus board, ~2400 tie points)

This particular board is a **dual-bus** layout: two independent
breadboard sections stacked top-and-bottom, each with its own pair of
power rails on each side. Four rail pairs total: Va / Vb (top section)
and Vc / Vd (bottom section).

```
 top    section:  [ Va+ Va- | rows 1..~30 | gutter | rows ~30..60 | Vb+ Vb- ]
 bottom section:  [ Vc+ Vc- | rows 1..~30 | gutter | rows ~30..60 | Vd+ Vd- ]
```

Read the diagram as: the leftmost two columns (`Va+`, `Va-`) are a pair
of power rails running the full length of the top section. The main
terminal field is next, split by the center gutter. Then another pair
of power rails on the right (`Vb+`, `Vb-`). Bottom section is the same
layout again with its own rail labels.

Assign power rails consistently across the whole session:
- **Va+ → 5V** (from USB-C breakout's 5V pad)
- **Va- → GND**
- **Vb+ → 3.3V** (from ESP32 dev board's 3V3 pin)
- **Vb- → GND** (same GND as Va-; tie them together with a jumper)
- **Vc, Vd rails** — leave free for peripheral-specific rails, or bridge to
  the main rails as needed.

**Before plugging anything in:** jumper `Va-` ↔ `Vb-` ↔ `Vc-` ↔ `Vd-`
so all four ground rails are the same GND. Do this first and leave it
for the whole bring-up.

### ESP32 dev board placement

Straddle the breadboard's center split with the ESP32 dev board.
Pins on each side of the board land in the outer 5 rows on each half
of the breadboard. USB-C port points off the edge so the cable doesn't
block breadboard rows.

### Shared rules (all sketches)

- All sketches compile with PlatformIO `[env:emory]` or any Arduino-IDE
  setup targeting ESP32-WROOM-32.
- **Serial monitor at 115200 baud** throughout.
- Ground everything to one common GND. Every peripheral's GND pin goes to
  the same rail. Floating grounds cause 90% of "it doesn't work" moments.
- **Disconnect power between wiring changes.** Don't rearrange live.

---

## Step 1 — ESP32 alone

**Goal:** confirm the USB-C path delivers stable 5V, the 3.3V regulator on
the dev board is working, and the flashing toolchain talks to the chip.

### Wiring

Only the USB-C breakout and the ESP32.

| From | To |
|---|---|
| USB-C breakout 5V | Va+ |
| USB-C breakout GND | Va- (and Vb-) |
| ESP32 VIN (or 5V pin) | Va+ |
| ESP32 GND | Va- |
| ESP32 3V3 pin | Vb+ |

### Sketch: `01_blink.cpp`

Most ESP32 dev boards have a blue onboard LED on GPIO 2.

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 alive");
  pinMode(2, OUTPUT);
}

void loop() {
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  delay(500);
  Serial.println("blink");
}
```

### Expected

- On-board blue LED blinks at 1 Hz.
- Serial monitor prints `ESP32 alive` once, then `blink` every second.
- Multimeter: 3V3 pin reads 3.30 V ± 0.1 V.

### If it fails

- No LED, no serial: double-check USB-C breakout's CC resistors (5.1kΩ
  each to GND). Without those, many chargers won't supply 5V.
- Serial garbage: baud rate mismatch — set 115200.
- Flash fails with "timed out waiting for packet header": hold the BOOT
  button on the ESP32 dev board while flashing, release when flashing
  starts.

---

## Step 2 — DS3231 RTC (I²C)

**Goal:** confirm I²C works, the DS3231 is alive at address `0x68`, and
the battery-charging circuit has been disabled.

**Prerequisite:** complete pinout.md step 3 (remove the 200Ω charging
resistor, install CR2032). Do NOT skip this — the module ships with a
trickle-charger that can vent a non-rechargeable CR2032.

### Wiring

| From | To | Rail |
|---|---|---|
| DS3231 VCC | ESP32 3V3 | Vb+ |
| DS3231 GND | GND | Va- |
| DS3231 SDA | ESP32 GPIO 21 | — |
| DS3231 SCL | ESP32 GPIO 22 | — |

The ZS-042 module has onboard SDA/SCL pullups (4.7k-10kΩ). Do not add
external pullups for breadboard bring-up — the internal ones are enough.

### Sketch: `02_i2c_scan.cpp`

```cpp
#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL
  delay(100);

  Serial.println("I2C scan:");
  int count = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", addr);
      count++;
    }
  }
  Serial.printf("done — %d device(s)\n", count);
}

void loop() {}
```

### Expected

```
I2C scan:
  found 0x57
  found 0x68
done — 2 device(s)
```

- `0x68` is the DS3231 itself.
- `0x57` is the onboard AT24C32 EEPROM (comes with the ZS-042 module).
  Not used by firmware but confirms the module is wired correctly.

### If it fails

- No devices found: GND is probably not shared, or SDA/SCL are swapped.
- Only 0x68 found (no 0x57): the EEPROM chip on this specific clone is
  missing or dead — functionally fine, but note for repair planning.
- Multiple devices at unexpected addresses: you probably have something
  else on the I²C bus; disconnect it.

### Set + read time sketch (optional sanity check)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  if (!rtc.begin()) { Serial.println("no RTC"); return; }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power; setting time from compile");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  DateTime now = rtc.now();
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
  delay(1000);
}
```

Requires `adafruit/RTClib` in `lib_deps` (already in `esp32-base`).

Expected: prints incrementing timestamp once per second. Power off the
ESP32 (keep CR2032 installed), leave overnight, power on next day with the
"set time from compile" line commented out → time should have advanced
correctly. Confirms the RTC keeps running on battery.

---

## Step 3 — microSD breakout (VSPI)

**Goal:** confirm SPI works, the SD card is detected, and the FAT32
filesystem reads.

### Wiring

| From | To | Rail |
|---|---|---|
| microSD VCC | 5V | Va+ |
| microSD GND | GND | Va- |
| microSD CS | ESP32 GPIO 5 | — |
| microSD MOSI | ESP32 GPIO 23 | — |
| microSD MISO | ESP32 GPIO 19 | — |
| microSD SCK | ESP32 GPIO 18 | — |

Power the breakout from **5V**, not 3.3V — its onboard regulator + level
shifter chain need the higher input.

Insert a FAT32-formatted card with `lullaby.mp3` and `birth.mp3` at root
(per the audio-format decision in `TODO.md`).

### Sketch: `03_sd_list.cpp`

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

void setup() {
  Serial.begin(115200);
  if (!SD.begin(5)) {  // CS pin
    Serial.println("SD init failed");
    return;
  }
  Serial.println("SD init ok");

  File root = SD.open("/");
  File f = root.openNextFile();
  while (f) {
    Serial.printf("  %s  %lu bytes\n", f.name(), f.size());
    f = root.openNextFile();
  }
  Serial.println("done");
}

void loop() {}
```

### Expected

```
SD init ok
  lullaby.mp3  2453120 bytes
  birth.mp3  487936 bytes
done
```

### If it fails

- `SD init failed`: check wiring (MOSI/MISO often swapped). Confirm card
  is FAT32, not exFAT. Cards >32 GB default to exFAT — reformat.
- Init OK but no files listed: card is readable but empty. Check the
  files are actually at the root, not in a subfolder.
- Random corrupt file listings: the microSD breakout might not be
  getting a stable 5V. Measure the breakout's VCC pin while running; if
  it dips below 4.5V, bulk-cap the 5V rail or switch USB power sources.

---

## Step 4 — MAX98357A + speaker (I²S)

**Goal:** confirm I²S audio path works end-to-end.

**Prereq:** speaker is JST-PH2.0 pre-attached (DWEII 4-pack from the parts
order). Plug directly into the MAX98357A's speaker output terminals.

### Wiring

| From | To | Rail |
|---|---|---|
| MAX98357A Vin | 5V | Va+ |
| MAX98357A GND | GND | Va- |
| MAX98357A DIN | ESP32 GPIO 27 | — |
| MAX98357A BCLK | ESP32 GPIO 26 | — |
| MAX98357A LRC | ESP32 GPIO 25 | — |
| MAX98357A SD | (leave unconnected) | — |
| MAX98357A GAIN | (leave unconnected) | — |

- SD pin floating → stereo-average channel (what we want for mono).
- GAIN pin floating → 9 dB gain (default).

### Sketch: `04_tone.cpp`

Simple 440 Hz square wave using ESP-IDF I²S driver. Arduino-ESP32 core
v2.x uses the legacy `driver/i2s.h` API; if using v3.x, the API is
different and the sketch below will need updating.

```cpp
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

constexpr int SAMPLE_RATE = 44100;
constexpr int FREQ = 440;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S tone test");

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);

  i2s_pin_config_t pins = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 27,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(I2S_NUM_0, &pins);
}

void loop() {
  static float phase = 0;
  const float step = 2.0f * M_PI * FREQ / SAMPLE_RATE;
  int16_t buf[256];
  for (int i = 0; i < 256; i += 2) {
    int16_t s = (int16_t)(sinf(phase) * 8000);
    buf[i]   = s;  // L
    buf[i+1] = s;  // R
    phase += step;
    if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
  }
  size_t written;
  i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
}
```

### Expected

- Clean 440 Hz tone out of the speaker (A above middle C — same pitch as
  an orchestra tuning reference).
- Steady amplitude, no crackle, no buzz.

### If it fails

- Silent: check DIN, BCLK, LRC are on the right GPIOs and not swapped.
  I²S is picky about which line is which.
- Crackly / distorted: Vin might be below 5V under load. Measure while
  playing. If dipping below 4.2V, bulk-cap the 5V rail (470 µF–1000 µF
  electrolytic) near the MAX98357A.
- Tone plays but at wrong pitch: sample rate mismatch between the I²S
  driver setup and the sketch — check the `SAMPLE_RATE` constant.
- Buzz during silence (long `delay()` in a future version): I²S DMA
  buffer underruns. Keep feeding the driver continuously.

### Optional: play lullaby.mp3 from SD

Once tone works, the natural next test is decoding the actual MP3 files
off the SD card. The ESP8266Audio library is the standard choice:

```
lib_deps = earlephilhower/ESP8266Audio
```

Sketch skeleton (don't bother until the `audio` firmware module is being
written — the smoke test above is enough to prove the I²S hardware):

```cpp
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>

AudioFileSourceSD *src;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;

void setup() {
  SD.begin(5);
  src = new AudioFileSourceSD("/lullaby.mp3");
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27);  // BCLK, LRC, DIN
  mp3 = new AudioGeneratorMP3();
  mp3->begin(src, out);
}

void loop() {
  if (mp3->isRunning()) mp3->loop();
}
```

---

## Step 5 — WS2812B LEDs + 74HCT245 level shifter

**Goal:** confirm the 3.3V → 5V level-shift path works and the strip
addresses cleanly.

**Prereqs (all critical — see pinout.md):**
- **Bulk cap first:** a 1000 µF electrolytic across the strip's +5V and
  GND inputs, installed BEFORE power is applied. Skipping this can fry
  the first LED on first power-on.
- **Level shifter required:** 74HCT245 (or 74HCT125) on the data line.
  The ESP32's 3.3V logic HIGH is below the WS2812B's 3.5V threshold —
  sometimes works, sometimes doesn't, never reliably.
- **Series resistor on data line:** 300-470 Ω between the level shifter
  output and the strip's DIN pin. Protects the first LED from edge-rate
  spikes on startup.

### Wiring (74HCT245 in unidirectional mode, A → B)

| From | To | Rail |
|---|---|---|
| 74HCT245 pin 20 (VCC) | 5V | Va+ |
| 74HCT245 pin 10 (GND) | GND | Va- |
| 74HCT245 pin 1 (DIR) | 5V (VCC) | — |
| 74HCT245 pin 19 (/OE) | GND | — |
| 74HCT245 pin 2 (A1) | ESP32 GPIO 13 (through 300Ω optional) | — |
| 74HCT245 pin 18 (B1) | WS2812B DIN (through 300Ω series) | — |
| WS2812B VDD | 5V (separate thick wire, 18 AWG+) | Va+ |
| WS2812B GND | GND (separate thick wire) | Va- |
| 1000 µF cap (+) | WS2812B VDD | — |
| 1000 µF cap (-) | WS2812B GND | — |

Route the LED's 5V/GND on thick wire (18 AWG) directly to the USB power
source, not through the breadboard rails — breadboard contact resistance
sags under the strip's inrush current.

Start with **just 1 LED** cut from the 5m strip. You can always add more;
starting with the full strip on breadboard is asking for trouble.

### Sketch: `05_fastled.cpp`

```cpp
#include <Arduino.h>
#include <FastLED.h>

constexpr int PIN_DATA = 13;
constexpr int NUM_LEDS = 1;

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, PIN_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(10);  // low brightness for bench work
}

void loop() {
  leds[0] = CRGB::Red;    FastLED.show(); delay(500);
  leds[0] = CRGB::Green;  FastLED.show(); delay(500);
  leds[0] = CRGB::Blue;   FastLED.show(); delay(500);
  leds[0] = CRGB::Black;  FastLED.show(); delay(500);
}
```

### Expected

- LED cycles R → G → B → off at 2 Hz.
- Colors are clean (not pink where it should be red — that indicates
  color channel ordering is wrong; change `GRB` to `RGB` or vice versa).

### If it fails

- No light at all:
  - Multimeter on the strip's VDD pin: should read 5.0 V. If lower,
    check the USB source can supply enough current.
  - Probe the 74HCT245's A1 pin (input) with a scope or logic probe:
    should see the WS2812B's 800 kHz data pulses during `FastLED.show()`.
  - Probe B1 (output): same signal at 5V amplitude.
- First LED lights but not subsequent ones:
  - Almost always a cold joint between LEDs or the 300Ω resistor in the
    wrong place (it should be in series between the level shifter and
    the FIRST LED's DIN, not between LEDs).
- All LEDs flicker or show wrong colors:
  - Ground loop. Confirm every GND meets at one point, and that the
    thick LED GND wire connects back to the USB source's GND, not just
    the breadboard rail.
- First LED is noticeably dim or discolored after first power-on:
  - The bulk cap was missed or wired backwards. Swap for a new strip
    piece (that LED may be damaged) and install the cap properly.

### Scaling up

Once 1 LED works:
- Cut a 5-LED strip. Verify the `leds[]` array size matches, all 5
  light correctly, no flicker.
- Cut a 25-LED strip (the production count). Confirm the whole chain
  works at low brightness, then gradually increase to 255. Watch for
  voltage sag — the last LEDs in the chain may tint red or drop out if
  the strip can't deliver full current along its length.

---

## Step 6 — Buttons (tact switches)

**Goal:** confirm buttons read clean and the ESP32's INPUT_PULLUP works.

### Wiring

3 × 6mm tact switches. Each switch has 4 pins arranged in a 2×2 grid;
diagonally-opposite pins are the same contact when pressed.

| Button | ESP32 GPIO |
|---|---|
| Hour-set | 32 |
| Minute-set | 33 |
| Audio play/stop | 14 |

For each button: one leg to the GPIO, the opposite-corner leg to GND.
No external pullup resistor — the ESP32's INPUT_PULLUP does it.

### Sketch: `06_buttons.cpp`

```cpp
#include <Arduino.h>

constexpr int PIN_HOUR = 32;
constexpr int PIN_MIN  = 33;
constexpr int PIN_AUD  = 14;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_HOUR, INPUT_PULLUP);
  pinMode(PIN_MIN,  INPUT_PULLUP);
  pinMode(PIN_AUD,  INPUT_PULLUP);
}

void loop() {
  int h = digitalRead(PIN_HOUR);
  int m = digitalRead(PIN_MIN);
  int a = digitalRead(PIN_AUD);
  Serial.printf("H=%d  M=%d  A=%d\n", h, m, a);
  delay(100);
}
```

### Expected

- Unpressed: all three print `H=1  M=1  A=1` (pulled up to 3.3V).
- Pressed: the corresponding value reads 0.

### If it fails

- Always reads 1 even when pressed: wrong diagonal pins used on the
  switch; try the other pair.
- Always reads 0: the other pair of pins is wired — you've got one leg
  on GPIO and another leg also on GND from the same contact side, short
  to GND.
- Noisy / bouncy reads: to be handled by a debounce routine in the
  `buttons` firmware module; for this smoke test, raw reads are fine.

---

## Step 7 — Full integration

All peripherals on the breadboard at once. Flash the real firmware
(wifi_provision + future display/rtc/ntp/audio/buttons modules).

### Wiring summary (all on at once)

Combine everything from Steps 1–6 without changes. The only integration
concern is **power rail capacity**: running all peripherals at once under
load (LEDs at 100%, audio playing, WiFi transmitting) can pull over 2 A
from the USB source.

Mitigations if sag is observed:
- Cap LED brightness at 60% in firmware until PCB-level power delivery
  is proven.
- Bulk-cap the 5V rail near the LED strip (the existing 1000 µF cap
  should be enough).
- Swap to a 5V/3A USB source instead of 2A during bring-up if sag
  persists.

### Firmware to flash

Once every peripheral has passed isolation, flash the current Emory env:

```
cd firmware && ~/Library/Python/3.9/bin/pio run -e emory -t upload
```

The wifi_provision module (already shipped) will come up first-boot,
broadcast `WordClock-Setup-XXXX`, and wait for credentials. See
`firmware/test/hardware_checks/wifi_provision_checks.md` for the 10-step
post-flash verification.

Each subsequent Phase 2 firmware module (display, rtc, ntp, audio,
buttons) will land with its own hardware check file under
`firmware/test/hardware_checks/`. Run each after the module ships.

---

## Troubleshooting reference

### "It worked at step N-1 and stopped working at step N"

Common cause: adding peripheral N pulled ESP32's 3V3 regulator below spec
or flooded a shared bus. Pull peripheral N back off, confirm step N-1
still works, then re-add N one wire at a time.

### "Random resets under load"

Power sag. Measure 5V rail with a multimeter during the failure. If it
dips below 4.5V, the USB source can't keep up — cap the rail or switch
sources.

### "ESP32 won't flash"

Hold BOOT while initiating flash; release when the progress bar starts.
The AITRIP module doesn't have auto-reset circuitry as reliable as
Adafruit boards.

### "Serial garbage on boot"

ROM bootloader output at 74880 baud gets printed to serial on every boot
— harmless. Your application output at 115200 comes after. Set monitor
to 115200 and ignore the first few chars.

### "Intermittent LED glitches"

90% of the time: ground loop or missing level shifter. Confirm both are
correct. 10%: bad WS2812B LED mid-strip — cut it out, resolder.

### "SD card mounts once, then won't mount again after reset"

ESP32's SD library sometimes leaves the card in an undefined state if
the last operation wasn't closed cleanly. Power-cycle the card (not just
the ESP32) — the easiest way is `SD.end()` before reset, or unplug the
microSD breakout between runs during bench work.

---

## What to bring back to `TODO.md`

When each step passes, check the corresponding box in the TODO's
"Breadboard bring-up" section. When all 7 steps pass, the whole
breadboard gate is green and the PCB submission window opens. Document
any deviations (e.g., "had to use 470Ω series instead of 300Ω for WS2812B
— ran out of 300Ω") so the PCB rework captures them.
