# Buttons Firmware Module — Design Spec

Date: 2026-04-16

## Overview

The `buttons` module handles input from the three tact switches on the
back panel (Hour-set, Minute-set, Audio). It debounces each switch,
detects the Hour + Audio reset combo, and notifies the rest of the
firmware via a single event callback.

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
(buttons are listed as a Phase 2 firmware module in TODO.md).

## Scope

**In:**
- Debounced press-detection for all three buttons (GPIO 14 / 32 / 33)
- Hour + Audio reset-combo detection (10 s simultaneous hold)
- Single-callback event API the rest of the firmware consumes
- Native-testable pure-logic modules (debouncer + combo detector)
- One addition to `wifi_provision.h`: `confirm_audio()` helper
- `#ifdef ARDUINO` guard on the ESP32 adapter (same pattern as wifi_provision's adapters)

**Out:**
- Routing of the events to other modules — lives in `main.cpp`, not here
- The audio module itself (plays / stops lullaby)
- The RTC advance-hour / advance-minute functions (live in the rtc module)
- Long-press auto-repeat for Hour/Minute (explicitly declined — one press = one tick)
- LED visual feedback during the reset-combo hold (display-module concern, out of scope)

## Behavioral Contract

### Button semantics

| Button | GPIO | Wiring | Behavior |
|---|---|---|---|
| Hour-set | 32 | INPUT_PULLUP, other leg → GND | Press → fire `HourTick` event exactly once |
| Minute-set | 33 | INPUT_PULLUP, other leg → GND | Press → fire `MinuteTick` event exactly once |
| Audio play/stop | 14 | INPUT_PULLUP, other leg → GND | Press → fire `AudioPressed` event exactly once |

All three buttons are active-low: `digitalRead()` returns LOW when pressed,
HIGH when released. No external pull resistors — ESP32's internal pullups
are enabled via `INPUT_PULLUP`.

Events fire on the **press-down transition** (after debounce), not on release.

### Debounce

- Raw GPIO reads can bounce for ~5–10 ms on mechanical contacts.
- The debouncer requires the raw state to be **stable for 25 ms** before
  committing to a new debounced state.
- The stability timer **resets to zero on every raw-state change**. A
  long bounce sequence that keeps flipping the raw reading extends the
  stabilization window; only a continuous 25 ms of unchanged raw state
  commits the new debounced value.
- A bounce on release does NOT retrigger the press event.

### Reset combo

- Fires `ResetCombo` event when Hour AND Audio are both debounced-pressed
  simultaneously for **10 000 ms** continuous.
- Event fires **once** — subsequent time held past 10 s does not repeat
  the event.
- Either release (Hour or Audio transitioning to released) cancels and
  resets the combo timer. A fresh re-press starts over.
- Minute is NOT part of the combo — holding Minute while Hour+Audio are
  held has no effect on combo detection, and Minute press events fire
  normally even during a combo hold.
- **Suppression:** while Hour AND Audio are both debounced-pressed (i.e.
  `combo_detector::in_combo()` returns true), any Hour press-down edge
  or Audio press-down edge detected in that window is suppressed and
  does NOT fire `HourTick` / `AudioPressed`. Rationale: the user is
  clearly attempting a reset, not a time adjustment or audio toggle.
  This applies from the moment both are down, not just after the 10 s
  threshold has elapsed.
- **Suppression ends on release.** `in_combo()` must return `false`
  immediately when either Hour or Audio transitions to released,
  regardless of whether the 10 s threshold had fired. This is a
  correctness invariant (see `ComboDetector` header contract below).
- **Press-edge + suppression coupling — correctness invariant.**
  `HourTick` / `AudioPressed` fire on press-down edges ONLY (not on
  release). Combined with the suppression rule above, this means:
  once a combo attempt is in progress and the user releases Hour
  mid-hold while still holding Audio, no spurious `AudioPressed`
  fires on that release — there is no new press-down edge in that
  tick. Do not change `Debouncer::step` to fire on other edges
  without revisiting this rule; the two are load-bearing together.

### Event ordering

All events are emitted synchronously from `buttons::loop()` via the
registered callback. If multiple events could logically fire in the same
`loop()` call, the callback fires once per event in this priority order:
`ResetCombo` → `HourTick` → `MinuteTick` → `AudioPressed`. In practice,
human press timing plus the 10 s combo threshold makes concurrent events
exceedingly rare; order is specified only to avoid ambiguity.

## Architecture

Hexagonal. Pure-logic modules are Unity-testable on the native env; the
ESP32 adapter wraps `digitalRead()` and `millis()`.

```
firmware/lib/buttons/
  include/
    buttons.h                  public module API (ESP32 only)
    buttons/
      event.h                  Event enum (pure, headers-only)
      debouncer.h              per-button state machine
      combo_detector.h         Hour+Audio hold-10s state machine
  src/
    buttons.cpp                adapter — pinMode, digitalRead, millis
                               (guarded with #ifdef ARDUINO)
    debouncer.cpp              pure logic
    combo_detector.cpp         pure logic
```

Tests land at:
```
firmware/test/
  test_buttons_debouncer/test_debouncer.cpp
  test_buttons_combo_detector/test_combo_detector.cpp
```

Pure-logic `.cpp` files get added to the `[env:native]` `build_src_filter`
in `platformio.ini` (same pattern wifi_provision's files use).

### New shared file: `firmware/configs/pinmap.h`

`docs/hardware/pinout.md` §Firmware constants specifies that Phase 2
modules share a `firmware/configs/pinmap.h` with `PIN_*` macros. This is
the first Phase 2 module to need it, so creating `pinmap.h` lands with
this module. Content per pinout.md:

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

// Tact switches (INPUT_PULLUP, other leg to GND)
#define PIN_BUTTON_HOUR   32
#define PIN_BUTTON_MINUTE 33
#define PIN_BUTTON_AUDIO  14
```

`configs/` is already on the ESP32 envs' `-I` include path via
`[esp32-base] build_flags = -I configs`, so `#include "pinmap.h"` works
from any module. Native tests for pure-logic modules never see this
header (no pin reads happen in pure logic).

## Public API

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

### Pure-logic headers

```cpp
// firmware/lib/buttons/include/buttons/debouncer.h
#pragma once

#include <cstdint>

namespace wc::buttons {

// Debounces a single active-low button. Feed raw state (HIGH=released,
// LOW=pressed in Arduino terms → we normalize to bool pressed).
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

```cpp
// firmware/lib/buttons/include/buttons/combo_detector.h
#pragma once

#include <cstdint>

namespace wc::buttons {

// Detects Hour + Audio held simultaneously for 10 s.
// Fires exactly once per continuous hold.
class ComboDetector {
public:
    // Feed the debounced states of Hour and Audio + current time.
    // Returns true on the tick where the 10 s threshold is crossed.
    bool step(bool hour_pressed, bool audio_pressed, uint32_t now_ms);

    // True iff BOTH Hour and Audio were debounced-pressed on the most
    // recent step() call. Transitions to false immediately when either
    // button releases, regardless of whether the 10 s threshold already
    // fired during the hold. Adapter uses this to suppress the
    // individual HourTick / AudioPressed events for the duration of the
    // both-held window.
    bool in_combo() const;

private:
    static constexpr uint32_t COMBO_MS = 10'000;
    bool armed_ = false;           // both pressed at last step?
    bool fired_ = false;           // already fired this hold?
    uint32_t armed_since_ = 0;
};

} // namespace wc::buttons
```

## Adapter (buttons.cpp) — pseudocode

Actual code written during implementation; shape for review:

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#include "buttons.h"
#include "buttons/debouncer.h"
#include "buttons/combo_detector.h"
#include "buttons/event.h"
#include "pinmap.h"             // PIN_BUTTON_{HOUR,MINUTE,AUDIO}

namespace wc::buttons {
    static Debouncer      db_hour, db_min, db_audio;
    static ComboDetector  combo;
    static Handler        on_event;

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

        // Priority order (see spec Behavioral Contract / Event ordering).
        if (combo_fired)                               on_event(Event::ResetCombo);
        if (h_edge && !combo.in_combo())               on_event(Event::HourTick);
        if (m_edge)                                    on_event(Event::MinuteTick);
        if (a_edge && !combo.in_combo())               on_event(Event::AudioPressed);
    }
}
#endif
```

## Integration with wifi_provision

Addition to `wifi_provision.h`:

```cpp
// Fires Event::AudioButtonConfirmed into the state machine.
// Caller: the buttons module when Audio is pressed during captive-portal
// AwaitingConfirmation state.
void confirm_audio();
```

Implementation in `wifi_provision.cpp` (inside the `#ifdef ARDUINO` guard):

```cpp
void confirm_audio() {
    sm.handle(Event::AudioButtonConfirmed);
}
```

## main.cpp routing

```cpp
wc::buttons::begin([](wc::buttons::Event e) {
    using BE = wc::buttons::Event;
    using WS = wc::wifi_provision::State;
    switch (e) {
        case BE::HourTick:    /* rtc::advance_hour();   */ break;
        case BE::MinuteTick:  /* rtc::advance_minute(); */ break;
        case BE::AudioPressed:
            if (wc::wifi_provision::state() == WS::AwaitingConfirmation)
                wc::wifi_provision::confirm_audio();
            else                                          /* audio::toggle_lullaby(); */
                break;
        case BE::ResetCombo:  wc::wifi_provision::reset_to_captive(); break;
    }
});
```

`rtc::advance_hour`, `rtc::advance_minute`, `audio::toggle_lullaby` are
stubs — those modules land in subsequent Phase 2 tasks. The buttons module
doesn't depend on them; main.cpp's switch just ignores the events until
those modules are wired up.

## Testing

### Pure-logic native tests

**debouncer tests** (must cover):
- First stable state committed after 25 ms — earlier reads ignored.
- Bounce within 25 ms window does not trigger a press.
- Press-down edge fires exactly once per press (not repeatedly while held).
- Release does NOT trigger a press edge.
- A second press after release does trigger again.
- Stability timer resets on every raw-state change.

**combo_detector tests** (must cover):
- Fires after exactly 10 000 ms of both-held.
- Does NOT fire before 10 000 ms.
- Fires only once per continuous hold (no double-fire if held 15 s).
- Hour released at 9 999 ms → combo does not fire; re-pressed → timer restarts from zero.
- Audio released mid-hold → same cancel behavior as Hour.
- Minute held alongside does not affect combo.
- `in_combo()` returns true while both held, false immediately on any release.

### ESP32-only (manual hardware check)

Not native-testable. Goes in
`firmware/test/hardware_checks/buttons_checks.md` alongside the
wifi_provision hardware checklist. 5 checks:
1. Pressing Hour increments the clock by one hour.
2. Pressing Minute increments by one minute.
3. Pressing Audio during normal operation toggles the lullaby (skipped
   until audio module lands).
4. Holding Hour + Audio for ≥10 s triggers reset to captive portal
   (AP `WordClock-Setup-XXXX` reappears).
5. Releasing either button before 10 s and re-pressing restarts the
   combo from 0.

## Open issues

None. All constants locked, all behavior specified, all integration
points identified.

## Verified numbers

| Item | Value | Source |
|---|---|---|
| GPIO pin for Hour | 32 | `docs/hardware/pinout.md` §Signal assignments |
| GPIO pin for Minute | 33 | `docs/hardware/pinout.md` §Signal assignments |
| GPIO pin for Audio | 14 | `docs/hardware/pinout.md` §Signal assignments |
| Wiring convention | INPUT_PULLUP, other leg → GND | `docs/hardware/pinout.md` §Signal assignments |
| Debounce threshold | 25 ms | Common tact-switch bounce is 5–10 ms; 25 ms leaves headroom without feeling sluggish |
| Reset-combo threshold | 10 000 ms | `firmware/lib/wifi_provision/include/wifi_provision.h` (`reset_to_captive` comment) |
| Audio button semantics | toggle lullaby / confirm WiFi depending on mode | `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §Audio button + `docs/superpowers/specs/2026-04-16-captive-portal-design.md` |

## References

- Parent design: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
- Pinout: `docs/hardware/pinout.md`
- Sibling module: `docs/superpowers/specs/2026-04-16-captive-portal-design.md`
  (captive portal defines the `reset_to_captive()` + `AudioButtonConfirmed` contract)
- Breadboard smoke test for this module: `docs/hardware/breadboard-bring-up-guide.md` §Step 6
