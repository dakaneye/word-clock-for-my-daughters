# Display Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `display` firmware module — pure-logic renderer that
composes the Phase 1 modules (`time_to_words`, `holidays`, `birthday`,
`dim_schedule`) into a per-LED RGB frame, plus a FastLED adapter that
pushes the frame to the WS2812B chain. Covers holiday palettes, a 60 s
birthday rainbow on the four decor words, amber stale-sync tint, and
the bedtime dim multiplier — in that priority order.

**Architecture:** Hexagonal. Five pure-logic modules (rgb, led_map,
palette, rainbow, renderer) are native-testable with Unity. ESP32
adapter (`display.cpp`) wraps `FastLED.addLeds`, `FastLED.show()`, and
`FastLED.setMaxPowerInVoltsAndMilliamps`. Same `#ifdef ARDUINO` guard
pattern as `wifi_provision` and `buttons`. Renderer is stateless —
rainbow animation emerges from `now_ms` changing, not from frame
history.

**Tech Stack:** Arduino-ESP32 core, FastLED 3.7, PlatformIO build
system, Unity C++ test framework for native tests.

---

## Spec reference

This plan implements
`docs/superpowers/specs/2026-04-17-display-design.md`. Read the spec
in full before starting — it pins every behavioral decision (priority
order, rainbow math, palette power budget, dim math, LED chain
mapping).

## File structure

```
firmware/
  lib/display/
    include/
      display.h                      # ESP32-only public API
      display/
        rgb.h                        # Rgb struct + Frame alias (header-only)
        led_map.h                    # WordId → LED chain index
        palette.h                    # Palette → Rgb lookup + constants
        rainbow.h                    # (WordId, now_ms) → Rgb
        renderer.h                   # RenderInput + render() entry
        detail/
          hsv.h                      # Pure HSV→RGB helper (header-only)
    src/
      display.cpp                    # FastLED adapter (#ifdef ARDUINO)
      led_map.cpp                    # Pure logic
      palette.cpp                    # Pure logic
      rainbow.cpp                    # Pure logic (uses detail/hsv.h)
      renderer.cpp                  # Pure logic (composes everything)
  test/
    test_display_led_map/test_led_map.cpp
    test_display_palette/test_palette.cpp
    test_display_rainbow/test_rainbow.cpp
    test_display_renderer/test_renderer.cpp
    test_display_renderer_golden/test_renderer_golden.cpp
    hardware_checks/display_checks.md
  platformio.ini                     # MODIFIED — native env + ESP32 LDF picks up automatically
  src/main.cpp                       # MODIFIED — call display::begin + render (bring-up stub with hardcoded time)
```

The pure-logic `.cpp` files (led_map, palette, rainbow, renderer) are
added to `[env:native]`'s `build_src_filter`. `display.cpp` is guarded
with `#ifdef ARDUINO` so PlatformIO's `deep+` LDF pulling the file
into native doesn't break the build — same workaround the buttons and
wifi_provision plans used.

---

## Task 1: Library scaffold + Rgb header + platformio wiring

**Files:**
- Create: `firmware/lib/display/include/display/rgb.h`
- Modify: `firmware/platformio.ini`

- [ ] **Step 1: Create the Rgb header**

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

constexpr bool operator!=(const Rgb& a, const Rgb& b) {
    return !(a == b);
}

inline constexpr uint8_t LED_COUNT = 35;

using Frame = std::array<Rgb, LED_COUNT>;

} // namespace wc::display
```

- [ ] **Step 2: Wire the display include path into [env:native]**

Edit `firmware/platformio.ini`. Current `[env:native]` section:

```ini
[env:native]
platform = native
test_framework = unity
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
    -I lib/buttons/include
build_src_filter =
    +<../lib/core/src/*>
    +<../lib/wifi_provision/src/state_machine.cpp>
    +<../lib/wifi_provision/src/form_parser.cpp>
    +<../lib/wifi_provision/src/credential_validator.cpp>
    +<../lib/wifi_provision/src/tz_options.cpp>
    +<../lib/buttons/src/debouncer.cpp>
    +<../lib/buttons/src/combo_detector.cpp>
lib_compat_mode = off
```

Replace with:

```ini
[env:native]
platform = native
test_framework = unity
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
    -I lib/buttons/include
    -I lib/display/include
build_src_filter =
    +<../lib/core/src/*>
    +<../lib/wifi_provision/src/state_machine.cpp>
    +<../lib/wifi_provision/src/form_parser.cpp>
    +<../lib/wifi_provision/src/credential_validator.cpp>
    +<../lib/wifi_provision/src/tz_options.cpp>
    +<../lib/buttons/src/debouncer.cpp>
    +<../lib/buttons/src/combo_detector.cpp>
    +<../lib/display/src/led_map.cpp>
    +<../lib/display/src/palette.cpp>
    +<../lib/display/src/rainbow.cpp>
    +<../lib/display/src/renderer.cpp>
lib_compat_mode = off
```

The four `display/src/*.cpp` entries reference files that don't exist
yet — PlatformIO emits a warning but doesn't fail. Same pattern the
buttons plan used for `combo_detector.cpp` before it was created.

- [ ] **Step 3: Verify ESP32 build still succeeds**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: build succeeds — nothing in the emory build references
display yet, so flash usage is unchanged.

- [ ] **Step 4: Verify native suite still passes**

Run: `cd firmware && pio test -e native`
Expected: 79/79 green. Native can't find the four referenced display
.cpp files yet; PlatformIO warns but the build links because no test
file pulls them in.

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/display/include/display/rgb.h firmware/platformio.ini
git commit -m "feat(firmware): scaffold display library + Rgb/Frame header"
```

---

## Task 2: led_map (scaffold + full coverage)

**Files:**
- Create: `firmware/lib/display/include/display/led_map.h`
- Create: `firmware/lib/display/src/led_map.cpp`
- Create: `firmware/test/test_display_led_map/test_led_map.cpp`

- [ ] **Step 1: Write the failing test file**

```cpp
// firmware/test/test_display_led_map/test_led_map.cpp
#include <unity.h>
#include <set>
#include "display/led_map.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_index_of_first_word_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, index_of(WordId::IT));
}

void test_index_of_name_is_23(void) {
    // NAME (enum 34) lands at LED chain index 23 — the key "PCB is
    // not enum order" assertion. See led-map table in the spec.
    TEST_ASSERT_EQUAL_UINT8(23, index_of(WordId::NAME));
}

void test_index_of_at_is_34(void) {
    // AT (enum 26) is D35, the last LED in the chain.
    TEST_ASSERT_EQUAL_UINT8(34, index_of(WordId::AT));
}

void test_all_word_ids_map_to_unique_led_indices(void) {
    std::set<uint8_t> seen;
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const uint8_t idx = index_of(static_cast<WordId>(i));
        TEST_ASSERT_LESS_THAN_UINT8(LED_COUNT, idx);
        TEST_ASSERT_TRUE_MESSAGE(seen.insert(idx).second,
                                 "index_of returned a duplicate LED index");
    }
    TEST_ASSERT_EQUAL_UINT32(LED_COUNT, seen.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_index_of_first_word_is_zero);
    RUN_TEST(test_index_of_name_is_23);
    RUN_TEST(test_index_of_at_is_34);
    RUN_TEST(test_all_word_ids_map_to_unique_led_indices);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_display_led_map`
Expected: compile FAIL with "fatal error: display/led_map.h: No such
file or directory".

- [ ] **Step 3: Create the header**

```cpp
// firmware/lib/display/include/display/led_map.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc::display {

// Returns the LED chain index (0..34) for a given WordId.
// Source of truth: hardware/word-clock-all-pos.csv. The PCB wires
// LEDs in spatial reading order (left→right, top→bottom), not in
// WordId enum order — this is an explicit permutation, encoded as a
// table so a future PCB LED reordering is caught by the permutation
// unit test rather than silently miswiring the firmware.
uint8_t index_of(WordId w);

} // namespace wc::display
```

- [ ] **Step 4: Create the implementation**

```cpp
// firmware/lib/display/src/led_map.cpp
#include "display/led_map.h"

#include <array>

namespace wc::display {

namespace {

// WordId enum value → LED chain index. Row i is for WordId(i).
// Derived from hardware/word-clock-all-pos.csv D-number minus 1.
constexpr std::array<uint8_t, 35> kMap = {
    /* IT        */  0,
    /* IS        */  1,
    /* TEN_MIN   */  2,
    /* HALF      */  3,
    /* QUARTER   */  4,
    /* TWENTY    */  9,
    /* FIVE_MIN  */  5,
    /* MINUTES   */  6,
    /* PAST      */  8,
    /* TO        */ 11,
    /* ONE       */ 12,
    /* TWO       */ 17,
    /* THREE     */ 10,
    /* FOUR      */ 15,
    /* FIVE_HR   */ 24,
    /* SIX       */ 21,
    /* SEVEN     */ 19,
    /* EIGHT     */ 18,
    /* NINE      */ 20,
    /* TEN_HR    */ 25,
    /* ELEVEN    */ 14,
    /* TWELVE    */ 22,
    /* OCLOCK    */ 26,
    /* NOON      */ 27,
    /* IN        */ 28,
    /* THE       */ 29,
    /* AT        */ 34,
    /* MORNING   */ 33,
    /* AFTERNOON */ 30,
    /* EVENING   */ 31,
    /* NIGHT     */ 32,
    /* HAPPY     */  7,
    /* BIRTH     */ 13,
    /* DAY       */ 16,
    /* NAME      */ 23,
};

} // namespace

