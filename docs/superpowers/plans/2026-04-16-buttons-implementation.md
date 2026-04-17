# Buttons Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `buttons` firmware module — debounced press detection on 3 tact switches (Hour / Minute / Audio), Hour+Audio hold-10 s reset-combo detector, single-callback event API, plus a `confirm_audio()` helper on wifi_provision and a wire-up in main.cpp.

**Architecture:** Hexagonal. Two pure-logic modules (`Debouncer`, `ComboDetector`) are native-testable with Unity. ESP32 adapter (`buttons.cpp`) wraps `pinMode`/`digitalRead`/`millis`, is guarded with `#ifdef ARDUINO`, and delegates all decisions to pure logic. Matches the wifi_provision pattern shipped earlier today.

**Tech Stack:** Arduino-ESP32 core (`Arduino.h`, `pinMode`, `digitalRead`, `millis`), PlatformIO build system, Unity C++ test framework for native tests.

---

## Spec reference

This plan implements `docs/superpowers/specs/2026-04-16-buttons-design.md`. Read the spec in full before starting — it pins down every behavioural decision (debounce threshold, combo timing, suppression rules, integration contract).

## File structure

```
firmware/
  configs/
    pinmap.h                      # NEW — shared pin macros (per pinout.md)
  lib/buttons/
    include/
      buttons.h                   # public API (ESP32 only)
      buttons/
        event.h                   # Event enum
        debouncer.h               # pure logic
        combo_detector.h          # pure logic
    src/
      buttons.cpp                 # ESP32 adapter (#ifdef ARDUINO)
      debouncer.cpp               # pure logic
      combo_detector.cpp          # pure logic
  test/
    test_buttons_debouncer/
      test_debouncer.cpp          # native Unity tests
    test_buttons_combo_detector/
      test_combo_detector.cpp     # native Unity tests
    hardware_checks/
      buttons_checks.md           # manual on-hardware verification
  lib/wifi_provision/
    include/
      wifi_provision.h            # MODIFIED — adds confirm_audio()
    src/
      wifi_provision.cpp          # MODIFIED — implements confirm_audio()
  src/
    main.cpp                      # MODIFIED — wires buttons::begin + routing
  platformio.ini                  # MODIFIED — native include + build_src_filter entries
```

---

## Task 1: Shared pinmap.h

**Files:**
- Create: `firmware/configs/pinmap.h`

- [ ] **Step 1: Create the header**

```cpp
// firmware/configs/pinmap.h
//
// Shared GPIO pin assignments for every Phase 2 firmware module.
// Source of truth: docs/hardware/pinout.md §Firmware constants.
//
// The `configs/` directory is on the ESP32 envs' -I include path via
// `[esp32-base] build_flags = -I configs` in platformio.ini, so any
// Arduino-env source can `#include "pinmap.h"`. Native/pure-logic code
// never includes this header — it doesn't touch hardware.
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

// Tact switches (INPUT_PULLUP, other leg to GND)
#define PIN_BUTTON_HOUR   32
#define PIN_BUTTON_MINUTE 33
#define PIN_BUTTON_AUDIO  14
```

- [ ] **Step 2: Sanity-check the ESP32 build still succeeds**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: build succeeds — `pinmap.h` isn't included by anything yet, so this is a non-change to the ESP32 build. Just confirms no syntax errors in the header.

- [ ] **Step 3: Sanity-check native suite**

Run: `cd firmware && pio test -e native`
Expected: 66/66 still green. Native doesn't see this header.

- [ ] **Step 4: Commit**

```bash
git add firmware/configs/pinmap.h
git commit -m "feat(firmware): add shared pinmap.h for phase 2 modules"
```

---

## Task 2: Debouncer scaffold + first test

**Files:**
- Create: `firmware/lib/buttons/include/buttons/debouncer.h`
- Create: `firmware/lib/buttons/src/debouncer.cpp`
- Create: `firmware/test/test_buttons_debouncer/test_debouncer.cpp`
- Modify: `firmware/platformio.ini` — add buttons include path + pure-logic sources to `[env:native]`

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_buttons_debouncer/test_debouncer.cpp
#include <unity.h>
#include "buttons/debouncer.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Raw press held for 25ms must commit on the 25ms tick.
void test_press_commits_after_25ms_stable(void) {
    Debouncer d;
    // Before 25ms the press isn't stable yet — no edge.
    TEST_ASSERT_FALSE(d.step(/* pressed = */ true, /* now_ms = */ 0));
    TEST_ASSERT_FALSE(d.step(true, 10));
    TEST_ASSERT_FALSE(d.step(true, 24));
    // At 25ms of continuous press the edge fires.
    TEST_ASSERT_TRUE(d.step(true, 25));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_press_commits_after_25ms_stable);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_buttons_debouncer`
