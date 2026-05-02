# Audio Playlist + Birthday-Interrupts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace single-file lullaby playback with a fixed two-song playlist (`lullaby1.wav` → `lullaby2.wav`), and change birthday auto-fire to interrupt an in-progress lullaby instead of being suppressed for the year.

**Architecture:** Extract a pure-logic playback state-machine into `audio/playback.h` (testable on the native PlatformIO env) and wire it into the existing ESP32 adapter (`audio.cpp`). Preserve the NVS-stamp-first sequencing that gives `birth.wav` its fire-once-per-year semantics.

**Tech Stack:** C++17, PlatformIO (espressif32@6.8.0 + native), Unity test framework, Arduino-ESP32 SD / I²S / NVS APIs.

**Spec:** `docs/superpowers/specs/2026-05-02-audio-playlist-design.md`

---

## File Structure

| Path | Status | Responsibility |
|------|--------|----------------|
| `firmware/lib/audio/include/audio/playback.h` | **NEW** | Pure-logic types (`State`, `Track`, `PlaybackEvent`, `PlaybackTransition`) + `next_transition()` signature + path constants. No Arduino includes. |
| `firmware/lib/audio/src/playback.cpp` | **NEW** | Implementation of `next_transition()`. Pure C++, no platform deps. |
| `firmware/test/test_audio_playback/test_playback.cpp` | **NEW** | Unity tests covering all 9 rows of the transition table from the spec. |
| `firmware/platformio.ini` | **MODIFIED** | Add `playback.cpp` to `[env:native].build_src_filter` so native tests link it. |
| `firmware/lib/audio/src/audio.cpp` | **MODIFIED** | Replace direct file-path constants and ad-hoc state transitions with calls to `next_transition()`. Move auto-fire out of "Idle only" branch. Add `track_` static variable. |
| `firmware/test/hardware_checks/audio_checks.md` | **MODIFIED** | Add 4 manual test scenarios for the new playlist + birthday-interrupts behavior. |
| `firmware/lib/audio/include/audio.h` | **UNCHANGED** | Public API shape preserved (semantics of `play_lullaby()` change but signature is identical). |
| `firmware/lib/audio/include/audio/fire_guard.h` | **UNCHANGED** | `should_auto_fire()` logic unchanged. |
| `firmware/lib/audio/src/fire_guard.cpp` | **UNCHANGED** | Same. |
| `firmware/src/main.cpp` | **UNCHANGED** | Button handler keeps calling `audio::play_lullaby()` and `audio::stop()`. |
| `docs/superpowers/specs/2026-04-18-audio-design.md` | **MODIFIED** | Add a "Superseded by" pointer at the top to the new spec. |

---

## Tasks

### Task 1: Scaffold pure-logic playback module

Create the header, the stub implementation, the new test directory, and update `platformio.ini` so the test binary links. End state: one test passes, covering `Idle + PlayLullabyRequested → OpenFile lullaby1.wav, Playing/LullabyOne`.

**Files:**
- Create: `firmware/lib/audio/include/audio/playback.h`
- Create: `firmware/lib/audio/src/playback.cpp`
- Create: `firmware/test/test_audio_playback/test_playback.cpp`
- Modify: `firmware/platformio.ini`

- [ ] **Step 1.1: Create the playback header**

```cpp
// firmware/lib/audio/include/audio/playback.h
//
// Pure-logic playback state machine. No Arduino, no SD/I²S/NVS reads —
// the adapter calls next_transition() with the current state + an event,
// and executes the returned action. Compiled into both the ESP32 adapter
// and the native test binary.
//
// Spec: docs/superpowers/specs/2026-05-02-audio-playlist-design.md
#pragma once

#include <cstdint>

namespace wc::audio {

enum class State : uint8_t { Idle, Playing };
enum class Track : uint8_t { None, LullabyOne, LullabyTwo, Birth };

struct PlaybackEvent {
    enum class Kind : uint8_t {
        PlayLullabyRequested,
        StopRequested,
        FileEnded,
        BirthdayFired,
    } kind;
};

struct PlaybackTransition {
    enum class Action : uint8_t {
        None,         // no I/O; state and track may still change (e.g. Idle stop)
        OpenFile,     // open `path`, start I²S
        CloseFile,    // stop I²S, close current file
        SwitchFile,   // close current, open `path`
    } action;
    const char* path;     // valid when action is OpenFile or SwitchFile; nullptr otherwise
    State next_state;
    Track next_track;
};

// SD card paths. Static const string literals — safe to return as
// const char* from next_transition() without lifetime concerns.
inline constexpr const char* kLullabyOnePath = "/lullaby1.wav";
inline constexpr const char* kLullabyTwoPath = "/lullaby2.wav";
inline constexpr const char* kBirthPath      = "/birth.wav";

// Pure. Returns the transition to execute for the given event in the
// given state. The caller (ESP32 adapter or test) is responsible for
// performing the I/O and updating its state to the returned values.
PlaybackTransition next_transition(State state, Track track, PlaybackEvent event);

} // namespace wc::audio
```

- [ ] **Step 1.2: Create the stub implementation**