uint8_t index_of(WordId w) {
    return kMap[static_cast<uint8_t>(w)];
}

} // namespace wc::display
```

- [ ] **Step 5: Run tests — expect all 4 to pass**

Run: `cd firmware && pio test -e native -f test_display_led_map`
Expected: 4 tests pass.

- [ ] **Step 6: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 83/83 pass (79 existing + 4 led_map).

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/display/include/display/led_map.h \
        firmware/lib/display/src/led_map.cpp \
        firmware/test/test_display_led_map/
git commit -m "feat(firmware): display led_map — WordId to PCB chain index"
```

---

## Task 3: HSV helper + Rainbow scaffold + first test

**Files:**
- Create: `firmware/lib/display/include/display/detail/hsv.h`
- Create: `firmware/lib/display/include/display/rainbow.h`
- Create: `firmware/lib/display/src/rainbow.cpp`
- Create: `firmware/test/test_display_rainbow/test_rainbow.cpp`

- [ ] **Step 1: Write the failing first test**

```cpp
// firmware/test/test_display_rainbow/test_rainbow.cpp
#include <unity.h>
#include "display/rainbow.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

// HAPPY at t=0 has 0° phase offset → should be red-dominant.
void test_rainbow_happy_at_t0_is_red(void) {
    const Rgb c = rainbow(WordId::HAPPY, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.r);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rainbow_happy_at_t0_is_red);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_display_rainbow`
Expected: compile FAIL with "fatal error: display/rainbow.h: No such
file or directory".

- [ ] **Step 3: Create the HSV helper (header-only)**

```cpp
// firmware/lib/display/include/display/detail/hsv.h
#pragma once

#include <cstdint>
#include "display/rgb.h"

namespace wc::display::detail {

// Standard 6-sector HSV→RGB at full saturation (S implicitly = 255).
// hue_deg ∈ [0, 360). v ∈ [0, 255] scales output.
// Integer math with truncating division — at sector midpoints
// (hue = 30°, 90°, 150°, 210°, 270°, 330°) the midpoint channel
// lands at 127 or 128 depending on whether it's the ascending or
// descending component. Tests tolerate ±7 LSB to absorb this and
// future tuning.
inline Rgb hsv_to_rgb_full_sat(uint16_t hue_deg, uint8_t v) {
    const uint8_t sector = static_cast<uint8_t>(hue_deg / 60U);        // 0..5
    const uint16_t f_num = hue_deg - static_cast<uint16_t>(sector) * 60U; // 0..59
    const uint8_t up = static_cast<uint8_t>(
        (static_cast<uint32_t>(v) * f_num) / 60U);
    const uint8_t dn = static_cast<uint8_t>(v - up);
    switch (sector) {
        case 0: return Rgb{v,  up, 0 };  // red → yellow
        case 1: return Rgb{dn, v,  0 };  // yellow → green
        case 2: return Rgb{0,  v,  up};  // green → cyan
        case 3: return Rgb{0,  dn, v };  // cyan → blue
        case 4: return Rgb{up, 0,  v };  // blue → magenta
        case 5: return Rgb{v,  0,  dn};  // magenta → red
        default: return Rgb{0, 0, 0};    // unreachable for hue_deg < 360
    }
}

} // namespace wc::display::detail
```

- [ ] **Step 4: Create the rainbow header**

```cpp
// firmware/lib/display/include/display/rainbow.h
#pragma once

#include <cstdint>

#include "display/rgb.h"
#include "word_id.h"

namespace wc::display {

// Full rainbow cycle period.
inline constexpr uint32_t RAINBOW_PERIOD_MS = 60'000;

// Rainbow color for a decor word at monotonic time now_ms.
// Valid only for w ∈ {HAPPY, BIRTH, DAY, NAME}; caller guarantees.
// Returns full-saturation, full-value RGB (dim multiplier is the
// caller's responsibility — see renderer::render).
// Period: RAINBOW_PERIOD_MS per full 360° cycle.
// Phase offsets: HAPPY=0°, BIRTH=90°, DAY=180°, NAME=270°.
// Implementation MUST use
// (now_ms % RAINBOW_PERIOD_MS) * 360 / RAINBOW_PERIOD_MS to keep the
// intermediate product from overflowing uint32_t every ≈3.3 h.
Rgb rainbow(WordId w, uint32_t now_ms);

} // namespace wc::display
```

- [ ] **Step 5: Create the rainbow implementation**

```cpp
// firmware/lib/display/src/rainbow.cpp
#include "display/rainbow.h"

#include "display/detail/hsv.h"

namespace wc::display {

namespace {

uint16_t phase_offset_deg(WordId w) {
    switch (w) {
        case WordId::HAPPY: return 0;
        case WordId::BIRTH: return 90;
        case WordId::DAY:   return 180;
        case WordId::NAME:  return 270;
        default:            return 0;  // caller contract prohibits
    }
}

} // namespace

Rgb rainbow(WordId w, uint32_t now_ms) {
    // Mod FIRST to keep the multiply under uint32_t overflow
    // (60'000 × 360 = 21'600'000 ≪ UINT32_MAX). Without the mod,
    // now_ms × 360 overflows every UINT32_MAX/360 ≈ 3.3 h.
    const uint32_t phase_frac =
        (now_ms % RAINBOW_PERIOD_MS) * 360U / RAINBOW_PERIOD_MS;
    const uint16_t hue = static_cast<uint16_t>(
        (phase_frac + phase_offset_deg(w)) % 360U);
    return detail::hsv_to_rgb_full_sat(hue, 255);
}

} // namespace wc::display
```

- [ ] **Step 6: Run test to confirm it passes**

Run: `cd firmware && pio test -e native -f test_display_rainbow`
Expected: 1 test passes.

- [ ] **Step 7: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 84/84 pass.

- [ ] **Step 8: Commit**

```bash
git add firmware/lib/display/include/display/detail/ \
        firmware/lib/display/include/display/rainbow.h \
        firmware/lib/display/src/rainbow.cpp \
        firmware/test/test_display_rainbow/
git commit -m "feat(firmware): display rainbow hue generator + HSV helper"
```

---

## Task 4: Rainbow full coverage

**Files:**
- Modify: `firmware/test/test_display_rainbow/test_rainbow.cpp`

All 7 tests (1 existing + 6 new) must pass. If any fail against the
Task 3 implementation, the impl has a bug — fix it in `rainbow.cpp`
rather than relaxing the test.

- [ ] **Step 1: Replace the test file with full coverage**