Expected: compile FAIL with "fatal error: buttons/debouncer.h: No such file or directory"

- [ ] **Step 3: Create the header**

```cpp
// firmware/lib/buttons/include/buttons/debouncer.h
#pragma once

#include <cstdint>

namespace wc::buttons {

// Debounces a single active-low button. Feed raw state (true = pressed).
// Reports a press-down edge once per press (fires on the first stable
// transition from released to pressed).
class Debouncer {
public:
    // Reset to released state. now_ms is used as the initial stability timestamp.
    explicit Debouncer(uint32_t now_ms = 0);

    // Feed current raw reading and timestamp. Returns true if this call
    // corresponds to a just-debounced press-down edge (fires exactly once
    // per press).
    bool step(bool raw_pressed, uint32_t now_ms);

    // Query current debounced state (does not trigger any edge event).
    bool is_pressed() const { return stable_; }

private:
    static constexpr uint32_t STABLE_MS = 25;
    bool stable_ = false;          // last-committed debounced state
    bool candidate_ = false;       // reading currently being measured for stability
    uint32_t candidate_since_ = 0; // millis when candidate first observed
};

} // namespace wc::buttons
```

- [ ] **Step 4: Create the impl**

```cpp
// firmware/lib/buttons/src/debouncer.cpp
#include "buttons/debouncer.h"

namespace wc::buttons {

Debouncer::Debouncer(uint32_t now_ms) {
    candidate_since_ = now_ms;
}

bool Debouncer::step(bool raw_pressed, uint32_t now_ms) {
    if (raw_pressed != candidate_) {
        // Raw state changed — restart the stability window.
        candidate_ = raw_pressed;
        candidate_since_ = now_ms;
        return false;
    }
    // Raw state matches candidate — check if it's been stable long enough.
    if (now_ms - candidate_since_ < STABLE_MS) {
        return false;
    }
    // Stable. Commit if it's a new state; otherwise no change.
    if (candidate_ == stable_) {
        return false;
    }
    const bool was_released = !stable_;
    stable_ = candidate_;
    // Press-down edge: just became pressed after being released.
    return stable_ && was_released;
}

} // namespace wc::buttons
```

- [ ] **Step 5: Wire buttons module into PlatformIO native env**

Edit `firmware/platformio.ini`. Current `[env:native]` section:

```ini
[env:native]
platform = native
test_framework = unity
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
build_src_filter =
    +<../lib/core/src/*>
    +<../lib/wifi_provision/src/state_machine.cpp>
    +<../lib/wifi_provision/src/form_parser.cpp>
    +<../lib/wifi_provision/src/credential_validator.cpp>
    +<../lib/wifi_provision/src/tz_options.cpp>
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

`combo_detector.cpp` is referenced even though Task 4 creates it — PlatformIO tolerates the nonexistent source with a warning; this matches the pattern the wifi_provision plan used.

- [ ] **Step 6: Run test to verify it passes**

Run: `cd firmware && pio test -e native -f test_buttons_debouncer`
Expected: 1 test passes.

- [ ] **Step 7: Run full native suite to confirm no regression**

Run: `cd firmware && pio test -e native`
Expected: 67/67 pass (66 existing + 1 new debouncer test).

- [ ] **Step 8: Commit**

```bash
git add firmware/lib/buttons/ firmware/test/test_buttons_debouncer/ firmware/platformio.ini
git commit -m "feat(firmware): scaffold buttons debouncer"
```

---

## Task 3: Debouncer full coverage

**Files:**
- Modify: `firmware/test/test_buttons_debouncer/test_debouncer.cpp`

The impl from Task 2 is complete — this task adds the remaining 5 spec-required test cases. Expect all 6 to pass; if any fail, the impl has a bug and should be fixed inline.

- [ ] **Step 1: Replace the test file with full coverage**

```cpp
// firmware/test/test_buttons_debouncer/test_debouncer.cpp
#include <unity.h>
#include "buttons/debouncer.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Raw press held for 25ms must commit on the 25ms tick.
void test_press_commits_after_25ms_stable(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_FALSE(d.step(true, 10));
    TEST_ASSERT_FALSE(d.step(true, 24));
    TEST_ASSERT_TRUE(d.step(true, 25));
}