```cpp
// firmware/lib/audio/src/playback.cpp
//
// Pure-logic playback state machine. See audio/playback.h.
#include "audio/playback.h"

namespace wc::audio {

PlaybackTransition next_transition(State state, Track track, PlaybackEvent event) {
    // Stub: returns "no-op, stay in current state". Will be filled in
    // case-by-case across Tasks 1-5 as tests get added.
    (void)event;
    return {PlaybackTransition::Action::None, nullptr, state, track};
}

} // namespace wc::audio
```

- [ ] **Step 1.3: Update platformio.ini to compile playback.cpp into the native build**

Find the `[env:native]` section and the `build_src_filter` block. After the line `+<../lib/audio/src/fire_guard.cpp>`, add a new line for `playback.cpp`:

```ini
build_src_filter =
    +<../lib/core/src/*>
    ...existing entries...
    +<../lib/audio/src/wav.cpp>
    +<../lib/audio/src/gain.cpp>
    +<../lib/audio/src/fire_guard.cpp>
    +<../lib/audio/src/playback.cpp>
```

- [ ] **Step 1.4: Create the test directory + first failing test**

```cpp
// firmware/test/test_audio_playback/test_playback.cpp
#include <unity.h>
#include <cstring>
#include "audio/playback.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

static PlaybackEvent make_event(PlaybackEvent::Kind k) {
    return PlaybackEvent{k};
}

void test_idle_play_lullaby_opens_lullaby1(void) {
    PlaybackTransition t = next_transition(
        State::Idle, Track::None,
        make_event(PlaybackEvent::Kind::PlayLullabyRequested));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::OpenFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NOT_NULL(t.path);
    TEST_ASSERT_EQUAL_STRING("/lullaby1.wav", t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::LullabyOne), static_cast<int>(t.next_track));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_play_lullaby_opens_lullaby1);
    return UNITY_END();
}
```

- [ ] **Step 1.5: Run the new test and verify it fails**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: 1 test fails. The stub returns `Action::None` so the assertion `Action::OpenFile` mismatches. Rest of the test suite still passes.

- [ ] **Step 1.6: Implement the `Idle + PlayLullabyRequested` case**

Replace the body of `next_transition` in `firmware/lib/audio/src/playback.cpp`. Each event case gets its own block (no fall-through grouping) so subsequent tasks can replace each block independently:

```cpp
PlaybackTransition next_transition(State state, Track track, PlaybackEvent event) {
    using K = PlaybackEvent::Kind;
    using A = PlaybackTransition::Action;

    switch (event.kind) {
    case K::PlayLullabyRequested:
        if (state == State::Idle) {
            return {A::OpenFile, kLullabyOnePath, State::Playing, Track::LullabyOne};
        }
        return {A::None, nullptr, state, track};

    case K::StopRequested:
        // To be implemented in Task 3.
        return {A::None, nullptr, state, track};

    case K::FileEnded:
        // To be implemented in Task 2.
        return {A::None, nullptr, state, track};

    case K::BirthdayFired:
        // To be implemented in Task 4.
        return {A::None, nullptr, state, track};
    }
    return {A::None, nullptr, state, track};
}
```

- [ ] **Step 1.7: Run the test and verify it passes**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: `1 Tests 0 Failures 0 Ignored`.

- [ ] **Step 1.8: Run the full native test suite to confirm no regressions**

```bash
~/Library/Python/3.9/bin/pio test -e native --verbose
```

Expected: all existing tests still pass. Should see something like `174 Tests 0 Failures 0 Ignored` (was 173 before, +1 new).

- [ ] **Step 1.9: Commit**

```bash
git add firmware/lib/audio/include/audio/playback.h \
        firmware/lib/audio/src/playback.cpp \
        firmware/test/test_audio_playback/test_playback.cpp \
        firmware/platformio.ini
git commit -m "$(cat <<'EOF'
feat(audio): scaffold playback state machine module

Pure-logic next_transition() function with the first transition case
(Idle + PlayLullabyRequested -> OpenFile lullaby1.wav) implemented and
tested. Stub returns the right state for the remaining 8 transition-
table rows; subsequent commits fill them in.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Implement FileEnded transitions

Three rows of the transition table: lullaby1 EOF advances to lullaby2; lullaby2 EOF terminates; birth EOF terminates.

**Files:**
- Modify: `firmware/lib/audio/src/playback.cpp`
- Test: `firmware/test/test_audio_playback/test_playback.cpp`

- [ ] **Step 2.1: Add the three failing tests**

Insert after `test_idle_play_lullaby_opens_lullaby1`, before `main(...)`:

```cpp
void test_lullaby1_end_switches_to_lullaby2(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::LullabyOne,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::SwitchFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_EQUAL_STRING("/lullaby2.wav", t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::LullabyTwo), static_cast<int>(t.next_track));
}