```cpp
// firmware/test/test_display_rainbow/test_rainbow.cpp
#include <unity.h>
#include <cstdint>
#include <cstdlib>
#include "display/rainbow.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

// HAPPY at t=0 has 0° phase offset → red.
void test_rainbow_happy_at_t0_is_red(void) {
    const Rgb c = rainbow(WordId::HAPPY, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.r);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
}

// BIRTH at t=0 has 90° phase offset → yellow-green
// (sector 1 midpoint; G=255, B≈0, R near-half).
void test_rainbow_birth_at_t0_is_yellow_green(void) {
    const Rgb c = rainbow(WordId::BIRTH, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
    TEST_ASSERT_TRUE_MESSAGE(c.r >= 120 && c.r <= 135,
                             "R should be near the 127.5 midpoint");
}

// DAY at t=0 has 180° phase offset → cyan.
void test_rainbow_day_at_t0_is_cyan(void) {
    const Rgb c = rainbow(WordId::DAY, 0);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.r);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.g);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.b);
}

// NAME at t=0 has 270° phase offset → purple
// (sector 4 midpoint; B=255, G≈0, R near-half).
void test_rainbow_name_at_t0_is_purple(void) {
    const Rgb c = rainbow(WordId::NAME, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.b);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_TRUE_MESSAGE(c.r >= 120 && c.r <= 135,
                             "R should be near the 127.5 midpoint");
}

// A full 60 s cycle returns to the same color.
void test_rainbow_period_is_60s(void) {
    const Rgb a = rainbow(WordId::HAPPY, 0);
    const Rgb b = rainbow(WordId::HAPPY, 60'000);
    // Exact equality; period wrap is modular-exact when now_ms hits
    // a whole multiple of RAINBOW_PERIOD_MS.
    TEST_ASSERT_EQUAL_UINT8(a.r, b.r);
    TEST_ASSERT_EQUAL_UINT8(a.g, b.g);
    TEST_ASSERT_EQUAL_UINT8(a.b, b.b);
}

// Across the uint32_t wrap boundary, colors stay in-cycle (no
// cycle-restart glitch).
void test_rainbow_wraps_across_uint32_max(void) {
    const Rgb before_wrap = rainbow(WordId::HAPPY, UINT32_MAX - 1'000);
    const Rgb after_wrap  = rainbow(WordId::HAPPY, 1'000);
    // They should be ≈2 s of phase apart (UINT32_MAX % 60'000 is
    // not a round number, so there's a phase offset between
    // t=UINT32_MAX-1000 and t=1000). Assert the outputs differ —
    // the assertion failure case we're guarding against is the
    // naive multiply silently wrapping and producing unrelated
    // colors with no modular relationship.
    const bool differs = (before_wrap.r != after_wrap.r) ||
                         (before_wrap.g != after_wrap.g) ||
                         (before_wrap.b != after_wrap.b);
    TEST_ASSERT_TRUE(differs);
}

// At the boundary where a naive `now_ms * 360` overflows uint32_t
// (UINT32_MAX / 360 ≈ 11'930'464), the correctly-implemented formula
// produces a stable hue with adjacent-input stability.
void test_rainbow_no_overflow_near_uint32_div_360(void) {
    const Rgb lo  = rainbow(WordId::HAPPY, 11'930'463);
    const Rgb mid = rainbow(WordId::HAPPY, 11'930'464);
    const Rgb hi  = rainbow(WordId::HAPPY, 11'930'465);
    // Adjacent millisecond inputs must land in the same hue bucket
    // (at a 60 s period × 360°, one ms ≈ 0.006° — well below one
    // integer hue step). Each channel must stay within ±2 LSB.
    const auto abs8 = [](uint8_t a, uint8_t b) -> uint8_t {
        return a > b ? static_cast<uint8_t>(a - b)
                     : static_cast<uint8_t>(b - a);
    };
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.r,  mid.r));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.g,  mid.g));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.b,  mid.b));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.r, hi.r));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.g, hi.g));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.b, hi.b));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rainbow_happy_at_t0_is_red);
    RUN_TEST(test_rainbow_birth_at_t0_is_yellow_green);
    RUN_TEST(test_rainbow_day_at_t0_is_cyan);
    RUN_TEST(test_rainbow_name_at_t0_is_purple);
    RUN_TEST(test_rainbow_period_is_60s);
    RUN_TEST(test_rainbow_wraps_across_uint32_max);
    RUN_TEST(test_rainbow_no_overflow_near_uint32_div_360);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests — expect all 7 to pass**

Run: `cd firmware && pio test -e native -f test_display_rainbow`
Expected: 7 tests pass. If any fail (especially the overflow test),
inspect `rainbow.cpp` for a missing `now_ms % RAINBOW_PERIOD_MS`.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 90/90 pass (79 pre-display + 4 led_map + 7 rainbow).

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_rainbow/
git commit -m "feat(firmware): complete display rainbow test coverage"
```

---

## Task 5: Palette scaffold + first tests

**Files:**
- Create: `firmware/lib/display/include/display/palette.h`
- Create: `firmware/lib/display/src/palette.cpp`
- Create: `firmware/test/test_display_palette/test_palette.cpp`

- [ ] **Step 1: Write the failing first tests**

```cpp
// firmware/test/test_display_palette/test_palette.cpp
#include <unity.h>
#include "display/palette.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_warm_white_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, warm_white().r);
    TEST_ASSERT_EQUAL_UINT8(170, warm_white().g);
    TEST_ASSERT_EQUAL_UINT8(100, warm_white().b);
}

void test_amber_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, amber().r);
    TEST_ASSERT_EQUAL_UINT8(120, amber().g);
    TEST_ASSERT_EQUAL_UINT8(30,  amber().b);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_warm_white_rgb);
    RUN_TEST(test_amber_rgb);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_display_palette`
Expected: compile FAIL with "fatal error: display/palette.h: No such
file or directory".

- [ ] **Step 3: Create the palette header**

```cpp
// firmware/lib/display/include/display/palette.h
#pragma once

#include <cstdint>

#include "display/rgb.h"
#include "holidays.h"
#include "word_id.h"

namespace wc::display {

// Per-entry maximum RGB sum, enforced by test_palette_power_budget.
// See the spec §Palette power budget for derivation.
inline constexpr uint16_t PALETTE_MAX_RGB_SUM = 700;

// Returns the RGB color a given word should be under a given palette.
// Implementation MUST switch(p) over named Palette enumerators (not
// array-indexed lookup) so a future Palette enum addition triggers
// -Wswitch-enum at compile time. Within a palette: word receives
// palette_colors[static_cast<uint8_t>(w) % size()].
Rgb color_for(Palette p, WordId w);

// Convenience constants.
Rgb warm_white();   // {255, 170, 100}
Rgb amber();        // {255, 120, 30}

} // namespace wc::display
```

- [ ] **Step 4: Create the palette implementation**

```cpp
// firmware/lib/display/src/palette.cpp
#include "display/palette.h"

#include <array>
#include <cstddef>

namespace wc::display {

namespace {

// Cycle a WordId through a fixed-size color array by enum index.
// Decoupled from PCB LED chain order — palette behavior survives
// any future PCB LED reorder without test changes.
template <std::size_t N>
Rgb pick(const std::array<Rgb, N>& colors, WordId w) {
    return colors[static_cast<std::size_t>(w) % N];
}

} // namespace

Rgb warm_white() { return Rgb{255, 170, 100}; }
Rgb amber()      { return Rgb{255, 120,  30}; }

Rgb color_for(Palette p, WordId w) {
    switch (p) {
        case Palette::WARM_WHITE:
            return warm_white();
        case Palette::MLK_PURPLE:
            return Rgb{128, 0, 180};
        case Palette::VALENTINES: {
            static constexpr std::array<Rgb, 2> cs = {{
                {255,  30,  60},
                {255, 120, 160},
            }};
            return pick(cs, w);
        }
        case Palette::WOMEN_PURPLE:
            return Rgb{150, 50, 180};
        case Palette::EARTH_DAY: {
            static constexpr std::array<Rgb, 2> cs = {{
                { 30, 140,  60},
                { 30,  80, 200},
            }};
            return pick(cs, w);
        }
        case Palette::EASTER_PASTEL: {
            static constexpr std::array<Rgb, 4> cs = {{
                {255, 180, 200},
                {180, 220, 255},
                {220, 255, 180},
                {255, 230, 180},
            }};
            return pick(cs, w);
        }
        case Palette::JUNETEENTH: {
            static constexpr std::array<Rgb, 3> cs = {{
                {200,  30,  30},
                { 10,  10,  10},
                { 30, 160,  60},
            }};
            return pick(cs, w);
        }
        case Palette::INDIGENOUS: {
            static constexpr std::array<Rgb, 3> cs = {{
                {160,  80,  40},
                {200, 140,  80},
                {120,  60,  30},
            }};
            return pick(cs, w);
        }
        case Palette::HALLOWEEN: {
            static constexpr std::array<Rgb, 2> cs = {{
                {255, 100,   0},
                {140,   0, 180},
            }};
            return pick(cs, w);
        }
        case Palette::CHRISTMAS: {
            static constexpr std::array<Rgb, 2> cs = {{
                {220,  30,  30},
                { 30, 160,  60},
            }};
            return pick(cs, w);
        }
    }
    // -Wswitch-enum + complete handling makes this unreachable.
    return warm_white();
}

} // namespace wc::display
```

- [ ] **Step 5: Run tests — expect both to pass**

Run: `cd firmware && pio test -e native -f test_display_palette`
Expected: 2 tests pass.

- [ ] **Step 6: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 92/92 pass.

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/display/include/display/palette.h \
        firmware/lib/display/src/palette.cpp \
        firmware/test/test_display_palette/
git commit -m "feat(firmware): scaffold display palette lookup"
```

---

## Task 6: Palette full coverage + power budget

**Files:**
- Modify: `firmware/test/test_display_palette/test_palette.cpp`

Adds 7 more tests. All should pass against the Task 5 implementation;
if any fail, fix the impl, not the test.

- [ ] **Step 1: Replace the test file with full coverage**

```cpp
// firmware/test/test_display_palette/test_palette.cpp
#include <unity.h>
#include "display/palette.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_warm_white_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, warm_white().r);
    TEST_ASSERT_EQUAL_UINT8(170, warm_white().g);
    TEST_ASSERT_EQUAL_UINT8(100, warm_white().b);
}

void test_amber_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, amber().r);
    TEST_ASSERT_EQUAL_UINT8(120, amber().g);
    TEST_ASSERT_EQUAL_UINT8(30,  amber().b);
}