// A bounce (state flip) within the 25ms window does NOT commit a press.
void test_bounce_within_window_suppresses_press(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));    // raw goes pressed
    TEST_ASSERT_FALSE(d.step(false, 10));  // bounces back to released — timer resets
    TEST_ASSERT_FALSE(d.step(true, 20));   // bounces pressed again — timer resets
    TEST_ASSERT_FALSE(d.step(true, 30));   // only 10ms stable — still not committed
    // Commits at 20+25=45 (25ms after the last raw-state change to true).
    TEST_ASSERT_TRUE(d.step(true, 45));
}

// Press-down edge fires exactly once per press.
void test_press_edge_fires_only_once_per_press(void) {
    Debouncer d;
    // Commit the press edge.
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));
    // Still held — no further edges.
    TEST_ASSERT_FALSE(d.step(true, 100));
    TEST_ASSERT_FALSE(d.step(true, 500));
    TEST_ASSERT_FALSE(d.step(true, 1000));
}

// Release does NOT fire a press edge (press edges only, no release edges).
void test_release_does_not_fire_edge(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));     // press committed
    TEST_ASSERT_FALSE(d.step(false, 100));  // raw released
    TEST_ASSERT_FALSE(d.step(false, 125));  // release committed, still no edge
    TEST_ASSERT_FALSE(d.step(false, 200));
}

// After release, a fresh press fires a new edge.
void test_second_press_after_release_fires(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));     // press 1
    TEST_ASSERT_FALSE(d.step(false, 100));
    TEST_ASSERT_FALSE(d.step(false, 125));  // released
    TEST_ASSERT_FALSE(d.step(true, 200));
    TEST_ASSERT_TRUE(d.step(true, 225));    // press 2
}

// is_pressed() reports the current debounced state, not the raw state.
void test_is_pressed_reflects_debounced_state(void) {
    Debouncer d;
    d.step(true, 0);
    TEST_ASSERT_FALSE(d.is_pressed()); // raw true but not yet stable
    d.step(true, 25);
    TEST_ASSERT_TRUE(d.is_pressed());  // now stable pressed
    d.step(false, 100);
    TEST_ASSERT_TRUE(d.is_pressed());  // raw released but not yet stable
    d.step(false, 125);
    TEST_ASSERT_FALSE(d.is_pressed()); // now stable released
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_press_commits_after_25ms_stable);
    RUN_TEST(test_bounce_within_window_suppresses_press);
    RUN_TEST(test_press_edge_fires_only_once_per_press);
    RUN_TEST(test_release_does_not_fire_edge);
    RUN_TEST(test_second_press_after_release_fires);
    RUN_TEST(test_is_pressed_reflects_debounced_state);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests — expect all 6 to pass**

Run: `cd firmware && pio test -e native -f test_buttons_debouncer`
Expected: 6 tests pass. If any fail, the Task 2 impl has a bug — fix in `debouncer.cpp` before committing.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 72/72 pass (66 existing + 6 debouncer).

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_buttons_debouncer/
git commit -m "feat(firmware): complete buttons debouncer coverage"
```

---

## Task 4: ComboDetector scaffold + first test

**Files:**
- Create: `firmware/lib/buttons/include/buttons/combo_detector.h`
- Create: `firmware/lib/buttons/src/combo_detector.cpp`
- Create: `firmware/test/test_buttons_combo_detector/test_combo_detector.cpp`

`combo_detector.cpp` is already in the native `build_src_filter` from Task 2 — no platformio.ini change.

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_buttons_combo_detector/test_combo_detector.cpp
#include <unity.h>
#include "buttons/combo_detector.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Both held for exactly 10000ms fires the combo on that tick.
void test_fires_after_10s_of_both_held(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(/* hour = */ true, /* audio = */ true, /* now = */ 0));
    TEST_ASSERT_FALSE(c.step(true, true, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 9999));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fires_after_10s_of_both_held);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_buttons_combo_detector`
