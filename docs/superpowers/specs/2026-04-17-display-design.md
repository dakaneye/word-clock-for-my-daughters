# Display Firmware Module — Design Spec

Date: 2026-04-17

## Overview

The `display` module turns the running clock state into pixels on the
35-LED WS2812B strip. It's the only firmware module that touches the
LED chain. Given time-of-day, date, a monotonic millisecond counter,
and the seconds-since-last-NTP-sync value from `wifi_provision`, it
produces the exact RGB value for every LED — including the holiday
palette shift, birthday rainbow cycle on the four decor words, amber
tint when the clock hasn't synced in 24 h, and the bedtime dim
multiplier. The pure-logic renderer composes cleanly with the Phase 1
modules (`time_to_words`, `holidays`, `birthday`, `dim_schedule`);
the ESP32 adapter is a thin FastLED wrapper.

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
(display listed as the first Phase 2 firmware module in TODO.md §Phase 2).

## Scope

**In:**
- Pure-logic `renderer::render(input) → Frame` that returns the final
  35-entry RGB array (dim multiplier already applied; caller writes
  it verbatim to the strip)
- `wc::display::begin()` + `wc::display::show(frame)` FastLED adapter
  (WS2812B on `PIN_LED_DATA` = GPIO 13 via 74HCT245 level shifter)
- `WordId → LED-chain-index` mapping table matching
  `hardware/word-clock-all-pos.csv`
- Palette lookup `(Palette, WordId) → Rgb` for all 10 palettes
  (warm white + 9 holiday palettes from `holidays.h`)
- Birthday-rainbow hue generator
  `(WordId, now_ms) → Rgb` for the 4 decor words
- Amber stale-sync color + `>24 h since last sync` threshold
- Dim multiplier applied inside the renderer (not via
  `FastLED.setBrightness`) so the renderer's output is the final,
  complete frame
- Native-testable pure-logic modules with Unity tests
- `#ifdef ARDUINO` guard on the FastLED adapter (same pattern as
  wifi_provision and buttons adapters — PlatformIO LDF otherwise
  pulls library sources into the native build)