void test_palette_warm_white_is_warm_white_for_all_words(void) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const Rgb c = color_for(Palette::WARM_WHITE, static_cast<WordId>(i));
        TEST_ASSERT_EQUAL_UINT8(255, c.r);
        TEST_ASSERT_EQUAL_UINT8(170, c.g);
        TEST_ASSERT_EQUAL_UINT8(100, c.b);
    }
}

void test_palette_halloween_alternates_orange_purple_by_enum_idx(void) {
    // IT=0 → orange; IS=1 → purple. (enum idx parity decides.)
    const Rgb orange = color_for(Palette::HALLOWEEN, WordId::IT);
    const Rgb purple = color_for(Palette::HALLOWEEN, WordId::IS);
    TEST_ASSERT_EQUAL_UINT8(255, orange.r);
    TEST_ASSERT_EQUAL_UINT8(100, orange.g);
    TEST_ASSERT_EQUAL_UINT8(  0, orange.b);
    TEST_ASSERT_EQUAL_UINT8(140, purple.r);
    TEST_ASSERT_EQUAL_UINT8(  0, purple.g);
    TEST_ASSERT_EQUAL_UINT8(180, purple.b);
}

void test_palette_christmas_alternates_red_green(void) {
    const Rgb red   = color_for(Palette::CHRISTMAS, WordId::IT);  // idx 0
    const Rgb green = color_for(Palette::CHRISTMAS, WordId::IS);  // idx 1
    TEST_ASSERT_EQUAL_UINT8(220, red.r);
    TEST_ASSERT_EQUAL_UINT8( 30, red.g);
    TEST_ASSERT_EQUAL_UINT8( 30, red.b);
    TEST_ASSERT_EQUAL_UINT8( 30, green.r);
    TEST_ASSERT_EQUAL_UINT8(160, green.g);
    TEST_ASSERT_EQUAL_UINT8( 60, green.b);
}

void test_palette_valentines_alternates_red_pink(void) {
    const Rgb red  = color_for(Palette::VALENTINES, WordId::IT);
    const Rgb pink = color_for(Palette::VALENTINES, WordId::IS);
    TEST_ASSERT_EQUAL_UINT8(255, red.r);
    TEST_ASSERT_EQUAL_UINT8( 30, red.g);
    TEST_ASSERT_EQUAL_UINT8( 60, red.b);
    TEST_ASSERT_EQUAL_UINT8(255, pink.r);
    TEST_ASSERT_EQUAL_UINT8(120, pink.g);
    TEST_ASSERT_EQUAL_UINT8(160, pink.b);
}

void test_palette_juneteenth_cycles_three_colors(void) {
    // idx 0, 3, 6 → palette[0] (red-ish)
    // idx 1, 4, 7 → palette[1] (black)
    // idx 2, 5, 8 → palette[2] (green)
    const Rgb r0 = color_for(Palette::JUNETEENTH, static_cast<WordId>(0));
    const Rgb r1 = color_for(Palette::JUNETEENTH, static_cast<WordId>(1));
    const Rgb r2 = color_for(Palette::JUNETEENTH, static_cast<WordId>(2));
    const Rgb r3 = color_for(Palette::JUNETEENTH, static_cast<WordId>(3));
    TEST_ASSERT_EQUAL_UINT8(200, r0.r);   // red
    TEST_ASSERT_EQUAL_UINT8( 10, r1.r);   // black
    TEST_ASSERT_EQUAL_UINT8( 30, r2.r);   // green
    TEST_ASSERT_EQUAL_UINT8(200, r3.r);   // wrap → red
}

void test_palette_easter_cycles_four_colors(void) {
    const Rgb r0 = color_for(Palette::EASTER_PASTEL, static_cast<WordId>(0));
    const Rgb r4 = color_for(Palette::EASTER_PASTEL, static_cast<WordId>(4));
    // Pink
    TEST_ASSERT_EQUAL_UINT8(255, r0.r);
    TEST_ASSERT_EQUAL_UINT8(180, r0.g);
    TEST_ASSERT_EQUAL_UINT8(200, r0.b);
    // Wrap back to pink
    TEST_ASSERT_EQUAL_UINT8(255, r4.r);
    TEST_ASSERT_EQUAL_UINT8(180, r4.g);
    TEST_ASSERT_EQUAL_UINT8(200, r4.b);
}

// The PSU-safety test: no palette entry ever exceeds the cap.
void test_palette_power_budget(void) {
    // Enumerate every Palette value. Adding a new Palette requires
    // this list to be updated — intentional failure mode for the
    // future St. Patrick's Day case.
    const Palette all_palettes[] = {
        Palette::WARM_WHITE,
        Palette::MLK_PURPLE,
        Palette::VALENTINES,
        Palette::WOMEN_PURPLE,
        Palette::EARTH_DAY,
        Palette::EASTER_PASTEL,
        Palette::JUNETEENTH,
        Palette::INDIGENOUS,
        Palette::HALLOWEEN,
        Palette::CHRISTMAS,
    };
    for (Palette p : all_palettes) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
            const WordId w = static_cast<WordId>(i);
            const Rgb c = color_for(p, w);
            const uint16_t sum = static_cast<uint16_t>(c.r) +
                                 static_cast<uint16_t>(c.g) +
                                 static_cast<uint16_t>(c.b);
            TEST_ASSERT_LESS_OR_EQUAL_UINT16(PALETTE_MAX_RGB_SUM, sum);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_warm_white_rgb);
    RUN_TEST(test_amber_rgb);
    RUN_TEST(test_palette_warm_white_is_warm_white_for_all_words);
    RUN_TEST(test_palette_halloween_alternates_orange_purple_by_enum_idx);
    RUN_TEST(test_palette_christmas_alternates_red_green);
    RUN_TEST(test_palette_valentines_alternates_red_pink);
    RUN_TEST(test_palette_juneteenth_cycles_three_colors);
    RUN_TEST(test_palette_easter_cycles_four_colors);
    RUN_TEST(test_palette_power_budget);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests — expect all 9 to pass**

Run: `cd firmware && pio test -e native -f test_display_palette`
Expected: 9 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 99/99 pass.

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_palette/
git commit -m "feat(firmware): complete display palette test coverage"
```

---

## Task 7: Renderer scaffold (unlit + warm white)

**Files:**
- Create: `firmware/lib/display/include/display/renderer.h`
- Create: `firmware/lib/display/src/renderer.cpp`
- Create: `firmware/test/test_display_renderer/test_renderer.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
// firmware/test/test_display_renderer/test_renderer.cpp
#include <unity.h>
#include "display/led_map.h"
#include "display/renderer.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