Expected: compile FAIL with "fatal error: buttons/combo_detector.h: No such file or directory"

- [ ] **Step 3: Create the header**

```cpp
// firmware/lib/buttons/include/buttons/combo_detector.h
#pragma once

#include <cstdint>

namespace wc::buttons {

// Detects Hour + Audio held simultaneously for 10s.
// Fires exactly once per continuous hold.
class ComboDetector {
public:
    // Feed the debounced states of Hour and Audio + current time.
    // Returns true on the tick where the 10s threshold is crossed.
    bool step(bool hour_pressed, bool audio_pressed, uint32_t now_ms);

    // True iff BOTH Hour and Audio were debounced-pressed on the most
    // recent step() call. Transitions to false immediately when either
    // button releases, regardless of whether the 10s threshold already
    // fired during the hold. Adapter uses this to suppress the
    // individual HourTick / AudioPressed events for the duration of
    // the both-held window.
    bool in_combo() const;

private:
    static constexpr uint32_t COMBO_MS = 10'000;
    bool armed_ = false;           // both pressed at last step?
    bool fired_ = false;           // already fired this hold?
    uint32_t armed_since_ = 0;
};

} // namespace wc::buttons
```

- [ ] **Step 4: Create the impl**

```cpp
// firmware/lib/buttons/src/combo_detector.cpp
#include "buttons/combo_detector.h"

namespace wc::buttons {

bool ComboDetector::step(bool hour_pressed, bool audio_pressed, uint32_t now_ms) {
    const bool both_pressed = hour_pressed && audio_pressed;

    if (!both_pressed) {
        // Either release cancels + resets.
        armed_ = false;
        fired_ = false;
        return false;
    }

    // Both pressed.
    if (!armed_) {
        armed_ = true;
        fired_ = false;
        armed_since_ = now_ms;
        return false;
    }

    // Still held; don't double-fire.
    if (fired_) {
        return false;
    }

    if (now_ms - armed_since_ >= COMBO_MS) {
        fired_ = true;
        return true;
    }
    return false;
}

bool ComboDetector::in_combo() const {
    return armed_;
}

} // namespace wc::buttons
```

- [ ] **Step 5: Run test to confirm it passes**

Run: `cd firmware && pio test -e native -f test_buttons_combo_detector`
Expected: 1 test passes.

- [ ] **Step 6: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 73/73 pass.

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/buttons/ firmware/test/test_buttons_combo_detector/
git commit -m "feat(firmware): scaffold buttons combo detector"
```

---

## Task 5: ComboDetector full coverage

**Files:**
- Modify: `firmware/test/test_buttons_combo_detector/test_combo_detector.cpp`

- [ ] **Step 1: Replace the test file with full coverage**

```cpp
// firmware/test/test_buttons_combo_detector/test_combo_detector.cpp
#include <unity.h>
#include "buttons/combo_detector.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Both held for exactly 10000ms fires the combo on that tick.
void test_fires_after_10s_of_both_held(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(true, true, 0));
    TEST_ASSERT_FALSE(c.step(true, true, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 9999));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
}

// Fires only once per continuous hold, even if held past 10s.
void test_does_not_refire_while_held(void) {
    ComboDetector c;
    c.step(true, true, 0);
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
    TEST_ASSERT_FALSE(c.step(true, true, 11000));
    TEST_ASSERT_FALSE(c.step(true, true, 15000));
    TEST_ASSERT_FALSE(c.step(true, true, 30000));
}

// Hour released just before 10s cancels the combo.
void test_hour_release_cancels_combo(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 9999);
    // Hour releases at t=9999 — no fire.
    TEST_ASSERT_FALSE(c.step(false, true, 9999));
    // Both re-pressed at t=10000 — timer starts fresh.
    TEST_ASSERT_FALSE(c.step(true, true, 10000));
    // Needs another 10s from the re-press moment.
    TEST_ASSERT_FALSE(c.step(true, true, 19999));
    TEST_ASSERT_TRUE(c.step(true, true, 20000));
}

// Audio released mid-hold cancels the combo (symmetric to Hour).
void test_audio_release_cancels_combo(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 5000);
    TEST_ASSERT_FALSE(c.step(true, false, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 5001));
    TEST_ASSERT_FALSE(c.step(true, true, 15000));
    TEST_ASSERT_TRUE(c.step(true, true, 15001));
}