**Out:**
- Deciding *whether* to call `render()` in a given frame — `main.cpp`
  owns the state-machine call for captive-portal / pre-sync boot
  (display just renders whatever it's told)
- RTC reads — `rtc` module owns DS3231 access; display consumes the
  date/time values it's handed
- NTP sync + tracking `seconds_since_last_sync()` — `wifi_provision`
  already exposes this
- Word-set computation — `wc::time_to_words()` (Phase 1) already
  returns the set of `WordId`s to light
- Holiday date detection — `wc::palette_for_date()` (Phase 1)
- Birthday detection — `wc::birthday_state()` (Phase 1)
- Brightness schedule — `wc::brightness()` (Phase 1)
- Voice-memo playback at the birth minute — `audio` module's job
- Any per-LED gamma, color-temperature correction, or FastLED power
  management — tunable during bring-up without changing this spec
- User-tunable palettes, weather-driven colors, scrolling-text modes
  — explicitly out (parent spec doesn't define them)

## Behavioral Contract

### Per-LED color precedence

Given inputs `(year, month, day, hour, minute, now_ms,
seconds_since_sync, birthday_cfg)`, for each `WordId w` the final LED
color is:

```
lit_set = time_to_words(hour, minute).words[0 .. count-1]
bday    = birthday_state(month, day, hour, minute, birthday_cfg)
palette = palette_for_date(year, month, day)
stale   = (seconds_since_sync > 86'400)  // UINT32_MAX also satisfies this

if bday.is_birthday:
    lit_set += {HAPPY, BIRTH, DAY, NAME}

if w not in lit_set:
    color = BLACK                                         // {0, 0, 0}
elif bday.is_birthday and w in {HAPPY, BIRTH, DAY, NAME}:
    color = rainbow(w, now_ms)                            // priority 1
elif palette != Palette::WARM_WHITE:
    color = color_for(palette, w)                         // priority 2
elif stale:
    color = AMBER = {255, 120, 30}                        // priority 3
else:
    color = WARM_WHITE = {255, 170, 100}                  // priority 4

color = color * brightness(hour, minute)                  // dim last
```

Priority from most-to-least important, locked during brainstorming:
**birthday rainbow (decor only) > holiday palette > amber stale-sync
> warm white**. Amber is a deliberate "don't fully trust the time"
signal; holidays and birthdays are celebration displays, so amber
yields to them.

### Rainbow

For `w ∈ {HAPPY, BIRTH, DAY, NAME}`:

- Phase offsets: `HAPPY=0°, BIRTH=90°, DAY=180°, NAME=270°`
- Hue: `hue_deg = ((now_ms % RAINBOW_PERIOD_MS) * 360 / RAINBOW_PERIOD_MS + phase_offset_deg) % 360`.
  Taking `now_ms % RAINBOW_PERIOD_MS` **before** the multiply keeps
  the intermediate product under `60'000 * 360 = 21'600'000`, well
  within `uint32_t` range. The naive form
  `now_ms * 360 / RAINBOW_PERIOD_MS` overflows `uint32_t` every
  `UINT32_MAX / 360 ≈ 3.3 hours` and produces a visible hue jump at
  each overflow boundary — unacceptable for a 60 s rainbow that
  must run for days.
- Output: standard HSV→RGB conversion at `sat=255, val=255`, done
  in a pure-C++ helper (`display::detail::hsv_to_rgb`) compiled
  into both the native and ESP32 builds. **Not** FastLED's
  `hsv2rgb_rainbow` — that variant has a custom non-linear hue
  distribution and would produce different colors than the native
  tests assert. Using a single HSV→RGB implementation across both
  environments keeps the renderer's behavior testable and
  deterministic; FastLED in the adapter is used only for pushing
  RGB bytes to the strip.
- `RAINBOW_PERIOD_MS = 60'000` — a full 360° cycle every 60 s.
- The `uint32_t now_ms` argument is allowed to wrap. Rainbow math is
  pure modular arithmetic on `now_ms`, so the hue increment across
  the wrap boundary is correct.
- **Full saturation and value** are returned pre-dim. The dim
  multiplier (step 7 of precedence) scales `val` afterward, producing
  dim-but-still-saturated rainbow at night.

### Birthday activation

- `birthday_state(...).is_birthday == true` for all 24 hours of the
  birthday date (midnight-to-midnight local, per parent spec).
- The birthday rainbow is active on decor words for that full window.
- `birthday_state(...).is_birth_minute == true` only at the exact
  birth hour:minute — not consumed by this module. The `audio`
  module uses it to trigger the voice-memo easter egg.

### Holiday palette cycling

`color_for(Palette p, WordId w)` is implemented as a `switch` over
named `Palette` enumerators — **not** as an array indexed by
`static_cast<uint8_t>(p)`. This keeps palette lookup robust against
`Palette` enum additions (a future `ST_PATRICKS` inserted between
existing values would silently break array-indexed lookup; the
switch form triggers `-Wswitch-enum` and forces the developer to
handle the new case).

Within a given palette, the word receives the color at:

```
color = palette_colors[static_cast<uint8_t>(w) % palette_colors.size()]
```

Cycling by `WordId` enum index (not by LED chain index) keeps the
palette lookup independent of PCB layout — the same palette logic
survives a PCB LED-reordering without test changes. Each word thus
receives a deterministic color within a given palette: on Halloween,
HAPPY (enum 31) gets `palette[31 % 2] = palette[1] = purple`; BIRTH
(enum 32) gets `palette[0] = orange`; etc.

### Palette power budget

Two-layer defense against palette-tuning drift blowing the PSU:

**Layer 1 — build-time check.** Every entry in every palette array
MUST satisfy `r + g + b ≤ PALETTE_MAX_RGB_SUM = 700`. Worst-case
current at this cap (all 35 LEDs lit to the sum cap simultaneously,
full brightness) is ≈ `35 × (700/765) × 60 mA ≈ 1.92 A`. The
existing defaults all fit well under 700 (max default entry is
`EASTER_PASTEL {255, 180, 200}` = 635). A
`test_palette_power_budget` Unity test enumerates every palette and
asserts the invariant on every entry, catching a bad palette tune
before it reaches the board.

**Layer 2 — runtime enforcement.** The adapter calls
`FastLED.setMaxPowerInVoltsAndMilliamps(5, 1800)` in
`display::begin()`. If the renderer ever emits a frame that would
pull more than 1.8 A on the strip, FastLED scales brightness down
automatically. This catches the "someone bumped the cap to 800 and
forgot to update the test" failure mode without damage.

The 1.8 A ceiling matches `docs/hardware/pinout.md` §Power budget
for the WS2812B strip alone, leaving headroom under the 2 A USB-C
cable rating for the ESP32 + RTC + amplifier + microSD draw.

Per-palette starting arrays (tunable on real hardware within the
sum-≤-450 constraint; the values below are defaults to commit with,
not final):

| Palette | RGB entries |
|---|---|
| `WARM_WHITE` | `[{255, 170, 100}]` |
| `MLK_PURPLE` | `[{128, 0, 180}]` |
| `VALENTINES` | `[{255, 30, 60}, {255, 120, 160}]` |
| `WOMEN_PURPLE` | `[{150, 50, 180}]` |
| `EARTH_DAY` | `[{30, 140, 60}, {30, 80, 200}]` |
| `EASTER_PASTEL` | `[{255, 180, 200}, {180, 220, 255}, {220, 255, 180}, {255, 230, 180}]` |
| `JUNETEENTH` | `[{200, 30, 30}, {10, 10, 10}, {30, 160, 60}]` |
| `INDIGENOUS` | `[{160, 80, 40}, {200, 140, 80}, {120, 60, 30}]` |
| `HALLOWEEN` | `[{255, 100, 0}, {140, 0, 180}]` |
| `CHRISTMAS` | `[{220, 30, 30}, {30, 160, 60}]` |

### Amber stale-sync

- Activation: `seconds_since_sync > 86'400` (strict >). `UINT32_MAX`
  (the "never synced this session" sentinel from
  `wifi_provision::seconds_since_last_sync()`) also satisfies this.
- Amber RGB: `{255, 120, 30}` (warm amber, ≈ 1800 K blackbody).
- Scope: amber replaces **warm white only**. On holiday days the
  palette wins; on birthday decor words the rainbow wins. This
  keeps amber from muddying celebration displays.
- Hysteresis: none. `seconds_since_sync` drops to 0 on a successful
  NTP, so the amber state exits on the very next render tick.

### Dim multiplier

Applied unconditionally as the last step per-LED. The renderer
derives an 8-bit multiplier from `wc::brightness(hour, minute)`:

```
const uint8_t bright_u8 = static_cast<uint8_t>(
    wc::brightness(hour, minute) * 255.0f + 0.5f);  // 26 or 255

r' = (r * bright_u8 + 127) / 255
g' = (g * bright_u8 + 127) / 255
b' = (b * bright_u8 + 127) / 255
```

`wc::brightness()` returns `0.1f` in the dim window (yielding
`bright_u8 = 26` — ≈ 10% of 255, with the half-step rounded up) and
`1.0f` otherwise (`bright_u8 = 255`). The per-channel formula is the
standard "multiply-by-8-bit, divide by 255, round-to-nearest" idiom:
`(x * a + 127) / 255`. Integer arithmetic keeps the render path
branch-light on ESP32, and the boundary behavior mirrors
`wc::brightness()` exactly: 18:59 is bright, 19:00 is dim, 07:59 is
dim, 08:00 is bright.

### Non-behaviors (explicit)

- **Stateless renderer.** `render()` is a pure function of its inputs.
  Rainbow animation emerges from `now_ms` changing, not from the
  renderer remembering the previous frame.
- **No transitions / fades** on word-set changes or day-boundary
  palette changes. The parent spec cadence is 5 minutes; hard-step
  matches QLOCKTWO tradition and Chelsea's clock.
- **No gamma correction** in v1. WS2812B + diffuser stack + dim
  multiplier already bend luminance; gamma is a bring-up tuning
  knob, not a spec requirement.
- **No color-temperature correction.** Face is hardwood; warm white
  on wood reads warm already.
- **No FastLED global brightness usage.** The renderer output is
  final. `show(frame)` memcpy's into FastLED's `CRGB[35]` and calls
  `FastLED.show()` — that's it.

## Architecture

Hexagonal. All color policy is pure logic, native-testable with Unity.
The ESP32 adapter is a thin FastLED wrapper.

```
firmware/lib/display/
  include/
    display.h                      public API (ESP32 only)
    display/
      rgb.h                        Rgb struct + Frame alias (header-only)
      led_map.h                    WordId → LED chain index (pure)
      palette.h                    Palette → Rgb lookup (pure)
      rainbow.h                    (WordId, now_ms) → Rgb (pure)
      renderer.h                   compose everything → Frame (pure)
  src/
    display.cpp                    FastLED adapter (#ifdef ARDUINO)
    led_map.cpp                    pure logic
    palette.cpp                    pure logic
    rainbow.cpp                    pure logic
    renderer.cpp                   pure logic
```

Tests:

```
firmware/test/
  test_display_led_map/test_led_map.cpp
  test_display_palette/test_palette.cpp
  test_display_rainbow/test_rainbow.cpp
  test_display_renderer/test_renderer.cpp
  test_display_renderer_golden/test_renderer_golden.cpp
```

Pure-logic `.cpp` files get added to `[env:native]`'s
`build_src_filter` in `platformio.ini`. `display.cpp` is guarded
with `#ifdef ARDUINO` — same reason wifi_provision's adapters are:
PlatformIO LDF's `deep+` mode pulls library sources into native
builds regardless of `build_src_filter`.

### LED chain map (authoritative, from `hardware/word-clock-all-pos.csv`)

The PCB wires LEDs roughly in spatial reading order (left→right,
top→bottom on the face), **not** in `WordId` enum order. The map
below is the firmware's single source of truth for the wiring:

| WordId | Enum idx | LED chain idx | PCB ref |
|---|---:|---:|---|
| IT | 0 | 0 | D1 |
| IS | 1 | 1 | D2 |
| TEN_MIN | 2 | 2 | D3 |
| HALF | 3 | 3 | D4 |
| QUARTER | 4 | 4 | D5 |
| TWENTY | 5 | 9 | D10 |
| FIVE_MIN | 6 | 5 | D6 |
| MINUTES | 7 | 6 | D7 |
| PAST | 8 | 8 | D9 |
| TO | 9 | 11 | D12 |
| ONE | 10 | 12 | D13 |
| TWO | 11 | 17 | D18 |
| THREE | 12 | 10 | D11 |
| FOUR | 13 | 15 | D16 |
| FIVE_HR | 14 | 24 | D25 |
| SIX | 15 | 21 | D22 |
| SEVEN | 16 | 19 | D20 |
| EIGHT | 17 | 18 | D19 |
| NINE | 18 | 20 | D21 |
| TEN_HR | 19 | 25 | D26 |
| ELEVEN | 20 | 14 | D15 |
| TWELVE | 21 | 22 | D23 |
| OCLOCK | 22 | 26 | D27 |
| NOON | 23 | 27 | D28 |
| IN | 24 | 28 | D29 |
| THE | 25 | 29 | D30 |
| AT | 26 | 34 | D35 |
| MORNING | 27 | 33 | D34 |
| AFTERNOON | 28 | 30 | D31 |
| EVENING | 29 | 31 | D32 |
| NIGHT | 30 | 32 | D33 |
| HAPPY | 31 | 7 | D8 |
| BIRTH | 32 | 13 | D14 |
| DAY | 33 | 16 | D17 |
| NAME | 34 | 23 | D24 |

The table is a permutation of `[0..34]` (verified in tests). Any
future PCB revision that reorders LEDs updates this table and the
unit test catches stale chain indices.

## Public API

### Header-only `rgb.h`

```cpp
// firmware/lib/display/include/display/rgb.h
#pragma once

#include <array>
#include <cstdint>

namespace wc::display {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr bool operator==(const Rgb& a, const Rgb& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline constexpr uint8_t LED_COUNT = 35;

using Frame = std::array<Rgb, LED_COUNT>;

} // namespace wc::display
```

### Pure-logic headers

```cpp
// firmware/lib/display/include/display/led_map.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc::display {

// LED chain index (0..34) for a given WordId. Source of truth:
// hardware/word-clock-all-pos.csv. Encoded as an explicit table so a
// future PCB revision that reorders LEDs is caught by the
// permutation unit test rather than silently miswiring the firmware.
uint8_t index_of(WordId w);

} // namespace wc::display
```

```cpp
// firmware/lib/display/include/display/palette.h
#pragma once

#include "display/rgb.h"
#include "holidays.h"
#include "word_id.h"

namespace wc::display {

// Returns the RGB color a given word should be under a given palette.
// Implementation MUST switch(p) over named Palette enumerators (not
// array-indexed lookup) so a future Palette enum addition triggers
// -Wswitch-enum at compile time. Within a palette: word gets
// palette_colors[static_cast<uint8_t>(w) % size()].
Rgb color_for(Palette p, WordId w);

// Palette PSU budget: every Rgb returned by color_for(...) must
// satisfy r + g + b <= PALETTE_MAX_RGB_SUM. Enforced by the
// test_palette_power_budget native test.
inline constexpr uint16_t PALETTE_MAX_RGB_SUM = 700;

// Convenience constants.
Rgb warm_white();   // {255, 170, 100}
Rgb amber();        // {255, 120, 30}

} // namespace wc::display
```

```cpp
// firmware/lib/display/include/display/rainbow.h
#pragma once

#include <cstdint>
#include "display/rgb.h"
#include "word_id.h"

namespace wc::display {

// Rainbow color for a decor word at monotonic time now_ms.
// Valid only for w ∈ {HAPPY, BIRTH, DAY, NAME}; caller guarantees.
// Returns full-saturation, full-value RGB (dim multiplier is the
// caller's responsibility — see renderer::render).
// Period: RAINBOW_PERIOD_MS = 60'000 per full 360° cycle.
// Phase offsets: HAPPY=0°, BIRTH=90°, DAY=180°, NAME=270°.
// now_ms is allowed to wrap; implementation MUST use
// (now_ms % RAINBOW_PERIOD_MS) * 360 / RAINBOW_PERIOD_MS to keep
// the intermediate product from overflowing uint32_t every 3.3 h.
Rgb rainbow(WordId w, uint32_t now_ms);

inline constexpr uint32_t RAINBOW_PERIOD_MS = 60'000;

} // namespace wc::display
```

```cpp
// firmware/lib/display/include/display/renderer.h
#pragma once

#include <cstdint>

#include "birthday.h"
#include "display/rgb.h"

namespace wc::display {

struct RenderInput {
    // Date + wall-clock time for palette, birthday, dim window, and
    // time_to_words. Caller guarantees ranges:
    //   month  ∈ [1, 12]
    //   day    ∈ [1, 31]
    //   hour   ∈ [0, 23]
    //   minute ∈ [0, 59]
    //   year   ≥ 2023 (the earliest plausible wall-clock for either
    //                  daughter's lifetime; pre-2023 is treated as
    //                  "clock never synced" territory — still renders
    //                  cleanly but behavior is unspecified).
    // The renderer does NOT re-validate; bad inputs trigger undefined
    // visual output but never crash. Bounds-checking is the `rtc`
    // module's responsibility — it's the only legitimate source of
    // these fields at runtime.
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;

    // Monotonic millis for rainbow phase. Wraps at ~49 days; rainbow
    // math works correctly across the wrap.
    uint32_t now_ms;

    // From wifi_provision::seconds_since_last_sync(). UINT32_MAX
    // means "never synced this session"; treated as stale (>24 h).
    uint32_t seconds_since_sync;

    // Per-kid birthday, sourced from config_{emory,nora}.h in
    // main.cpp before calling render().
    BirthdayConfig birthday;
};

// Stale-sync threshold. Above this value, amber replaces warm white
// on lit time words (holidays and birthday decor still win).
inline constexpr uint32_t STALE_SYNC_THRESHOLD_S = 86'400;  // 24 h

// Pure function. Given the inputs, returns the final per-LED Frame
// ready to push to the strip (dim multiplier already applied).
Frame render(const RenderInput& in);

} // namespace wc::display
```

### ESP32-only public API

```cpp
// firmware/lib/display/include/display.h
//
// ESP32-only public API. Depends on FastLED + Arduino.h. Include
// ONLY from translation units that compile under the Arduino
// toolchain (the emory / nora PlatformIO envs). Native tests
// include the pure-logic headers under display/ instead — never
// this header.
#pragma once

#include <Arduino.h>
#include "display/rgb.h"

namespace wc::display {

// Initialize FastLED: addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, 35).
// Call once from setup(). Idempotent.
void begin();

// Push a rendered Frame to the LED strip. Copies frame into the
// FastLED CRGB buffer and calls FastLED.show(). The adapter owns
// no color policy — it pushes exactly what renderer::render
// returned.
void show(const Frame& frame);

} // namespace wc::display
```

## Adapter (`display.cpp`) — pseudocode

Shape for review; actual code written during implementation.

```cpp
#ifdef ARDUINO

#include <Arduino.h>
#include <FastLED.h>
#include "display.h"
#include "display/rgb.h"
#include "pinmap.h"    // PIN_LED_DATA

namespace wc::display {

static CRGB leds[LED_COUNT];
static bool started = false;

void begin() {
    if (started) return;
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, LED_COUNT);
    // Runtime layer-2 defense on the palette power budget. If a
    // palette tune ever emits a frame that would pull more than
    // 1.8 A, FastLED scales brightness down automatically.
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1800);
    FastLED.clear();
    FastLED.show();
    started = true;
}

void show(const Frame& frame) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        leds[i] = CRGB(frame[i].r, frame[i].g, frame[i].b);
    }
    FastLED.show();
}

} // namespace wc::display

#endif // ARDUINO
```

No color logic in the adapter. All policy lives in `renderer.cpp`.

## Integration

### Pin + level-shifter

- Data line: `PIN_LED_DATA` (GPIO 13), already defined in
  `firmware/configs/pinmap.h` (shipped with the buttons module).
- Hardware path: ESP32 GPIO 13 → 74HCT245 buffer (3.3 V→5 V logic
  lift) → 300 Ω inline resistor → WS2812B `DIN`. All three elements
  are on the PCB; firmware assumes the level shifter is present
  (per `docs/hardware/pinout.md` §Critical issues #2).
- Color order: `GRB` — the WS2812B datasheet default. Confirmed
  during breadboard bring-up against `FastLED.addLeds<WS2812B, GPIO, GRB>`
  by flashing a red-test sketch (see
  `docs/hardware/breadboard-bring-up-guide.md` §Step 9).

### Dependency on `wifi_provision`

The renderer consumes `seconds_since_sync` from
`wifi_provision::seconds_since_last_sync()` (header already declared
in `firmware/lib/wifi_provision/include/wifi_provision.h`). Display
does not call the wifi_provision API directly — `main.cpp` reads
the value once per loop tick and passes it into `RenderInput`,
which keeps the renderer pure.

### Dependency on `rtc` (future module)

Display consumes year/month/day/hour/minute from whatever the `rtc`
module exposes. Until `rtc` ships, `main.cpp` can call the DS3231
directly via RTClib for integration on the breadboard — this spec
doesn't require the `rtc` module to exist first.

### main.cpp routing (illustrative only — executed by the future
main.cpp integration spec, not this one)

```cpp
#include "display.h"
#include "display/renderer.h"
#include "wifi_provision.h"
// #include "rtc.h" (future Phase 2 module)

void setup() {
    // ... wifi_provision::begin(), buttons::begin() ...
    wc::display::begin();
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();

    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        // const auto dt = wc::rtc::now();          // future module
        wc::display::RenderInput in{
            .year   = dt.year,
            .month  = dt.month,
            .day    = dt.day,
            .hour   = dt.hour,
            .minute = dt.minute,
            .now_ms = millis(),
            .seconds_since_sync = wc::wifi_provision::seconds_since_last_sync(),
            .birthday = {CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                         CLOCK_BIRTH_HOUR, CLOCK_BIRTH_MINUTE},
        };
        wc::display::show(wc::display::render(in));
    } else {
        wc::display::show(wc::display::Frame{});  // all-dark
    }
    delay(1);
}
```

## Testing

### Pure-logic native tests (Unity)

**`test_display_led_map`** — 4 tests
- `test_index_of_first_word_is_zero`: `index_of(WordId::IT) == 0`
- `test_index_of_name_is_23`: `index_of(WordId::NAME) == 23`
  (verifies the non-identity mapping specifically)
- `test_index_of_at_is_34`: `index_of(WordId::AT) == 34` (verifies
  the last LED in the chain)
- `test_all_word_ids_map_to_unique_led_indices`: iterate
  `0..WordId::COUNT-1`, collect `index_of(WordId(i))`, assert the
  result is a permutation of `0..34`

**`test_display_palette`** — 9 tests
- `test_warm_white_rgb`: `warm_white() == Rgb{255, 170, 100}`
- `test_amber_rgb`: `amber() == Rgb{255, 120, 30}`
- `test_palette_warm_white_is_warm_white_for_all_words`
- `test_palette_halloween_alternates_orange_purple_by_enum_idx`:
  `color_for(HALLOWEEN, WordId::IT)` (enum 0) → orange;
  `color_for(HALLOWEEN, WordId::IS)` (enum 1) → purple
- `test_palette_christmas_alternates_red_green`
- `test_palette_valentines_alternates_red_pink`
- `test_palette_juneteenth_cycles_three_colors`
- `test_palette_easter_cycles_four_colors`
- `test_palette_power_budget`: iterate every `Palette` enum value;
  for each, iterate every `WordId`; assert
  `color_for(p, w).r + color_for(p, w).g + color_for(p, w).b
  <= PALETTE_MAX_RGB_SUM` (= 700). Catches PSU-hostile palette tunes
  at build time.

**`test_display_rainbow`** — 7 tests
- `test_rainbow_happy_at_t0_is_red`: `rainbow(HAPPY, 0) ≈ {255, 0, 0}`
  (R ≥ 250, G ≤ 5, B ≤ 5 — hue 0° of the standard HSV→RGB curve)
- `test_rainbow_birth_at_t0_is_yellow_green`:
  `rainbow(BIRTH, 0) ≈ {127, 255, 0}` (G ≥ 250, B ≤ 5,
  R ∈ [120, 135] — hue 90° landing in the red→green ramp)
- `test_rainbow_day_at_t0_is_cyan`:
  `rainbow(DAY, 0) ≈ {0, 255, 255}` (R ≤ 5, G ≥ 250, B ≥ 250 —
  hue 180°)
- `test_rainbow_name_at_t0_is_purple`:
  `rainbow(NAME, 0) ≈ {127, 0, 255}` (B ≥ 250, G ≤ 5,
  R ∈ [120, 135] — hue 270°)
- `test_rainbow_period_is_60s`: `rainbow(HAPPY, 60'000) ==
  rainbow(HAPPY, 0)` per-channel (within 1 LSB for integer rounding)
- `test_rainbow_wraps_across_uint32_max`:
  `rainbow(HAPPY, UINT32_MAX - 1'000)` vs `rainbow(HAPPY, 1'000)`
  correspond to hue positions ~2 s of phase apart (≈ 12° hue
  shift); assert both outputs exist and differ per-channel by at
  least 1 LSB — the point is that modular arithmetic doesn't
  spuriously restart the cycle at the wrap boundary
- `test_rainbow_no_overflow_near_uint32_div_360`: pick
  `now_ms = UINT32_MAX / 360 = 11'930'464` (the boundary where a
  naive `now_ms * 360` would overflow `uint32_t`). Assert
  `rainbow(HAPPY, 11'930'463)` and `rainbow(HAPPY, 11'930'465)`
  differ per-channel by no more than a few LSB — no visible hue
  jump. Fails loudly if someone "simplifies" the rainbow formula
  back to the overflow-prone form.

**`test_display_renderer`** — 13 tests (the behavioral contract
end-to-end)
- `test_unlit_word_is_black`: at 6:20 PM non-birthday non-holiday
  fresh-sync, `NOON` index in Frame is `{0, 0, 0}`
- `test_warm_white_default`: 2030-01-15 14:00,
  `seconds_since_sync=0` → every lit time-word LED is
  `{255, 170, 100}`
- `test_holiday_palette_replaces_warm_white`: 2030-10-31 14:00 →
  lit time words are the Halloween palette pattern
- `test_birthday_rainbow_on_decor_only`: Emory birthday 2030-10-06
  14:00 non-holiday → HAPPY/BIRTH/DAY/NAME show rainbow values,
  time words are warm white
- `test_birthday_rainbow_shifts_with_now_ms`: same inputs with
  `now_ms=0` vs `now_ms=15'000` — decor LEDs differ, time-word LEDs
  identical
- `test_birthday_rainbow_wins_over_holiday_palette`: construct a
  `BirthdayConfig` that coincides with Halloween → decor rainbow,
  time words show Halloween palette (both signals coexist per
  priority order)
- `test_stale_sync_replaces_warm_white_on_time_words`:
  non-birthday non-holiday `seconds_since_sync=90'000` → lit time
  words are amber
- `test_stale_sync_boundary`: `seconds_since_sync=86'400` is NOT
  stale (warm white); `seconds_since_sync=86'401` IS stale (amber)
- `test_stale_sync_does_not_override_holiday`: Halloween +
  `seconds_since_sync=90'000` → orange/purple pattern, no amber
- `test_stale_sync_does_not_override_birthday_decor`: birthday +
  stale → decor rainbow preserved, time words amber
- `test_dim_multiplier_warm_white`: 20:00 on a warm-white day →
  lit time words are `{26, 17, 10}` (10% of `{255, 170, 100}`
  with round-to-nearest)
- `test_dim_multiplier_applies_to_rainbow`: birthday 20:00 → decor
  values are the pre-dim rainbow values scaled by 26/255
- `test_dim_boundary_matches_dim_schedule`: 18:59 is full-bright,
  19:00 is dim, 07:59 is dim, 08:00 is full-bright (mirrors
  `wc::brightness()` boundaries exactly)

**`test_display_renderer_golden`** — 2 tests
- `test_golden_birthday_non_holiday_bright`: Emory, 2030-10-06
  14:23, `seconds_since_sync=0`, `now_ms=0`. Asserts per-LED
  *categories* (not exact RGB), since exact-byte assertions rot the
  moment a palette value is tuned during bring-up:
  - LEDs for lit time words (`IT, IS, TWENTY, MINUTES, PAST, FIVE_HR,
    AT, AFTERNOON`) → warm-white family: `R == 255 && G == 170
    && B == 100` (warm white is a single exact tuple and not
    tuning-target; asserting it exactly is fine)
  - LEDs for decor words (`HAPPY, BIRTH, DAY, NAME`) → rainbow
    family: each satisfies saturated-hue invariants (min channel
    ≤ 5, max channel ≥ 250) and matches its phase-offset hue
    sector (HAPPY red, BIRTH yellow-green, DAY cyan, NAME purple)
  - All other LEDs → `{0, 0, 0}`
- `test_multi_signal_transition_is_atomic`: single `render()` call
  at 2030-11-01 00:00 with a synthetic `BirthdayConfig{11, 1, ...}`
  so three signals flip on the same tick (day change to non-Halloween
  + birthday start + dim window active). Asserts the frame is
  internally consistent with the priority table — no LED is "mid-
  transition" between two states. Guards against someone adding
  stateful caching to the renderer in the future.

Category-based golden assertions catch the two real drift risks
(palette entry reorder, precedence bug) while surviving legitimate
palette tuning without fake-positive test failures. The palette's
exact RGB values are still asserted byte-for-byte in
`test_display_palette` — that's the single byte-level source of
truth. Tuning a palette there → palette test fails → developer
updates the palette table and re-runs; the renderer golden does
not require re-coordination.

Total display pure-logic tests: 4 (led_map) + 9 (palette) + 7
(rainbow) + 13 (renderer) + 2 (golden) = **35 display tests**.

Target native-suite count after this module: 79 (pre-display) + 35
(display) = **114 native tests**.

### ESP32-only manual hardware check

`firmware/test/hardware_checks/display_checks.md` — executed once
the breadboard has the full WS2812B chain + 74HCT245 level
shifter (per `breadboard-bring-up-guide.md` §Step 9). Not
automatable — requires eyes on a glowing face.

1. Flash `emory` env, open serial at 115200. Connect phone to
   `WordClock-Setup-XXXX` and enter WiFi; wait for `Online` state.
2. Face shows the current time in warm white. Word boundaries
   clean (no bleed between adjacent pockets — catches light-channel
   or color-order bugs).
3. Force day to 2030-10-06 (Emory birthday) via a temporary build
   flag or RTClib set call. Observe: HAPPY/BIRTH/DAY/NAME cycle
   hues; full 360° rotation in ≈ 60 s; the four words chase with
   visible 90° offset (HAPPY leading, NAME trailing).
4. Force day to 2030-10-31. Observe: time words show an
   orange+purple alternation pattern; no rainbow.
5. Temporary-build-flag the stale-sync threshold to 60 s.
   Disconnect WiFi for 90 s and observe: lit time words shift to
   amber; holiday/birthday cases (re-run 3 and 4 with stale) are
   unaffected.
6. Wait until 19:00 (or temporarily shift the dim window). Observe:
   face dims to ≈ 10% brightness across the board. Rainbow on
   birthday still smoothly cycles, just at 10%.
7. Power-cycle during the dim window. Confirm the face comes up
   dim immediately — no full-bright flash-and-settle.

## Open issues

None. All 4 brainstorming decisions locked. Palette RGB values are
declared as starting defaults; they will be retuned on real
hardware during bring-up without changing the spec's behavior.

## Verified numbers

| Item | Value | Source |
|---|---|---|
| LED count | 35 | `hardware/word-clock-all-pos.csv` (35 D-refs D1–D35) |
| LED chain order | spatial reading order | `hardware/word-clock-all-pos.csv` (CPL table above) |
| LED data GPIO | 13 | `docs/hardware/pinout.md` §Signal assignments + `firmware/configs/pinmap.h` |
| Level shifter required | 74HCT245, 3.3 V→5 V | `docs/hardware/pinout.md` §Critical issues #2 |
| Inline series resistor | 300 Ω | `docs/hardware/pinout.md` §Power + §Critical issues |
| Bulk capacitor | 1000 µF electrolytic | `docs/hardware/pinout.md` §Power |
| WS2812B color order | GRB | WS2812B datasheet default; confirmed by the bring-up rainbow sketch in `docs/hardware/breadboard-bring-up-guide.md` §Step 9 |
| Rainbow period | 60 000 ms | Brainstorm Decision 3b (gentle cadence matching Chelsea's clock lineage) |
| Rainbow phase offsets | 0° / 90° / 180° / 270° | Brainstorm Decision 3b (4 decor words evenly distributed) |
| Stale-sync threshold | >86 400 s (24 h) | `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §Time sync |
| Warm-white RGB | {255, 170, 100} | Brainstorm Decision 4a starting point (≈ 2700 K on WS2812B + diffuser) |
| Amber RGB | {255, 120, 30} | Brainstorm Decision 4a (≈ 1800 K, distinct from warm white, not alarming) |
| Dim-window brightness | ≈ 10% (26/255 8-bit) | `firmware/lib/core/src/dim_schedule.cpp` |
| Dim-window boundaries | 19:00–07:59 dim; 08:00–18:59 full | `firmware/lib/core/src/dim_schedule.cpp` + `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §Bedtime dim |
| Birthday activation window | full day (midnight-to-midnight local) | `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §Birthday mode + `firmware/lib/core/src/birthday.cpp` |
| WordId count | 35 | `firmware/lib/core/include/word_id.h` (`WordId::COUNT`) |
| Palette power budget | `r + g + b ≤ 700` per entry | Derived: `35 LEDs × (700/765) × 60 mA = 1.92 A` worst case; `docs/hardware/pinout.md` §Power budget allocates up to ~2.1 A for LED strip alone |
| Runtime strip-current cap | 1.8 A via `FastLED.setMaxPowerInVoltsAndMilliamps(5, 1800)` | `docs/hardware/pinout.md` §Power (LED subsystem budget) |
| Rainbow overflow guard | `(now_ms % RAINBOW_PERIOD_MS) * 360 / RAINBOW_PERIOD_MS` | Keeps product under `60'000 × 360 = 21'600'000` well within `uint32_t`; naive form overflows at `UINT32_MAX / 360 ≈ 11.93 Ms ≈ 3.3 h` |

## References

- Parent design: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
  (§Firmware Behaviors, §Bedtime dim, §Birthday mode, §Holiday
  modes, §Time sync)
- Hardware pinout: `docs/hardware/pinout.md`
- PCB LED placements: `hardware/word-clock-all-pos.csv`
- Phase 1 dependencies: `firmware/lib/core/include/{time_to_words,holidays,birthday,dim_schedule,word_id,grid}.h`
- Shared pin macros: `firmware/configs/pinmap.h`
- Sibling modules (template pattern + hexagonal boundary examples):
  - `docs/superpowers/specs/2026-04-16-buttons-design.md`
  - `docs/superpowers/specs/2026-04-16-captive-portal-design.md`
  - `firmware/lib/buttons/`, `firmware/lib/wifi_provision/`
- Breadboard bring-up (manual hardware-check prerequisite):
  `docs/hardware/breadboard-bring-up-guide.md` §Step 9
- Laser-cut face spec (LED-to-word geometry + filler-isolation
  constraints that flowed in from hardware): `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`