namespace {

// Reusable input factory — override fields per test.
RenderInput make_input() {
    RenderInput in{};
    in.year   = 2027;    // non-holiday, non-birthday year for defaults
    in.month  = 5;
    in.day    = 15;
    in.hour   = 14;      // full-bright window
    in.minute = 0;
    in.now_ms = 0;
    in.seconds_since_sync = 0;  // fresh sync
    in.birthday = BirthdayConfig{10, 6, 18, 10};  // Emory birthday
    return in;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_unlit_word_is_black(void) {
    // 14:00 → no HAPPY/BIRTH/DAY/NAME (non-birthday); NOON is
    // an AM/PM-specific word that should NOT be lit at 14:00.
    // (At 14:00 the word set is about "two oclock in the
    // afternoon" — not "noon".)
    const Frame f = render(make_input());
    const Rgb noon = f[index_of(WordId::NOON)];
    TEST_ASSERT_EQUAL_UINT8(0, noon.r);
    TEST_ASSERT_EQUAL_UINT8(0, noon.g);
    TEST_ASSERT_EQUAL_UINT8(0, noon.b);
    // Decor words should be black on a non-birthday day.
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        const Rgb c = f[index_of(w)];
        TEST_ASSERT_EQUAL_UINT8(0, c.r);
        TEST_ASSERT_EQUAL_UINT8(0, c.g);
        TEST_ASSERT_EQUAL_UINT8(0, c.b);
    }
}

void test_warm_white_default(void) {
    // 14:00 non-holiday non-birthday fresh-sync: every lit word
    // should be {255, 170, 100}.
    const Frame f = render(make_input());
    // IT + IS are always part of any time readout.
    for (WordId w : {WordId::IT, WordId::IS}) {
        const Rgb c = f[index_of(w)];
        TEST_ASSERT_EQUAL_UINT8(255, c.r);
        TEST_ASSERT_EQUAL_UINT8(170, c.g);
        TEST_ASSERT_EQUAL_UINT8(100, c.b);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unlit_word_is_black);
    RUN_TEST(test_warm_white_default);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm compile fail**

Run: `cd firmware && pio test -e native -f test_display_renderer`
Expected: compile FAIL with "display/renderer.h: No such file".

- [ ] **Step 3: Create the renderer header**

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
    //   year   ≥ 2023 (earliest plausible wall-clock for either
    //                  daughter's lifetime; pre-2023 renders
    //                  cleanly but holiday behavior is unspecified).
    // The renderer does NOT re-validate; bad inputs trigger
    // undefined visual output but never crash. Bounds-checking is
    // the `rtc` module's responsibility.
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;

    // Monotonic millis for rainbow phase. Wraps at ~49 days; rainbow
    // math handles the wrap correctly.
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

- [ ] **Step 4: Create the renderer implementation**

```cpp
// firmware/lib/display/src/renderer.cpp
#include "display/renderer.h"

#include <cstdint>

#include "birthday.h"
#include "dim_schedule.h"
#include "display/led_map.h"
#include "display/palette.h"
#include "display/rainbow.h"
#include "holidays.h"
#include "time_to_words.h"
#include "word_id.h"

namespace wc::display {

namespace {

bool word_in_set(WordId w, const WordSet& s) {
    for (uint8_t i = 0; i < s.count; ++i) {
        if (s.words[i] == w) return true;
    }
    return false;
}

bool is_decor_word(WordId w) {
    return w == WordId::HAPPY || w == WordId::BIRTH ||
           w == WordId::DAY   || w == WordId::NAME;
}

// (r * bright_u8 + 127) / 255 — round-to-nearest integer division.
uint8_t scale_channel(uint8_t v, uint8_t bright_u8) {
    const uint16_t num =
        static_cast<uint16_t>(v) * static_cast<uint16_t>(bright_u8) + 127U;
    return static_cast<uint8_t>(num / 255U);
}

Rgb apply_dim(Rgb c, uint8_t bright_u8) {
    return Rgb{
        scale_channel(c.r, bright_u8),
        scale_channel(c.g, bright_u8),
        scale_channel(c.b, bright_u8),
    };
}

} // namespace

Frame render(const RenderInput& in) {
    const WordSet lit = time_to_words(in.hour, in.minute);
    const BirthdayState bs = birthday_state(
        in.month, in.day, in.hour, in.minute, in.birthday);
    const Palette palette = palette_for_date(in.year, in.month, in.day);
    const bool stale = in.seconds_since_sync > STALE_SYNC_THRESHOLD_S;
    const uint8_t bright_u8 = static_cast<uint8_t>(
        wc::brightness(in.hour, in.minute) * 255.0f + 0.5f);

    Frame frame{};  // zero-initialized (all black)
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const bool decor = is_decor_word(w);

        // Time words come from time_to_words; decor is lit only on
        // birthdays.
        const bool lit_this_tick =
            word_in_set(w, lit) || (bs.is_birthday && decor);
        if (!lit_this_tick) continue;

        // Priority: birthday rainbow > holiday > amber > warm white.
        Rgb color;
        if (bs.is_birthday && decor) {
            color = rainbow(w, in.now_ms);
        } else if (palette != Palette::WARM_WHITE) {
            color = color_for(palette, w);
        } else if (stale) {
            color = amber();
        } else {
            color = warm_white();
        }

        frame[index_of(w)] = apply_dim(color, bright_u8);
    }
    return frame;
}

} // namespace wc::display
```

- [ ] **Step 5: Run renderer tests — expect both pass**

Run: `cd firmware && pio test -e native -f test_display_renderer`
Expected: 2 tests pass.

- [ ] **Step 6: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 101/101 pass.

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/display/include/display/renderer.h \
        firmware/lib/display/src/renderer.cpp \
        firmware/test/test_display_renderer/
git commit -m "feat(firmware): scaffold display renderer (warm white path)"
```

---

## Task 8: Renderer holiday + stale-sync path

**Files:**
- Modify: `firmware/test/test_display_renderer/test_renderer.cpp`

Adds 4 tests covering the holiday-palette branch and the
amber-stale-sync branch. No impl change needed — the Task 7
implementation already handles these paths; these tests verify.

- [ ] **Step 1: Append the 4 tests to the existing test file**

Above the `main()` function, BEFORE the existing `RUN_TEST` calls
(keeping the existing two tests intact):

```cpp
void test_holiday_palette_replaces_warm_white(void) {
    RenderInput in = make_input();
    in.month = 10;   // Halloween
    in.day   = 31;
    in.hour  = 14;
    const Frame f = render(in);
    // IT is enum 0 — Halloween palette[0] = orange {255, 100, 0}
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
    // IS is enum 1 — Halloween palette[1] = purple {140, 0, 180}
    const Rgb is = f[index_of(WordId::IS)];
    TEST_ASSERT_EQUAL_UINT8(140, is.r);
    TEST_ASSERT_EQUAL_UINT8(  0, is.g);
    TEST_ASSERT_EQUAL_UINT8(180, is.b);
}

void test_stale_sync_replaces_warm_white_on_time_words(void) {
    RenderInput in = make_input();
    in.seconds_since_sync = 90'000;   // > 86'400
    const Frame f = render(in);
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(120, it.g);
    TEST_ASSERT_EQUAL_UINT8( 30, it.b);
}

void test_stale_sync_boundary(void) {
    RenderInput in = make_input();
    in.seconds_since_sync = 86'400;    // NOT stale (strict >)
    Frame f = render(in);
    Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(170, it.g);
    TEST_ASSERT_EQUAL_UINT8(100, it.b);

    in.seconds_since_sync = 86'401;    // IS stale
    f = render(in);
    it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(120, it.g);
    TEST_ASSERT_EQUAL_UINT8( 30, it.b);
}

void test_stale_sync_does_not_override_holiday(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 31;
    in.seconds_since_sync = 90'000;  // stale
    const Frame f = render(in);
    // IT still shows Halloween orange, not amber.
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
}
```

And update `main()` to run them:

```cpp
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unlit_word_is_black);
    RUN_TEST(test_warm_white_default);
    RUN_TEST(test_holiday_palette_replaces_warm_white);
    RUN_TEST(test_stale_sync_replaces_warm_white_on_time_words);
    RUN_TEST(test_stale_sync_boundary);
    RUN_TEST(test_stale_sync_does_not_override_holiday);
    return UNITY_END();
}
```

- [ ] **Step 2: Run renderer tests — expect 6 pass**

Run: `cd firmware && pio test -e native -f test_display_renderer`
Expected: 6 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 105/105 pass.

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_renderer/
git commit -m "feat(firmware): renderer holiday + stale-sync test coverage"
```

---

## Task 9: Renderer birthday path

**Files:**
- Modify: `firmware/test/test_display_renderer/test_renderer.cpp`

Adds 3 tests covering the birthday-rainbow branch + its interaction
with the holiday palette.

- [ ] **Step 1: Append the 3 tests to the existing test file**

Above `main()`:

```cpp
// Returns true if `c` is a saturated rainbow-family color: one
// channel ≥ 250, at least one channel ≤ 5.
bool is_rainbow_family(Rgb c) {
    const uint8_t mx = c.r > c.g ? (c.r > c.b ? c.r : c.b)
                                 : (c.g > c.b ? c.g : c.b);
    const uint8_t mn = c.r < c.g ? (c.r < c.b ? c.r : c.b)
                                 : (c.g < c.b ? c.g : c.b);
    return mx >= 250 && mn <= 5;
}

void test_birthday_rainbow_on_decor_only(void) {
    RenderInput in = make_input();
    in.month = 10;                // Emory birthday
    in.day   = 6;
    in.now_ms = 0;
    const Frame f = render(in);
    // Decor words show rainbow family.
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        TEST_ASSERT_TRUE_MESSAGE(is_rainbow_family(f[index_of(w)]),
                                 "decor word should be rainbow-family");
    }
    // Time words show warm white (Oct 6 is not a holiday).
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(170, it.g);
    TEST_ASSERT_EQUAL_UINT8(100, it.b);
}

void test_birthday_rainbow_shifts_with_now_ms(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 6;

    in.now_ms = 0;
    const Frame f0 = render(in);

    in.now_ms = 15'000;   // 1/4 of rainbow period — HAPPY shifts 90°
    const Frame f15 = render(in);

    // HAPPY decor shifts.
    const Rgb happy0  = f0[index_of(WordId::HAPPY)];
    const Rgb happy15 = f15[index_of(WordId::HAPPY)];
    const bool shifted = (happy0.r != happy15.r) ||
                         (happy0.g != happy15.g) ||
                         (happy0.b != happy15.b);
    TEST_ASSERT_TRUE(shifted);

    // Time words (warm white) are identical across the two frames.
    const Rgb it0  = f0[index_of(WordId::IT)];
    const Rgb it15 = f15[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(it0.r, it15.r);
    TEST_ASSERT_EQUAL_UINT8(it0.g, it15.g);
    TEST_ASSERT_EQUAL_UINT8(it0.b, it15.b);
}

void test_birthday_rainbow_wins_over_holiday_palette(void) {
    // Synthetic BirthdayConfig coinciding with Halloween — neither
    // daughter's real birthday, but exercises precedence.
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 31;
    in.birthday = BirthdayConfig{10, 31, 12, 0};
    const Frame f = render(in);
    // Decor still rainbow despite Halloween palette being active.
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        TEST_ASSERT_TRUE(is_rainbow_family(f[index_of(w)]));
    }
    // Time words use Halloween palette, not warm white, not amber.
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);   // Halloween orange
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
}
```