// Minute held alongside does nothing — combo_detector only sees Hour+Audio.
void test_minute_not_part_of_combo(void) {
    // ComboDetector's API only takes hour & audio state; Minute state
    // never reaches it. This test documents the API contract: the
    // caller (buttons.cpp adapter) must not pass Minute state here.
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(true, true, 0));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
    // (Calling with only hour or only audio would represent a single
    // button — tested in the hour/audio release-cancel cases above.)
}

// in_combo() is true only while both buttons are actively debounced-pressed.
void test_in_combo_tracks_both_pressed(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.in_combo());  // initial
    c.step(true, false, 0);
    TEST_ASSERT_FALSE(c.in_combo());  // only hour
    c.step(true, true, 100);
    TEST_ASSERT_TRUE(c.in_combo());   // both pressed
    c.step(true, true, 5000);
    TEST_ASSERT_TRUE(c.in_combo());   // still both
    c.step(true, false, 5100);
    TEST_ASSERT_FALSE(c.in_combo());  // audio released — immediately false
}

// in_combo() reports false after fire + release.
void test_in_combo_false_after_fire_and_release(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 10000);        // fires
    TEST_ASSERT_TRUE(c.in_combo());   // still both held
    c.step(false, true, 10100);
    TEST_ASSERT_FALSE(c.in_combo());  // released — false immediately
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fires_after_10s_of_both_held);
    RUN_TEST(test_does_not_refire_while_held);
    RUN_TEST(test_hour_release_cancels_combo);
    RUN_TEST(test_audio_release_cancels_combo);
    RUN_TEST(test_minute_not_part_of_combo);
    RUN_TEST(test_in_combo_tracks_both_pressed);
    RUN_TEST(test_in_combo_false_after_fire_and_release);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests — expect all 7 to pass**

Run: `cd firmware && pio test -e native -f test_buttons_combo_detector`
Expected: 7 tests pass.

- [ ] **Step 3: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: 79/79 pass (66 existing + 6 debouncer + 7 combo).

- [ ] **Step 4: Commit**

```bash
git add firmware/test/test_buttons_combo_detector/
git commit -m "feat(firmware): complete buttons combo detector coverage"
```

---

## Task 6: Event enum + public buttons.h

**Files:**
- Create: `firmware/lib/buttons/include/buttons/event.h`
- Create: `firmware/lib/buttons/include/buttons.h`

No tests — these are declarations only. Compile is verified in Task 7 when the adapter includes them.

- [ ] **Step 1: Create the Event enum header**

```cpp
// firmware/lib/buttons/include/buttons/event.h
#pragma once

namespace wc::buttons {

enum class Event {
    HourTick,
    MinuteTick,
    AudioPressed,
    ResetCombo,
};

} // namespace wc::buttons
```

- [ ] **Step 2: Create the public module header**

```cpp
// firmware/lib/buttons/include/buttons.h
//
// ESP32-only public API. Depends on Arduino.h. Include this ONLY in
// translation units that compile under the Arduino toolchain (i.e. the
// emory / nora PlatformIO envs). Native-env tests include the pure-logic
// headers `buttons/debouncer.h` and `buttons/combo_detector.h` instead —
// never this header.
#pragma once

#include <Arduino.h>
#include <functional>
#include "buttons/event.h"

namespace wc::buttons {

using Handler = std::function<void(Event)>;

// Register the pin modes + event callback. Call once from setup().
void begin(Handler on_event);

// Poll GPIOs, advance state machines, invoke callback for each event.
// Call once per main loop tick.
void loop();

} // namespace wc::buttons
```

- [ ] **Step 3: Confirm native suite still passes**

Run: `cd firmware && pio test -e native`
Expected: 79/79. Native doesn't see these headers; the Arduino-only `buttons.h` would break native if pulled in, but nothing pulls it in yet.

- [ ] **Step 4: Commit**

```bash
git add firmware/lib/buttons/include/
git commit -m "feat(firmware): public buttons module API + event enum"
```

---

## Task 7: ESP32 adapter (buttons.cpp)

**Files:**
- Create: `firmware/lib/buttons/src/buttons.cpp`