void test_lullaby2_end_closes_to_idle(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::LullabyTwo,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NULL(t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}

void test_birth_end_closes_to_idle(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::Birth,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NULL(t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}
```

Also add the `RUN_TEST(...)` lines inside `main()`:

```cpp
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_play_lullaby_opens_lullaby1);
    RUN_TEST(test_lullaby1_end_switches_to_lullaby2);
    RUN_TEST(test_lullaby2_end_closes_to_idle);
    RUN_TEST(test_birth_end_closes_to_idle);
    return UNITY_END();
}
```

- [ ] **Step 2.2: Verify the new tests fail**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: 1 pass, 3 fail. The three new tests fail because `K::FileEnded` falls into the placeholder `return {None, nullptr, state, track}`.

- [ ] **Step 2.3: Implement the FileEnded cases**

In `firmware/lib/audio/src/playback.cpp`, replace the `case K::FileEnded:` block:

```cpp
    case K::FileEnded:
        if (state == State::Playing && track == Track::LullabyOne) {
            return {A::SwitchFile, kLullabyTwoPath, State::Playing, Track::LullabyTwo};
        }
        // Playing/LullabyTwo, Playing/Birth, or anomalous (Idle/None) — close and idle.
        return {A::CloseFile, nullptr, State::Idle, Track::None};
```

- [ ] **Step 2.4: Verify all 4 tests pass**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: `4 Tests 0 Failures 0 Ignored`.

- [ ] **Step 2.5: Commit**

```bash
git add firmware/lib/audio/src/playback.cpp \
        firmware/test/test_audio_playback/test_playback.cpp
git commit -m "$(cat <<'EOF'
feat(audio): implement FileEnded transitions

lullaby1 EOF -> SwitchFile lullaby2.wav (Playing/LullabyTwo);
lullaby2 EOF -> CloseFile (Idle/None);
birth EOF -> CloseFile (Idle/None) — birth does not auto-resume
the lullaby that was interrupted, per spec design decision 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Implement StopRequested transitions

Two rows: stop while Playing closes the file and goes Idle; stop while already Idle is a no-op.

**Files:**
- Modify: `firmware/lib/audio/src/playback.cpp`
- Test: `firmware/test/test_audio_playback/test_playback.cpp`

- [ ] **Step 3.1: Add the two failing tests**

Insert before `main()`:

```cpp
void test_playing_stop_closes_to_idle(void) {
    // Test for each Track value to confirm the transition is uniform.
    for (Track t_in : {Track::LullabyOne, Track::LullabyTwo, Track::Birth}) {
        PlaybackTransition t = next_transition(
            State::Playing, t_in,
            make_event(PlaybackEvent::Kind::StopRequested));
        TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                          static_cast<int>(t.action));
        TEST_ASSERT_NULL(t.path);
        TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
        TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
    }
}

void test_idle_stop_is_noop(void) {
    PlaybackTransition t = next_transition(
        State::Idle, Track::None,
        make_event(PlaybackEvent::Kind::StopRequested));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::None),
                      static_cast<int>(t.action));
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}
```

Add `RUN_TEST` calls in `main()`:

```cpp
    RUN_TEST(test_playing_stop_closes_to_idle);
    RUN_TEST(test_idle_stop_is_noop);
```

- [ ] **Step 3.2: Verify failing**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: 4 pass, 2 fail.

- [ ] **Step 3.3: Implement the StopRequested cases**

In `firmware/lib/audio/src/playback.cpp`, replace the `case K::StopRequested:` block:

```cpp
    case K::StopRequested:
        if (state == State::Playing) {
            return {A::CloseFile, nullptr, State::Idle, Track::None};
        }
        return {A::None, nullptr, State::Idle, Track::None};
```

- [ ] **Step 3.4: Verify all 6 tests pass**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: `6 Tests 0 Failures 0 Ignored`.

- [ ] **Step 3.5: Commit**

```bash
git add firmware/lib/audio/src/playback.cpp \
        firmware/test/test_audio_playback/test_playback.cpp
git commit -m "$(cat <<'EOF'
feat(audio): implement StopRequested transitions

Playing/* + Stop -> CloseFile, Idle/None.
Idle + Stop -> None (defensive no-op; main.cpp already gates on
is_playing() but the state machine handles the case anyway).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Implement BirthdayFired transitions

Two rows: birthday from Idle opens birth.wav; birthday from Playing/* switches to birth.wav (interrupting the lullaby).

**Files:**
- Modify: `firmware/lib/audio/src/playback.cpp`
- Test: `firmware/test/test_audio_playback/test_playback.cpp`

- [ ] **Step 4.1: Add the failing tests**

Insert before `main()`:

```cpp
void test_idle_birthday_opens_birth(void) {
    PlaybackTransition t = next_transition(
        State::Idle, Track::None,
        make_event(PlaybackEvent::Kind::BirthdayFired));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::OpenFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_EQUAL_STRING("/birth.wav", t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::Birth), static_cast<int>(t.next_track));
}

void test_playing_lullaby_birthday_switches_to_birth(void) {
    // Both lullaby1 and lullaby2 should be interrupted by birthday.
    for (Track t_in : {Track::LullabyOne, Track::LullabyTwo}) {
        PlaybackTransition t = next_transition(
            State::Playing, t_in,
            make_event(PlaybackEvent::Kind::BirthdayFired));
        TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::SwitchFile),
                          static_cast<int>(t.action));
        TEST_ASSERT_EQUAL_STRING("/birth.wav", t.path);
        TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
        TEST_ASSERT_EQUAL(static_cast<int>(Track::Birth), static_cast<int>(t.next_track));
    }
}
```

Add `RUN_TEST` calls:

```cpp
    RUN_TEST(test_idle_birthday_opens_birth);
    RUN_TEST(test_playing_lullaby_birthday_switches_to_birth);