Update `main()` to run them after the existing tests:

```cpp
    RUN_TEST(test_birthday_rainbow_on_decor_only);
    RUN_TEST(test_birthday_rainbow_shifts_with_now_ms);
    RUN_TEST(test_birthday_rainbow_wins_over_holiday_palette);
```

- [ ] **Step 2: Run tests — 9 pass**

Run: `cd firmware && pio test -e native -f test_display_renderer`
Expected: 9 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 108/108 pass.

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_renderer/
git commit -m "feat(firmware): renderer birthday-rainbow coverage"
```

---

## Task 10: Renderer stale×birthday + dim

**Files:**
- Modify: `firmware/test/test_display_renderer/test_renderer.cpp`

Adds 4 tests: stale-sync during birthday, and dim multiplier in
three configurations. Verifies the full priority table under
combined conditions.

- [ ] **Step 1: Append 4 tests to the existing file**

```cpp
void test_stale_sync_does_not_override_birthday_decor(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 6;
    in.seconds_since_sync = 90'000;   // stale
    const Frame f = render(in);
    // Decor words still rainbow (priority 1 wins over stale).
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        TEST_ASSERT_TRUE(is_rainbow_family(f[index_of(w)]));
    }
    // Time words show amber (stale wins over warm white since no
    // holiday palette; birthday doesn't light time words).
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(120, it.g);
    TEST_ASSERT_EQUAL_UINT8( 30, it.b);
}

void test_dim_multiplier_warm_white(void) {
    RenderInput in = make_input();
    in.hour = 20;    // dim window
    const Frame f = render(in);
    // Dim math: bright_u8 = round(0.1 × 255) = 26.
    // (255 × 26 + 127)/255 = 26, (170 × 26 + 127)/255 = 17,
    // (100 × 26 + 127)/255 = 10.
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(26, it.r);
    TEST_ASSERT_EQUAL_UINT8(17, it.g);
    TEST_ASSERT_EQUAL_UINT8(10, it.b);
}

void test_dim_multiplier_applies_to_rainbow(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 6;
    in.hour  = 20;    // dim
    in.now_ms = 0;
    const Frame f = render(in);
    // HAPPY at now_ms=0 is {255, 0, 0} pre-dim; dim → {26, 0, 0}.
    const Rgb happy = f[index_of(WordId::HAPPY)];
    TEST_ASSERT_EQUAL_UINT8(26, happy.r);
    TEST_ASSERT_EQUAL_UINT8( 0, happy.g);
    TEST_ASSERT_EQUAL_UINT8( 0, happy.b);
}

void test_dim_boundary_matches_dim_schedule(void) {
    RenderInput in = make_input();

    // 18:59 — full bright → IT is warm white.
    in.hour = 18; in.minute = 59;
    Frame f = render(in);
    TEST_ASSERT_EQUAL_UINT8(255, f[index_of(WordId::IT)].r);
    TEST_ASSERT_EQUAL_UINT8(170, f[index_of(WordId::IT)].g);
    TEST_ASSERT_EQUAL_UINT8(100, f[index_of(WordId::IT)].b);

    // 19:00 — dim → IT is {26, 17, 10}.
    in.hour = 19; in.minute = 0;
    f = render(in);
    TEST_ASSERT_EQUAL_UINT8(26, f[index_of(WordId::IT)].r);
    TEST_ASSERT_EQUAL_UINT8(17, f[index_of(WordId::IT)].g);
    TEST_ASSERT_EQUAL_UINT8(10, f[index_of(WordId::IT)].b);

    // 07:59 — still dim.
    in.hour = 7; in.minute = 59;
    f = render(in);
    TEST_ASSERT_EQUAL_UINT8(26, f[index_of(WordId::IT)].r);
    TEST_ASSERT_EQUAL_UINT8(17, f[index_of(WordId::IT)].g);
    TEST_ASSERT_EQUAL_UINT8(10, f[index_of(WordId::IT)].b);

    // 08:00 — bright.
    in.hour = 8; in.minute = 0;
    f = render(in);
    TEST_ASSERT_EQUAL_UINT8(255, f[index_of(WordId::IT)].r);
    TEST_ASSERT_EQUAL_UINT8(170, f[index_of(WordId::IT)].g);
    TEST_ASSERT_EQUAL_UINT8(100, f[index_of(WordId::IT)].b);
}
```

Add to `main()`:

```cpp
    RUN_TEST(test_stale_sync_does_not_override_birthday_decor);
    RUN_TEST(test_dim_multiplier_warm_white);
    RUN_TEST(test_dim_multiplier_applies_to_rainbow);
    RUN_TEST(test_dim_boundary_matches_dim_schedule);
```

- [ ] **Step 2: Run tests — 13 pass**

Run: `cd firmware && pio test -e native -f test_display_renderer`
Expected: 13 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 112/112 pass.

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_renderer/
git commit -m "feat(firmware): renderer stale×birthday + dim coverage"
```

---

## Task 11: Renderer golden + atomic-transition tests

**Files:**
- Create: `firmware/test/test_display_renderer_golden/test_renderer_golden.cpp`

Two tests: a category-based golden frame for Emory's birthday at
2:23 PM (13 renderer tests don't exercise every LED simultaneously;
this one does), and an atomic-transition regression test that
protects against anyone adding stateful caching to the renderer.

- [ ] **Step 1: Create the golden test file**

```cpp
// firmware/test/test_display_renderer_golden/test_renderer_golden.cpp
#include <unity.h>
#include "display/led_map.h"
#include "display/renderer.h"
#include "time_to_words.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

namespace {

bool word_in(WordId w, const WordSet& s) {
    for (uint8_t i = 0; i < s.count; ++i) if (s.words[i] == w) return true;
    return false;
}

bool is_decor(WordId w) {
    return w == WordId::HAPPY || w == WordId::BIRTH ||
           w == WordId::DAY   || w == WordId::NAME;
}

bool is_rainbow_family(Rgb c) {
    const uint8_t mx = c.r > c.g ? (c.r > c.b ? c.r : c.b)
                                 : (c.g > c.b ? c.g : c.b);
    const uint8_t mn = c.r < c.g ? (c.r < c.b ? c.r : c.b)
                                 : (c.g < c.b ? c.g : c.b);
    return mx >= 250 && mn <= 5;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

// Fixed input: Emory, 2030-10-06 14:23, fresh sync, now_ms=0.
// Every LED is classified by role; the full frame is asserted.
void test_golden_birthday_non_holiday_bright(void) {
    RenderInput in{};
    in.year   = 2030;
    in.month  = 10;
    in.day    = 6;
    in.hour   = 14;
    in.minute = 23;
    in.now_ms = 0;
    in.seconds_since_sync = 0;
    in.birthday = BirthdayConfig{10, 6, 18, 10};

    const Frame f = render(in);
    const WordSet lit = time_to_words(14, 23);

    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const Rgb c = f[index_of(w)];
        if (word_in(w, lit)) {
            // Time words: warm white exactly.
            TEST_ASSERT_EQUAL_UINT8(255, c.r);
            TEST_ASSERT_EQUAL_UINT8(170, c.g);
            TEST_ASSERT_EQUAL_UINT8(100, c.b);
        } else if (is_decor(w)) {
            // Decor words: rainbow family (some phase of 4 decor).
            TEST_ASSERT_TRUE_MESSAGE(is_rainbow_family(c),
                                     "decor word not rainbow-family");
        } else {
            // All other LEDs: black.
            TEST_ASSERT_EQUAL_UINT8(0, c.r);
            TEST_ASSERT_EQUAL_UINT8(0, c.g);
            TEST_ASSERT_EQUAL_UINT8(0, c.b);
        }
    }
}

// Three signals change simultaneously (palette boundary + birthday
// start + dim window entry). Verify the rendered frame is
// internally consistent with the priority table — no LED
// "half-transitioned" between two states. Regression guard against
// adding stateful caching.
void test_multi_signal_transition_is_atomic(void) {
    RenderInput in{};
    in.year   = 2030;
    in.month  = 11;        // Nov 1 — non-Halloween, non-holiday
    in.day    = 1;
    in.hour   = 0;         // dim window active
    in.minute = 0;
    in.now_ms = 0;
    in.seconds_since_sync = 0;
    in.birthday = BirthdayConfig{11, 1, 0, 0};  // synthetic Nov 1 birthday

    const Frame f = render(in);
    const WordSet lit = time_to_words(0, 0);

    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const Rgb c = f[index_of(w)];
        if (word_in(w, lit)) {
            // Time words: warm white × dim = {26, 17, 10}.
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(26, c.r,
                "time word R should be warm-white × dim");
            TEST_ASSERT_EQUAL_UINT8(17, c.g);
            TEST_ASSERT_EQUAL_UINT8(10, c.b);
        } else if (is_decor(w)) {
            // Decor words: rainbow value at now_ms=0 × dim.
            // Rainbow produces saturated colors; dim scales by 26/255,
            // which keeps at-least-one channel non-zero. Assert not-black.
            const bool not_black = (c.r != 0) || (c.g != 0) || (c.b != 0);
            TEST_ASSERT_TRUE_MESSAGE(not_black,
                "decor word should be lit in dim × rainbow");
        } else {
            TEST_ASSERT_EQUAL_UINT8(0, c.r);
            TEST_ASSERT_EQUAL_UINT8(0, c.g);
            TEST_ASSERT_EQUAL_UINT8(0, c.b);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_golden_birthday_non_holiday_bright);
    RUN_TEST(test_multi_signal_transition_is_atomic);
    return UNITY_END();
}
```