Wrap in `#ifdef ARDUINO` (same pattern as wifi_provision's adapters — required because PlatformIO LDF pulls library sources into native builds regardless of `build_src_filter`).

- [ ] **Step 1: Create the adapter**

```cpp
// firmware/lib/buttons/src/buttons.cpp
// Guarded with #ifdef ARDUINO — same pattern as wifi_provision's
// adapters. PlatformIO LDF would otherwise compile this TU in the native
// test build where <Arduino.h> doesn't exist.
#ifdef ARDUINO

#include <Arduino.h>
#include "buttons.h"
#include "buttons/debouncer.h"
#include "buttons/combo_detector.h"
#include "buttons/event.h"
#include "pinmap.h"                 // PIN_BUTTON_{HOUR,MINUTE,AUDIO}

namespace wc::buttons {

static Debouncer     db_hour;
static Debouncer     db_min;
static Debouncer     db_audio;
static ComboDetector combo;
static Handler       on_event;

void begin(Handler h) {
    pinMode(PIN_BUTTON_HOUR,   INPUT_PULLUP);
    pinMode(PIN_BUTTON_MINUTE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_AUDIO,  INPUT_PULLUP);
    on_event = std::move(h);
}

void loop() {
    const uint32_t now = millis();
    // Raw reads: LOW = pressed for active-low INPUT_PULLUP buttons.
    const bool h_raw = (digitalRead(PIN_BUTTON_HOUR)   == LOW);
    const bool m_raw = (digitalRead(PIN_BUTTON_MINUTE) == LOW);
    const bool a_raw = (digitalRead(PIN_BUTTON_AUDIO)  == LOW);

    const bool h_edge = db_hour.step(h_raw, now);
    const bool m_edge = db_min.step(m_raw, now);
    const bool a_edge = db_audio.step(a_raw, now);

    const bool combo_fired =
        combo.step(db_hour.is_pressed(), db_audio.is_pressed(), now);

    // Priority order (see spec §Event ordering).
    if (combo_fired)                  on_event(Event::ResetCombo);
    if (h_edge && !combo.in_combo())  on_event(Event::HourTick);
    if (m_edge)                       on_event(Event::MinuteTick);
    if (a_edge && !combo.in_combo())  on_event(Event::AudioPressed);
}

} // namespace wc::buttons

#endif // ARDUINO
```

- [ ] **Step 2: Compile the emory build**

Run: `cd firmware && pio run -e emory --target=checkprogsize`

Expected: emory build SUCCEEDS. `buttons.cpp` may or may not be pulled into the emory build yet (nothing in main.cpp references it) — the important thing is no compile error. If LDF does pull it in, the adapter must link cleanly: `Debouncer` / `ComboDetector` definitions come from the pure-logic `.cpp` files already compiled into the lib.

- [ ] **Step 3: Compile the nora build**

Run: `cd firmware && pio run -e nora --target=checkprogsize`
Expected: SUCCESS.

- [ ] **Step 4: Confirm native suite still green**

Run: `cd firmware && pio test -e native`
Expected: 79/79. The `#ifdef ARDUINO` guard keeps this TU out of native even though pure-logic source files in the same library are compiled.

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/buttons/src/buttons.cpp
git commit -m "feat(firmware): buttons ESP32 adapter + event dispatch"
```

---

## Task 8: wifi_provision::confirm_audio()

**Files:**
- Modify: `firmware/lib/wifi_provision/include/wifi_provision.h` — add declaration
- Modify: `firmware/lib/wifi_provision/src/wifi_provision.cpp` — add implementation

- [ ] **Step 1: Add the declaration to the public header**

Locate this block in `firmware/lib/wifi_provision/include/wifi_provision.h`:

```cpp
// Force a reset back into captive-portal mode. Clears stored credentials.
// Called by the buttons module when Hour + Audio is held for 10 s.
void reset_to_captive();

} // namespace wc::wifi_provision
```

Insert the new declaration BEFORE the namespace close:

```cpp
// Force a reset back into captive-portal mode. Clears stored credentials.
// Called by the buttons module when Hour + Audio is held for 10 s.
void reset_to_captive();

// Fire Event::AudioButtonConfirmed into the state machine.
// Caller: the buttons module when Audio is pressed during captive-portal
// AwaitingConfirmation state.
void confirm_audio();

} // namespace wc::wifi_provision
```

- [ ] **Step 2: Add the implementation to wifi_provision.cpp**

Locate the existing `reset_to_captive()` implementation near the bottom of `firmware/lib/wifi_provision/src/wifi_provision.cpp` (inside the `#ifdef ARDUINO` guard, inside `namespace wc::wifi_provision {`). It looks like:

```cpp
void reset_to_captive() {
    nvs_store::clear();
    sm.handle(Event::ResetCombo);
    start_ap();
}
```

Add the new function DIRECTLY BELOW it (still inside the same namespace + guard):

```cpp
void confirm_audio() {
    sm.handle(Event::AudioButtonConfirmed);
}
```

- [ ] **Step 3: Compile both ESP32 envs**

```bash
cd firmware
pio run -e emory --target=checkprogsize
pio run -e nora --target=checkprogsize
```
Expected: both succeed.

- [ ] **Step 4: Confirm native suite still green**

Run: `cd firmware && pio test -e native`
Expected: 79/79.

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/wifi_provision/
git commit -m "feat(firmware): add confirm_audio helper to wifi_provision"
```

---

## Task 9: Integrate buttons into main.cpp

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace main.cpp**

Current `firmware/src/main.cpp`:

```cpp
#include <Arduino.h>
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    wc::wifi_provision::begin();
}

void loop() {
    wc::wifi_provision::loop();
    delay(1);  // yield to the IDLE task so watchdog + WiFi stacks run
}
```

Replace with:

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

Serial logs for Hour/Minute/Audio outside captive are temporary — they make the buttons observable during hardware bring-up until the rtc and audio modules land (future Phase 2 tasks will replace the `Serial.println` calls with `rtc::advance_*` / `audio::toggle_lullaby`).

- [ ] **Step 2: Compile both ESP32 envs**

```bash
cd firmware
pio run -e emory --target=checkprogsize
pio run -e nora --target=checkprogsize
```

Expected: both succeed. Flash usage will grow slightly from Task 8's measurement — buttons adapter + 3 Debouncers + ComboDetector + the callback closure adds a few KB.

- [ ] **Step 3: Native suite still green**

Run: `cd firmware && pio test -e native`
Expected: 79/79.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): wire buttons into main + event routing"
```

---

## Task 10: Hardware manual-verification checklist

**Files:**
- Create: `firmware/test/hardware_checks/buttons_checks.md`

Not code — a handoff doc for running the real-hardware tests once parts arrive and the breadboard bring-up has reached Step 6 (buttons wiring) in `docs/hardware/breadboard-bring-up-guide.md`.

- [ ] **Step 1: Write the checklist**

```markdown
<!-- firmware/test/hardware_checks/buttons_checks.md -->
# buttons — manual hardware verification

Run these with the breadboard wired per
`docs/hardware/breadboard-bring-up-guide.md` §Step 6. Three tact
switches: Hour → GPIO 32, Minute → GPIO 33, Audio → GPIO 14. Each
switch has one leg on its GPIO and the diagonal-opposite leg on GND.
No external resistors — the ESP32's INPUT_PULLUP provides the pullup.

## 1. Each button produces its expected serial log

- [ ] Flash: `pio run -e emory -t upload`
- [ ] Open serial monitor: `pio device monitor -e emory`
- [ ] Boot completes; `word-clock booting for target: Emory` appears.
- [ ] Press Hour → serial shows `[buttons] HourTick (rtc module not yet wired)`.
- [ ] Press Minute → serial shows `[buttons] MinuteTick (rtc module not yet wired)`.
- [ ] Press Audio (outside captive portal) → serial shows `[buttons] AudioPressed (audio module not yet wired)`.

## 2. Debounce behaves

- [ ] Rapid-tap any one button 5 times — serial shows exactly 5 events, no spurious extras or swallows.
- [ ] Press and hold any one button for 3 s — serial shows exactly 1 event (fires on press-down, not repeatedly while held).
- [ ] Release the held button — no event fires on release.

## 3. Reset combo fires at 10 s

- [ ] With the clock running normally (post-wifi), hold Hour + Audio simultaneously.
- [ ] After 10 s, serial shows `[buttons] ResetCombo — resetting to captive portal`.
- [ ] The clock re-enters AP mode: `WordClock-Setup-XXXX` reappears on your phone's WiFi list.
- [ ] Stored credentials are cleared (you'll need to re-submit the form to reconnect).

## 4. Reset combo cancels on early release

- [ ] Hold Hour + Audio for ~5 s (less than 10 s), then release Hour while keeping Audio held.
- [ ] No ResetCombo event fires.
- [ ] Release Audio. No delayed event. No AudioPressed event fires (suppressed during the combo hold + no press-down edge on release).
- [ ] Re-press Hour + Audio and hold a fresh 10 s — combo fires normally (timer reset).

## 5. Audio during AwaitingConfirmation confirms credentials

- [ ] Trigger reset combo (step 3) to get back to captive portal.
- [ ] Connect phone, submit valid WiFi credentials via the form.
- [ ] Phone page shows "Press the Audio button on the clock".
- [ ] Press Audio within 60 s → clock transitions to Validating → Online.
- [ ] Serial does NOT show `[buttons] AudioPressed (audio module not yet wired)` — confirming the state-routing switched to `confirm_audio()` instead of the default log.

## 6. Minute during combo is unaffected

- [ ] Hold Hour + Audio + Minute simultaneously for 10 s.
- [ ] ResetCombo fires (as in check 3).
- [ ] During the 10 s hold, pressing Minute registers as normal MinuteTicks — combo suppression is Hour/Audio only.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/buttons_checks.md
git commit -m "docs(firmware): manual hardware checks for buttons module"
```