```

- [ ] **Step 4.2: Verify failing**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: 6 pass, 2 fail.

- [ ] **Step 4.3: Implement the BirthdayFired cases**

In `firmware/lib/audio/src/playback.cpp`, replace the `case K::BirthdayFired:` block:

```cpp
    case K::BirthdayFired:
        if (state == State::Idle) {
            return {A::OpenFile, kBirthPath, State::Playing, Track::Birth};
        }
        // Playing/* — switch to birth, interrupting whatever was playing.
        // Playing/Birth + BirthdayFired is a no-op the NVS gate prevents
        // from ever reaching here, but if it did, SwitchFile would
        // re-open birth.wav from the start, which is benign.
        return {A::SwitchFile, kBirthPath, State::Playing, Track::Birth};
```

- [ ] **Step 4.4: Verify all 8 tests pass**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: `8 Tests 0 Failures 0 Ignored`.

- [ ] **Step 4.5: Commit**

```bash
git add firmware/lib/audio/src/playback.cpp \
        firmware/test/test_audio_playback/test_playback.cpp
git commit -m "$(cat <<'EOF'
feat(audio): implement BirthdayFired transitions

Idle + Birthday -> OpenFile birth.wav.
Playing/Lullaby* + Birthday -> SwitchFile birth.wav (interrupts
lullaby). Birthday does not auto-resume the lullaby after birth.wav
finishes; per spec, birth.wav -> Idle is a clean termination.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Implement redundant PlayLullabyRequested no-op

Pressing the Audio button while a lullaby is already playing should be a no-op at the state-machine level. (The actual button handler in main.cpp converts press-while-playing into a Stop event, but the state machine should be defensive.)