- [ ] **Step 2: Run golden tests — 2 pass**

Run: `cd firmware && pio test -e native -f test_display_renderer_golden`
Expected: 2 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: **114/114 pass** — matches the spec's target test count.

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_display_renderer_golden/
git commit -m "feat(firmware): renderer golden + atomic transition tests"
```

---

## Task 12: ESP32 adapter (display.cpp)

**Files:**
- Create: `firmware/lib/display/include/display.h`
- Create: `firmware/lib/display/src/display.cpp`

The adapter holds FastLED state; guarded with `#ifdef ARDUINO`
following the wifi_provision / buttons pattern (PlatformIO LDF's
`deep+` mode would otherwise compile this TU into native where
`<FastLED.h>` doesn't exist).

- [ ] **Step 1: Create the public header**

```cpp
// firmware/lib/display/include/display.h
//
// ESP32-only public API. Depends on FastLED + Arduino.h. Include
// this ONLY from translation units that compile under the Arduino
// toolchain (the emory / nora PlatformIO envs). Native tests
// include the pure-logic headers under display/ instead — never
// this header.
#pragma once

#include <Arduino.h>
#include "display/rgb.h"

namespace wc::display {

// Initialize FastLED (addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, 35))
// and set the 1.8 A runtime power ceiling. Idempotent.
void begin();

// Push a rendered Frame to the LED strip (copies the Frame into the
// internal FastLED CRGB buffer and calls FastLED.show()). The
// adapter owns no color policy — it pushes exactly what render()
// returned.
void show(const Frame& frame);

} // namespace wc::display
```

- [ ] **Step 2: Create the adapter**

```cpp
// firmware/lib/display/src/display.cpp
//
// Guarded with #ifdef ARDUINO — PlatformIO LDF's deep+ mode would
// otherwise compile this TU in the native test build where
// FastLED.h doesn't exist. Same pattern as
// firmware/lib/wifi_provision/src/{nvs_store,dns_wrapper,web_server,
// wifi_provision}.cpp and firmware/lib/buttons/src/buttons.cpp.
#ifdef ARDUINO

#include <Arduino.h>
#include <FastLED.h>

#include "display.h"
#include "display/rgb.h"
#include "pinmap.h"       // PIN_LED_DATA

namespace wc::display {

namespace {

CRGB leds[LED_COUNT];
bool started = false;

} // namespace

void begin() {
    if (started) return;
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, LED_COUNT);
    // Runtime layer-2 defense on the palette power budget. If a
    // palette tune ever emits a frame that would pull more than
    // 1.8 A on the strip, FastLED scales brightness down
    // automatically — belt-and-suspenders for the build-time
    // test_palette_power_budget invariant.
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

#endif  // ARDUINO
```

- [ ] **Step 3: Verify emory build**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: SUCCESS. FastLED links; the `display.cpp` TU gets pulled
into the ESP32 build by PlatformIO LDF (even without a main.cpp
reference yet — LDF walks library headers). Flash usage grows by a
few KB for FastLED initialization code.

- [ ] **Step 4: Verify nora build**

Run: `cd firmware && pio run -e nora --target=checkprogsize`
Expected: SUCCESS.

- [ ] **Step 5: Verify native suite unchanged**

Run: `cd firmware && pio test -e native`
Expected: 114/114 pass. The `#ifdef ARDUINO` guard keeps this TU
out of the native build.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/display/include/display.h \
        firmware/lib/display/src/display.cpp
git commit -m "feat(firmware): display ESP32 adapter + FastLED power cap"
```

---

## Task 13: Bring-up wire-up in main.cpp

**Files:**
- Modify: `firmware/src/main.cpp`

A minimal integration that calls `display::begin()` and pushes a
hardcoded-time frame so the breadboard hardware check can exercise
the full pipeline. The real state-machine integration (captive
portal blanking, RTC-driven time, dim-window interaction with the
current boot state) lives in the future `main.cpp integration spec`
and replaces this temporary wiring.

- [ ] **Step 1: Replace main.cpp**

Current `firmware/src/main.cpp`:

```cpp
// firmware/src/main.cpp
#include <Arduino.h>
#include "wifi_provision.h"
#include "buttons.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();

    wc::buttons::begin([](wc::buttons::Event e) {
        using BE = wc::buttons::Event;
        using WS = wc::wifi_provision::State;
        switch (e) {
            case BE::HourTick:
                Serial.println("[buttons] HourTick (rtc module not yet wired)");
                break;
            case BE::MinuteTick:
                Serial.println("[buttons] MinuteTick (rtc module not yet wired)");
                break;
            case BE::AudioPressed:
                if (wc::wifi_provision::state() == WS::AwaitingConfirmation) {
                    wc::wifi_provision::confirm_audio();
                } else {
                    Serial.println("[buttons] AudioPressed (audio module not yet wired)");
                }
                break;
            case BE::ResetCombo:
                Serial.println("[buttons] ResetCombo — resetting to captive portal");
                wc::wifi_provision::reset_to_captive();
                break;
        }
    });
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();
    delay(1);  // yield to the IDLE task so watchdog + WiFi stacks run
}
```

Replace with:

```cpp
// firmware/src/main.cpp
#include <Arduino.h>
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();
    wc::display::begin();

    wc::buttons::begin([](wc::buttons::Event e) {
        using BE = wc::buttons::Event;
        using WS = wc::wifi_provision::State;
        switch (e) {
            case BE::HourTick:
                Serial.println("[buttons] HourTick (rtc module not yet wired)");
                break;
            case BE::MinuteTick:
                Serial.println("[buttons] MinuteTick (rtc module not yet wired)");
                break;
            case BE::AudioPressed:
                if (wc::wifi_provision::state() == WS::AwaitingConfirmation) {
                    wc::wifi_provision::confirm_audio();
                } else {
                    Serial.println("[buttons] AudioPressed (audio module not yet wired)");
                }
                break;
            case BE::ResetCombo:
                Serial.println("[buttons] ResetCombo — resetting to captive portal");
                wc::wifi_provision::reset_to_captive();
                break;
        }
    });
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();

    // Bring-up stub: once provisioning is Online, push a hardcoded
    // 14:23 non-holiday non-birthday frame so the breadboard LED
    // check has something to verify. This is replaced by the
    // main.cpp integration spec with a real RTC-driven render.
    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        wc::display::RenderInput in{};
        in.year   = 2027;   // non-holiday year window
        in.month  = 5;
        in.day    = 15;
        in.hour   = 14;     // "IT IS ... PAST TWO IN THE AFTERNOON"
        in.minute = 23;
        in.now_ms = millis();
        in.seconds_since_sync = wc::wifi_provision::seconds_since_last_sync();
        in.birthday = {CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                       CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE};
        wc::display::show(wc::display::render(in));
    } else {
        // Blank while not Online (captive portal runs here). The
        // future main.cpp spec may paint something prettier, e.g.
        // a slow-pulsing decor word to cue "setup mode".
        wc::display::show(wc::display::Frame{});
    }

    delay(1);  // yield to IDLE for watchdog + WiFi
}
```

- [ ] **Step 2: Verify emory build**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: SUCCESS.

- [ ] **Step 3: Verify nora build**

Run: `cd firmware && pio run -e nora --target=checkprogsize`
Expected: SUCCESS.

- [ ] **Step 4: Verify native suite unchanged**

Run: `cd firmware && pio test -e native`
Expected: 114/114 pass.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): bring-up wire-up of display in main.cpp"
```

---

## Task 14: Hardware manual-verification checklist

**Files:**
- Create: `firmware/test/hardware_checks/display_checks.md`

Handoff doc for the real-hardware tests. Executed after the
breadboard is wired per `docs/hardware/breadboard-bring-up-guide.md`
§Step 9 (WS2812B chain + 74HCT245 level shifter).

- [ ] **Step 1: Create the checklist**