---

## Self-review

**Spec coverage:**
- Pin assignments → Task 1 (pinmap.h) + Task 7 (adapter uses PIN_BUTTON_*). ✓
- Debounce (25 ms stable, timer reset on raw change, press-edge-only) → Task 2 (impl) + Task 3 (6 tests). ✓
- Reset combo (10 s, single-fire, cancel-on-release) → Task 4 (impl) + Task 5 (7 tests). ✓
- `in_combo()` returns false immediately on release → Task 4 impl + Task 5 `test_in_combo_tracks_both_pressed` + `test_in_combo_false_after_fire_and_release`. ✓
- Suppression of HourTick/AudioPressed during `in_combo()` → Task 7 adapter code. ✓
- Press-edge + suppression coupling invariant → Task 7 adapter preserves press-down-only semantics via `Debouncer::step` return + `in_combo()` guard. ✓
- Event ordering `ResetCombo → HourTick → MinuteTick → AudioPressed` → Task 7 adapter's if-sequence. ✓
- `confirm_audio()` on wifi_provision → Task 8. ✓
- main.cpp routing based on `wifi_provision::state()` → Task 9. ✓
- Native-testable pure logic, `#ifdef ARDUINO` guard on adapter → Tasks 2/4 (pure logic) + Task 7 (guard). ✓
- Hardware checklist → Task 10. ✓
- Minute not part of combo — documented in Task 5 `test_minute_not_part_of_combo` + Task 10 check 6. ✓

**Placeholder scan:** no TBD, no TODO-later, no "similar to Task N". Every code block is complete.

**Type consistency:**
- `Debouncer::step(bool, uint32_t) → bool` consistent across Tasks 2, 3, 7.
- `Debouncer::is_pressed() → bool` consistent across Tasks 2, 3, 7.
- `ComboDetector::step(bool, bool, uint32_t) → bool` consistent across Tasks 4, 5, 7.
- `ComboDetector::in_combo() → bool` consistent across Tasks 4, 5, 7.
- `Event` enum values used in Task 7 adapter match Task 6 header (HourTick, MinuteTick, AudioPressed, ResetCombo).
- `Handler = std::function<void(Event)>` signature consistent between Tasks 6 and 9.
- `wc::wifi_provision::State` enum value `AwaitingConfirmation` referenced in Task 9 matches what `state_machine.h` shipped in the captive-portal plan Task 1.
- `wc::wifi_provision::Event::AudioButtonConfirmed` referenced in Task 8 matches the captive-portal state machine's Event enum.

**Pin constants:** `PIN_BUTTON_HOUR` / `PIN_BUTTON_MINUTE` / `PIN_BUTTON_AUDIO` introduced in Task 1 `pinmap.h`, consumed in Task 7 `buttons.cpp`. Names match exactly.

**Build filter:** Task 2 adds both `debouncer.cpp` AND `combo_detector.cpp` to `build_src_filter` up-front (combo_detector doesn't exist until Task 4 but the filter tolerates missing sources, matching the wifi_provision plan's precedent).

Plan is complete.