**Files:**
- Test: `firmware/test/test_audio_playback/test_playback.cpp`
- Modify: `firmware/lib/audio/src/playback.cpp` (no change needed if Task 1's fall-through already handles this, but verify)

- [ ] **Step 5.1: Add the failing test**

Insert before `main()`:

```cpp
void test_playing_play_lullaby_is_noop(void) {
    // While Playing any track, requesting another lullaby start is a no-op.
    // (main.cpp converts press-while-playing to Stop, so this row exists
    // for defensive completeness rather than runtime expectation.)
    for (Track t_in : {Track::LullabyOne, Track::LullabyTwo, Track::Birth}) {
        PlaybackTransition t = next_transition(
            State::Playing, t_in,
            make_event(PlaybackEvent::Kind::PlayLullabyRequested));
        TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::None),
                          static_cast<int>(t.action));
        TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
        TEST_ASSERT_EQUAL(static_cast<int>(t_in), static_cast<int>(t.next_track));
    }
}
```

Add `RUN_TEST` call:

```cpp
    RUN_TEST(test_playing_play_lullaby_is_noop);
```

- [ ] **Step 5.2: Run the test**

```bash
~/Library/Python/3.9/bin/pio test -e native --filter test_audio_playback --verbose
```

Expected: 9 pass. The Task 1 implementation of `K::PlayLullabyRequested` already handles this case — `if (state == State::Idle)` returns OpenFile, otherwise falls through to `return {None, nullptr, state, track}`. No code change needed.

If the test fails, the implementation in `playback.cpp` may have drifted; re-read the `K::PlayLullabyRequested` block from Task 1 step 1.6 and verify it still has the fall-through.

- [ ] **Step 5.3: Run the full native test suite**

```bash
~/Library/Python/3.9/bin/pio test -e native --verbose
```

Expected: all tests pass. Pure-logic playback module is complete (9 transition-table rows covered).

- [ ] **Step 5.4: Commit**

```bash
git add firmware/test/test_audio_playback/test_playback.cpp
git commit -m "$(cat <<'EOF'
test(audio): cover Playing + PlayLullabyRequested no-op

Defensive completeness — main.cpp's button handler converts
press-during-playback into Stop, but the state machine handles
the redundant request without changing state.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Wire playback state machine into ESP32 adapter (file paths + playlist)

Refactor `audio.cpp` to use `next_transition()` for `play_lullaby()`, `stop()`, and EOF handling. After this task, the playlist works (lullaby1 → lullaby2) but birthday auto-fire still only runs when Idle (i.e., birthday-interrupts is NOT yet implemented).

**Files:**
- Modify: `firmware/lib/audio/src/audio.cpp`

- [ ] **Step 6.1: Read the current adapter to confirm starting state**

```bash
grep -n "LULLABY_PATH\|BIRTH_PATH\|state_\|transition_to" \
    /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/lib/audio/src/audio.cpp
```

Confirm the constants and state variable lines line up with what's referenced in the edits below.

- [ ] **Step 6.2: Replace file-path constants and add Track tracking**

In `firmware/lib/audio/src/audio.cpp`, find these lines (around line 28-32):

```cpp
static constexpr i2s_port_t   I2S_PORT          = I2S_NUM_0;
static constexpr size_t       SD_READ_CHUNK     = 1024;   // bytes; 512 samples
static constexpr const char*  LULLABY_PATH      = "/lullaby.wav";
static constexpr const char*  BIRTH_PATH        = "/birth.wav";
static constexpr const char*  NVS_NAMESPACE     = "audio";
static constexpr const char*  NVS_KEY_LAST_YEAR = "last_birth_year";
```

Replace with:

```cpp
static constexpr i2s_port_t   I2S_PORT          = I2S_NUM_0;
static constexpr size_t       SD_READ_CHUNK     = 1024;   // bytes; 512 samples
// File paths now live in audio/playback.h: kLullabyOnePath, kLullabyTwoPath, kBirthPath.
static constexpr const char*  NVS_NAMESPACE     = "audio";
static constexpr const char*  NVS_KEY_LAST_YEAR = "last_birth_year";
```

Add the `audio/playback.h` include (around line 20, near the other audio/ includes):

```cpp
#include "audio.h"              // pulls in audio/fire_guard.h transitively
#include "audio/playback.h"
#include "audio/wav.h"
#include "audio/gain.h"
```

Find this block (around line 37-46):

```cpp
enum class State : uint8_t { Idle, Playing };

static bool        started              = false;
static bool        sd_ok                = false;
static State       state_               = State::Idle;
static BirthConfig birth_               = {};
static uint16_t    last_birth_year_     = 0;
static File        current_file_;
static uint32_t    bytes_played_        = 0;
static uint8_t     read_buf_[SD_READ_CHUNK];
```

Replace with (delete the local `enum class State` since it's now in `playback.h`, add `track_`):

```cpp
// State and Track are defined in audio/playback.h.

static bool        started              = false;
static bool        sd_ok                = false;
static State       state_               = State::Idle;
static Track       track_               = Track::None;
static BirthConfig birth_               = {};
static uint16_t    last_birth_year_     = 0;
static File        current_file_;
static uint32_t    bytes_played_        = 0;
static uint8_t     read_buf_[SD_READ_CHUNK];
```

- [ ] **Step 6.3: Replace `transition_to_playing` and `transition_to_idle` with a `dispatch_event` helper**

Find this block in `audio.cpp` (around line 99-126):

```cpp
static void transition_to_playing(const char* path) {
    current_file_ = SD.open(path, FILE_READ);
    if (!current_file_) {
        Serial.printf("[audio] error: file %s not found\n", path);
        return;  // stay Idle
    }
    uint8_t header[WAV_CANONICAL_HEADER_LEN];
    size_t  got = current_file_.read(header, sizeof(header));
    ParseResult hdr = parse_wav_header(header, got);
    if (hdr.error != WavParseError::Ok) {
        Serial.printf("[audio] error: wav header invalid (code=%u)\n",
                      static_cast<unsigned>(hdr.error));
        current_file_.close();
        return;  // stay Idle
    }
    current_file_.seek(hdr.data_offset);  // safety; should already be here
    bytes_played_ = 0;
    state_ = State::Playing;
    Serial.printf("[audio] play %s\n", path);
}

static void transition_to_idle(const char* reason) {
    if (current_file_) current_file_.close();
    i2s_zero_dma_buffer(I2S_PORT);
    state_ = State::Idle;
    Serial.printf("[audio] %s (played %u bytes)\n",
                  reason, static_cast<unsigned>(bytes_played_));
}
```

Replace with:

```cpp
// Open path on SD, validate WAV header, seek to data, reset byte counter.
// Returns true if the file is ready to pump samples; false on any error.
// Caller owns the state machine update — this function does NOT mutate
// state_ / track_.
static bool open_file_for_playback(const char* path) {
    current_file_ = SD.open(path, FILE_READ);
    if (!current_file_) {
        Serial.printf("[audio] error: file %s not found\n", path);
        return false;
    }
    uint8_t header[WAV_CANONICAL_HEADER_LEN];
    size_t  got = current_file_.read(header, sizeof(header));
    ParseResult hdr = parse_wav_header(header, got);
    if (hdr.error != WavParseError::Ok) {
        Serial.printf("[audio] error: wav header invalid (code=%u) for %s\n",
                      static_cast<unsigned>(hdr.error), path);
        current_file_.close();
        return false;
    }
    current_file_.seek(hdr.data_offset);
    bytes_played_ = 0;
    Serial.printf("[audio] play %s\n", path);
    return true;
}

// Drive the state machine for the given event. Calls next_transition()
// from the pure-logic playback module, executes the returned action,
// and updates state_/track_ on success. On file-open failure, drops to
// Idle as a safe fallback.
static void dispatch_event(PlaybackEvent::Kind kind) {
    PlaybackTransition t = next_transition(state_, track_, PlaybackEvent{kind});
    using A = PlaybackTransition::Action;

    bool io_ok = true;
    switch (t.action) {
    case A::None:
        // No I/O. State/track may still update (e.g. Idle stop is a no-op
        // but stays Idle). Apply below.
        break;
    case A::OpenFile:
        io_ok = open_file_for_playback(t.path);
        break;
    case A::CloseFile: {
        if (current_file_) current_file_.close();
        i2s_zero_dma_buffer(I2S_PORT);
        const char* reason =
            (kind == PlaybackEvent::Kind::FileEnded)    ? "finished" :
            (kind == PlaybackEvent::Kind::StopRequested) ? "stopped"  :
                                                            "closed";
        Serial.printf("[audio] %s (played %u bytes)\n",
                      reason, static_cast<unsigned>(bytes_played_));
        break;
    }
    case A::SwitchFile:
        if (current_file_) current_file_.close();
        io_ok = open_file_for_playback(t.path);
        break;
    }

    if (!io_ok) {
        // SD open / WAV parse failed — bail to Idle.
        i2s_zero_dma_buffer(I2S_PORT);
        state_ = State::Idle;
        track_ = Track::None;
        return;
    }

    state_ = t.next_state;
    track_ = t.next_track;
}
```

- [ ] **Step 6.4: Update the public API functions to dispatch events**

Find this block (around line 218-228):

```cpp
void play_lullaby() {
    if (!started || !sd_ok)    return;
    if (state_ != State::Idle) return;
    transition_to_playing(LULLABY_PATH);
}

void stop() {
    if (!started)                  return;
    if (state_ != State::Playing)  return;
    transition_to_idle("stopped");
}
```

Replace with:

```cpp
void play_lullaby() {
    if (!started || !sd_ok) return;
    dispatch_event(PlaybackEvent::Kind::PlayLullabyRequested);
}

void stop() {
    if (!started) return;
    dispatch_event(PlaybackEvent::Kind::StopRequested);
}
```

- [ ] **Step 6.5: Update `loop()` to dispatch FileEnded for EOF**

Find this block in `loop()` (around line 154-188):

```cpp
    if (state_ == State::Playing) {
        // Pump: read chunk, gain, non-blocking write.
        int n = current_file_.read(read_buf_, sizeof(read_buf_));
        if (n <= 0) {
            transition_to_idle("finished");
            return;
        }
```

Replace `transition_to_idle("finished");` with `dispatch_event(PlaybackEvent::Kind::FileEnded);`. There's a second occurrence a few lines down (the `n &= ~1` then `if (n == 0)` path); change that one too. There's also a third occurrence on `i2s_write` error (`transition_to_idle("aborted");`) — change that to `dispatch_event(PlaybackEvent::Kind::FileEnded);` since we're treating any error as "ended".

After the changes, the Playing branch reads:

```cpp
    if (state_ == State::Playing) {
        // Pump: read chunk, gain, non-blocking write.
        int n = current_file_.read(read_buf_, sizeof(read_buf_));
        if (n <= 0) {
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        n &= ~1;
        if (n == 0) {
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        apply_gain_q8(reinterpret_cast<int16_t*>(read_buf_),
                      static_cast<size_t>(n) / 2,
                      CLOCK_AUDIO_GAIN_Q8);
        size_t written = 0;
        esp_err_t err = i2s_write(I2S_PORT, read_buf_,
                                  static_cast<size_t>(n),
                                  &written, /*ticks=*/0);
        if (err != ESP_OK) {
            Serial.printf("[audio] error: i2s_write err=%d\n", err);
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        if (written < static_cast<size_t>(n)) {
            current_file_.seek(current_file_.position() - (n - written));
        }
        bytes_played_ += written;
        return;
    }
```

- [ ] **Step 6.6: Leave the auto-fire block alone for now**

The auto-fire branch in `loop()` (the part after `if (state_ == State::Playing) { ... return; }`) should be left exactly as it is for now. Birthday-interrupts is implemented in Task 7. Confirming: the line `if (!sd_ok) return;` and the `should_auto_fire(...)` block beneath it stay where they are.

- [ ] **Step 6.7: Build for both ESP32 environments**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware
~/Library/Python/3.9/bin/pio run -e emory 2>&1 | tail -10
~/Library/Python/3.9/bin/pio run -e nora 2>&1 | tail -10
```

Expected: both end with `[SUCCESS]`. The new `playback.h` include and `dispatch_event` machinery should compile clean.

- [ ] **Step 6.8: Run native tests to confirm pure-logic still passes**

```bash
~/Library/Python/3.9/bin/pio test -e native --verbose 2>&1 | tail -20
```

Expected: all tests pass (174 from before, no change in count — Task 6 doesn't add new tests, just wires existing logic).

- [ ] **Step 6.9: Commit**

```bash
git add firmware/lib/audio/src/audio.cpp
git commit -m "$(cat <<'EOF'
feat(audio): wire playback state machine into ESP32 adapter

play_lullaby(), stop(), and loop()'s EOF handling now dispatch events
through next_transition(). File-path constants migrated to
audio/playback.h. Track tracking added (track_) so EOF on lullaby1
correctly advances to lullaby2 instead of going Idle.

Birthday auto-fire still runs only when Idle in this commit;
birthday-interrupts behavior arrives in the next commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Birthday-interrupts behavior in adapter

Move the auto-fire guard out of the "Idle only" branch so it runs every tick. When the guard fires, dispatch BirthdayFired (which the state machine handles for both Idle and Playing). Preserve the NVS-stamp-first sequencing.

**Files:**
- Modify: `firmware/lib/audio/src/audio.cpp`

- [ ] **Step 7.1: Restructure `loop()` so auto-fire runs first**

Find the current `loop()` (around line 151-216 in the post-Task-6 file). Replace its body with:

```cpp
void loop() {
    if (!started) return;

    // Auto-fire check runs every tick regardless of state — birthday
    // can interrupt a playing lullaby per spec design decision 1
    // (docs/superpowers/specs/2026-05-02-audio-playlist-design.md).
    if (sd_ok) {
        auto dt = wc::rtc::now();
        NowFields nf{ dt.year, dt.month, dt.day, dt.hour, dt.minute };
        bool time_known =
            (wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX);
        // already_playing=false: birthday should fire even if a lullaby
        // is playing. The NVS year stamp prevents re-firing within the
        // same calendar year.
        if (should_auto_fire(nf, birth_, last_birth_year_,
                             time_known, /*already_playing=*/false)) {
            Serial.printf("[audio] play birth.wav (year=%u)\n", nf.year);
            // Stamp NVS FIRST. If birth.wav fails to open later, the
            // stamp persists and we don't retry this year — matches
            // original spec's intent (once the system "decides" to fire,
            // don't double-up later in the year).
            if (!nvs_write_last_birth_year(nf.year)) {
                Serial.println("[audio] error: NVS stamp FAILED; skipping birth.wav this tick");
                return;
            }
            last_birth_year_ = nf.year;
            dispatch_event(PlaybackEvent::Kind::BirthdayFired);
            return;  // don't pump audio in same tick as switch
        }
    }

    // Pump audio if Playing.
    if (state_ == State::Playing) {
        int n = current_file_.read(read_buf_, sizeof(read_buf_));
        if (n <= 0) {
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        n &= ~1;
        if (n == 0) {
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        apply_gain_q8(reinterpret_cast<int16_t*>(read_buf_),
                      static_cast<size_t>(n) / 2,
                      CLOCK_AUDIO_GAIN_Q8);
        size_t written = 0;
        esp_err_t err = i2s_write(I2S_PORT, read_buf_,
                                  static_cast<size_t>(n),
                                  &written, /*ticks=*/0);
        if (err != ESP_OK) {
            Serial.printf("[audio] error: i2s_write err=%d\n", err);
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        if (written < static_cast<size_t>(n)) {
            current_file_.seek(current_file_.position() - (n - written));
        }
        bytes_played_ += written;
    }
}
```

- [ ] **Step 7.2: Build for both ESP32 environments**

```bash
~/Library/Python/3.9/bin/pio run -e emory 2>&1 | tail -10
~/Library/Python/3.9/bin/pio run -e nora 2>&1 | tail -10
```

Expected: both `[SUCCESS]`.

- [ ] **Step 7.3: Run all native tests**

```bash
~/Library/Python/3.9/bin/pio test -e native --verbose 2>&1 | tail -10
```

Expected: all tests pass.

- [ ] **Step 7.4: Commit**

```bash
git add firmware/lib/audio/src/audio.cpp
git commit -m "$(cat <<'EOF'
feat(audio): birthday auto-fire interrupts in-progress lullaby

Move should_auto_fire() check from the Idle-only branch to run every
tick. When the guard fires while a lullaby is playing, dispatch
BirthdayFired to the playback state machine, which closes the lullaby
file and opens birth.wav (interrupt). NVS year-stamp ordering preserved
so a power loss during birth.wav doesn't double-fire next year.

Spec: docs/superpowers/specs/2026-05-02-audio-playlist-design.md
design decision 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Update hardware-checks documentation

The bench-bring-up checklist needs the 4 new manual scenarios from the spec.

**Files:**
- Modify: `firmware/test/hardware_checks/audio_checks.md`

- [ ] **Step 8.1: Read the existing audio_checks.md**

```bash
cat /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/test/hardware_checks/audio_checks.md
```

Identify where new scenarios should go — typically there's a numbered list of checks and we append.

- [ ] **Step 8.2: Append the new scenarios**

Add the following to `firmware/test/hardware_checks/audio_checks.md`, at the end of the existing checks section:

```markdown
## Playlist + birthday-interrupts checks (added 2026-05-02)

These checks supersede the original single-file lullaby check from the
2026-04-18 audio spec. Run them once on each clock during bench
bring-up, and again after any audio-module firmware change.

### Check P1 — Playlist progression
1. Idle clock, SD card present with `lullaby1.wav`, `lullaby2.wav`,
   `birth.wav` at root.
2. Press the **Audio** button (SW2).
3. Verify `lullaby1.wav` starts playing audibly.
4. Wait for `lullaby1.wav` to end without pressing anything.
5. Verify `lullaby2.wav` starts playing automatically (no input required)
   within ~1 second of `lullaby1.wav` ending.
6. Wait for `lullaby2.wav` to end.
7. Verify the clock returns to silence (Idle) — no further audio.

### Check P2 — Stop during lullaby1 + restart
1. Press **Audio** to start the playlist.
2. While `lullaby1.wav` is playing, press **Audio** again.
3. Verify audio stops immediately.
4. Press **Audio** again.
5. Verify `lullaby1.wav` starts playing **from the beginning**, NOT
   `lullaby2.wav`. (Always-restart-from-lullaby1, per spec design
   decision 2.)

### Check P3 — Stop during lullaby2 + restart
1. Press **Audio** to start the playlist; let `lullaby1.wav` finish.
2. While `lullaby2.wav` is playing, press **Audio**.
3. Verify audio stops immediately.
4. Press **Audio** again.
5. Verify `lullaby1.wav` (NOT `lullaby2.wav`) starts playing from the
   beginning.

### Check P4 — Birthday interrupts lullaby
1. Set the clock's RTC to 1 minute before the configured birth minute.
   (E.g. for Emory: set RTC to October 6, 18:09.) Use a temporary
   firmware bypass or `rtc::set(...)` from a serial-trigger debug build.
2. Press **Audio** to start the playlist.
3. While `lullaby1.wav` is playing, advance RTC to the birth minute
   (October 6, 18:10).
4. Verify within ~1 second: `lullaby1.wav` cuts off and `birth.wav`
   begins playing.
5. Wait for `birth.wav` to end.
6. Verify the clock returns to silence — `lullaby1.wav` does NOT resume
   automatically (per spec).
7. (Optional) Power-cycle the clock and verify that on the next tick at
   the birth minute, `birth.wav` does NOT play again — the NVS year
   stamp is honored across reboot.

### Check P5 — Missing lullaby2.wav graceful handling
1. Prepare a test SD card with `lullaby1.wav` and `birth.wav` only
   (no `lullaby2.wav`).
2. Insert into the clock, power on.
3. Press **Audio** to start the playlist.
4. Verify `lullaby1.wav` plays normally.
5. After `lullaby1.wav` ends, verify the clock returns to silence
   (Idle). Serial log should show `[audio] error: file /lullaby2.wav not found`.
6. The clock should NOT crash or hang. Subsequent presses of Audio
   should still attempt to start `lullaby1.wav`.
```

- [ ] **Step 8.3: Verify the file renders cleanly (no markdown errors)**

```bash
head -100 /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/test/hardware_checks/audio_checks.md
tail -80 /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/test/hardware_checks/audio_checks.md
```

Visual check: section headers are correct level, code blocks are closed, no stray characters.

- [ ] **Step 8.4: Commit**

```bash
git add firmware/test/hardware_checks/audio_checks.md
git commit -m "$(cat <<'EOF'
docs(audio): add playlist + birthday-interrupts hardware checks

Adds 5 manual bench checks covering the 2-song playlist progression,
stop-and-restart from lullaby1 / lullaby2, birthday-interrupts during
playback, and graceful handling of missing lullaby2.wav.

Spec: docs/superpowers/specs/2026-05-02-audio-playlist-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Cross-link old and new specs + final verification

Add a "Superseded by" pointer in the original audio spec and run the full CI gate locally.

**Files:**
- Modify: `docs/superpowers/specs/2026-04-18-audio-design.md`

- [ ] **Step 9.1: Add the superseded-by pointer to the original spec**

Open `docs/superpowers/specs/2026-04-18-audio-design.md`. Add at the very top of the file (before the existing first heading), a one-line callout:

```markdown
> **Note:** Single-file lullaby playback and the "birthday suppressed during lullaby" behavior described in this spec are superseded by [`2026-05-02-audio-playlist-design.md`](./2026-05-02-audio-playlist-design.md). The rest of this document (I²S setup, NVS stamping, gain control, file format) remains current.
```

- [ ] **Step 9.2: Run the full CI gate locally**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware
~/Library/Python/3.9/bin/pio run -e emory 2>&1 | tail -5
~/Library/Python/3.9/bin/pio run -e nora 2>&1 | tail -5
~/Library/Python/3.9/bin/pio test -e native --verbose 2>&1 | tail -10
```

Expected:
- Both `pio run` commands end with `[SUCCESS]`
- `pio test -e native` summary shows 9 new tests passed in `test_audio_playback`, all existing tests still pass, total `Failures: 0 Ignored: 0`

- [ ] **Step 9.3: Verify by file-level smoke check**

```bash
ls -la /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/lib/audio/include/audio/
ls -la /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/lib/audio/src/
ls -la /Users/samueldacanay/dev/personal/word-clock-for-my-daughters/firmware/test/test_audio_playback/
```

Expected:
- `firmware/lib/audio/include/audio/` contains `fire_guard.h`, `gain.h`, `playback.h`, `wav.h`
- `firmware/lib/audio/src/` contains `audio.cpp`, `fire_guard.cpp`, `gain.cpp`, `playback.cpp`, `wav.cpp`
- `firmware/test/test_audio_playback/` contains `test_playback.cpp`

- [ ] **Step 9.4: Commit**

```bash
git add docs/superpowers/specs/2026-04-18-audio-design.md
git commit -m "$(cat <<'EOF'
docs(audio): cross-link old spec to playlist superseder

Adds a top-of-file pointer in the original audio design doc to the
2026-05-02 playlist spec, calling out that single-file lullaby and
birthday-suppression behaviors are superseded while the rest of the
original spec (I²S, NVS, gain, file format) remains current.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Summary

**Total commits:** 9 (one per task)

**Total new tests:** 9 native (one per row of the transition table)

**Total new code:** ~80 lines pure-logic (`playback.h` + `playback.cpp`), ~50 lines adapter refactor net (delete the two transition_to_* helpers, add open_file_for_playback + dispatch_event, replace state mutations), ~150 lines new tests, ~40 lines new doc

**CI gate:** `pio run -e emory && pio run -e nora && pio test -e native` should pass at every commit boundary.

**Out of scope (per spec):** per-song volume, more than 2 songs, crossfade timing, mid-playback resume on power-on. None of these features have any code path in this plan.