```markdown
<!-- firmware/test/hardware_checks/display_checks.md -->
# display — manual hardware verification

Run these after the full breadboard bring-up reaches
`docs/hardware/breadboard-bring-up-guide.md` §Step 9. Full WS2812B
chain (35 LEDs) on GPIO 13 through the 74HCT245 level shifter +
300 Ω series resistor + 1000 µF bulk cap.

Flash: `pio run -e emory -t upload`
Serial: `pio device monitor -e emory`

## 1. Boot + captive portal + blank face

- [ ] Serial shows `word-clock booting for target: EMORY`.
- [ ] Phone shows `WordClock-Setup-XXXX` AP. Face is all dark (no
      LEDs glowing) while in AP mode. Confirms main's
      `if state == Online` guard is in effect.
- [ ] Connect phone, submit valid WiFi creds via the form. State
      advances to Online per serial logs.

## 2. First light — warm-white clock face

- [ ] After Online transition, face shows a lit clock readout in
      warm white (per the hardcoded 14:23: `IT IS ... PAST TWO IN
      THE AFTERNOON`; the exact word set is whatever
      `time_to_words(14, 23)` returns).
- [ ] Each lit word reads uniformly — no per-letter flicker, no
      dimmer halos between word pockets. Confirms the light
      channel + diffuser stack are working AND the LED chain is
      wired correctly.
- [ ] No decor words (HAPPY/BIRTH/DAY/NAME) glow — it's not a
      birthday in the hardcoded scenario.

## 3. Color order verification

Temporarily patch `firmware/src/main.cpp` to paint one LED red:
replace the `wc::display::show(wc::display::render(in))` call with
a single-red-LED frame (e.g., `Frame f{}; f[0] = {255, 0, 0};
wc::display::show(f);`). Reflash.

- [ ] D1 (LED_IT, top-left corner per the face) glows RED.
      If it glows GREEN, the FastLED addLeds color order is wrong
      (should be `GRB` for WS2812B) — revert the spec's GRB
      declaration investigation rather than rewriting the renderer.
- [ ] Revert the patch; reflash.

## 4. Level-shifter sanity (optional but recommended)

- [ ] Disconnect the 74HCT245 output wire from the WS2812B DIN.
      Reflash and power-cycle: LEDs stay dark or show random
      colors (nothing downstream is receiving a stable signal).
      Confirms the level shifter is load-bearing — the bare ESP32
      3.3 V drive at 5 V Vdd would "sometimes work" (per
      `docs/hardware/pinout.md` §Critical issues #2).
- [ ] Reconnect and reflash; face returns to normal.

## 5. Holiday palette on real hardware

Temporarily edit `firmware/src/main.cpp` to set `in.month = 10;
in.day = 31;`. Reflash.

- [ ] Lit time words show alternating ORANGE and PURPLE per the
      Halloween palette. The pattern is deterministic by WordId
      enum idx — if HAPPY were lit (it isn't on a non-birthday
      Halloween), it'd be purple because enum idx 31 % 2 == 1.
- [ ] Revert to `in.month = 5; in.day = 15;`.

## 6. Birthday rainbow

Temporarily edit `firmware/src/main.cpp` to set `in.month = 10;
in.day = 6;`. Reflash.

- [ ] Time words show WARM WHITE (Oct 6 isn't a holiday).
- [ ] HAPPY, BIRTH, DAY, and NAME words cycle through the hue
      wheel smoothly. Full 360° cycle visibly completes in ≈ 60 s.
- [ ] The four decor words are at different hues at any instant
      (90° phase offset). Visually reads as a "chase" around the
      decor set.
- [ ] Revert to the non-birthday config.

## 7. Dim window

Temporarily edit `firmware/src/main.cpp` to set `in.hour = 20;
in.minute = 0;`. Reflash.

- [ ] Face brightness drops visibly to ~10% — readable in dim
      light, not blinding in a dark room.
- [ ] Face still shows a complete time readout (no words drop out
      at 10% brightness — the 300 Ω series resistor + level
      shifter path works at low current).
- [ ] Revert to `in.hour = 14;`.

## 8. Amber stale-sync

Temporarily edit `firmware/lib/display/include/display/renderer.h`
to `STALE_SYNC_THRESHOLD_S = 60` (one minute, for the test).
Reflash, let the clock come online, then disconnect WiFi by
powering off your router or moving out of range.

- [ ] After 60 s of no WiFi, `wifi_provision::seconds_since_last_sync()`
      crosses 60. Face shifts from warm white to AMBER
      (`{255, 120, 30}`).
- [ ] Reconnect WiFi; after the next NTP sync the face returns to
      warm white within a few seconds.
- [ ] Revert the threshold to 86'400. Reflash.

## 9. Power-budget sanity (belt-and-suspenders check)

- [ ] Measure USB-C input current during full-bright warm-white
      display (no birthday). Expected: ~500-700 mA at typical
      lit-word count (~7 words).
- [ ] Measure during a synthetic palette that hits the 700-sum
      per-entry cap on all 35 LEDs (e.g., temporarily patch
      warm_white() to return `{255, 230, 215}` and reflash).
      Expected: current stays under ~1.8 A because
      `FastLED.setMaxPowerInVoltsAndMilliamps` is enforcing.
- [ ] Revert the palette patch.

## 10. Stress / burn-in

- [ ] Let the clock run for 1 h on the warm-white readout. No
      hang, no flash corruption, no serial reset messages.
- [ ] Watch for any color drift across the 35 LEDs — all lit
      words should read visually identical. A single off-color
      LED suggests a bad pixel or a misaligned word pocket.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/display_checks.md
git commit -m "docs(firmware): manual hardware checks for display module"
```

---

## Self-review

**Spec coverage:**
- LED chain map (35 entries, authoritative from CSV) → Task 2 (impl + permutation test). ✓
- Rainbow math with overflow-safe formula → Task 3 (impl) + Task 4 (full coverage, incl. overflow test). ✓
- Palette lookup via switch(Palette) + power-budget cap → Task 5 (impl with switch) + Task 6 (full coverage, incl. `test_palette_power_budget`). ✓
- Rgb type + Frame alias → Task 1. ✓
- Priority order: birthday rainbow (decor only) > holiday > amber > warm white → Task 7 (impl) + Tasks 8, 9, 10 (tests covering every pair of interacting layers, including "stale doesn't override holiday" and "stale doesn't override birthday decor"). ✓
- `RenderInput` validity contract → Task 7 header comments. ✓
- Dim multiplier formula `(v × bright_u8 + 127) / 255` → Task 7 impl + Task 10 tests (warm-white, rainbow, boundaries). ✓
- Golden category-based test + atomic transition regression → Task 11. ✓
- FastLED adapter with `setMaxPowerInVoltsAndMilliamps(5, 1800)` → Task 12. ✓
- ESP32-only adapter guarded by `#ifdef ARDUINO` → Task 12. ✓
- Bring-up wire-up + hardware verification doc → Tasks 13, 14. ✓

**Placeholder scan:** no TBD, no TODO-later, no "similar to Task N",
no blank code blocks. Every step contains the full code the engineer
types. The bring-up stub in Task 13 explicitly flags "replaced by
the future main.cpp integration spec" — that's a documented-in-spec
follow-up, not a planning gap.

**Type consistency:**
- `Rgb` / `Frame` / `LED_COUNT` consistent across all tasks.
- `WordId` enum referenced throughout; verified against
  `firmware/lib/core/include/word_id.h` by the truth-verifier.
- `Palette` enum switch-coverage complete in Task 5 (all 10 values).
- `RenderInput` struct fields used identically in Tasks 7-13.
- `BirthdayConfig` and `BirthdayState` match
  `firmware/lib/core/include/birthday.h` signatures.
- `WordSet` used correctly — `.count` and `.words[]` per
  `firmware/lib/core/include/time_to_words.h`.
- `PIN_LED_DATA` (= 13) consumed in Task 12 adapter from
  `firmware/configs/pinmap.h` (shipped by the buttons plan).
- `wc::brightness(hour, minute)` signature matches
  `firmware/lib/core/include/dim_schedule.h`.
- `wc::palette_for_date(year, month, day)` matches
  `firmware/lib/core/include/holidays.h`.
- `wc::birthday_state(month, day, hour, minute, cfg)` matches
  `firmware/lib/core/include/birthday.h`.
- `wc::time_to_words(hour, minute)` matches
  `firmware/lib/core/include/time_to_words.h`.
- `wc::wifi_provision::seconds_since_last_sync()` + `state()` +
  `State::Online` all match
  `firmware/lib/wifi_provision/include/wifi_provision.h`.
- `PALETTE_MAX_RGB_SUM` (= 700) and `STALE_SYNC_THRESHOLD_S` (= 86'400)
  used consistently across spec + tests + impl.
- `RAINBOW_PERIOD_MS` (= 60'000) same in rainbow header, impl, tests.

**Build filter:** Task 1 adds all four pure-logic `.cpp` paths to
`build_src_filter` up-front, so subsequent tasks that create those
files don't need to re-touch `platformio.ini`. Same pattern buttons +
wifi_provision plans used.

**Final test count:** 114/114 native tests target (79 pre-display +
35 display). Matches the spec's stated target.

Plan is complete.
