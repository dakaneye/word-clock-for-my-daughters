# Audio Playlist — Two-Lullaby Playback Design

**Status:** Design
**Author:** Sam
**Date:** 2026-05-02
**Supersedes / extends:** `docs/superpowers/specs/2026-04-18-audio-design.md`

## Summary

Replace single-file lullaby playback with a fixed two-file playlist
(`lullaby1.wav` → `lullaby2.wav`), played in sequence on a single Audio
button press. Birthday auto-fire (`birth.wav`) is preserved as a
separate single-file path with one behavior change: **birthday now
interrupts an in-progress lullaby** rather than being suppressed for the
year.

## Motivation

The original audio design supported one button-triggered song. Sam's
bedtime routine for both daughters is a two-song sequence:

- **Lullaby 1** — "I Am Kind" by Raffi & Lindsay Munroe (shared across
  both clocks).
- **Lullaby 2** — per-kid: Emory gets "Three Little Birds", Nora gets
  "Take on Me".

A single button press should play both songs back to back, matching the
real bedtime routine. A second press at any point during playback stops
everything and returns to Idle.

## Behavior changes

Three decisions, captured here so future maintainers (or Sam in five
years swapping a CR2032 battery) understand the trade-offs:

### 1. Birthday interrupts lullaby (was: suppress)

The original design ran the birthday auto-fire guard only when the
state machine was `Idle`, so a lullaby playing at the exact birth
minute would suppress `birth.wav` for the entire year (NVS-gated to
once per year regardless). With lullaby playtime growing from ~3 min
to ~5-7 min in the new design, the suppression window also grows —
making "kid happens to be playing the lullaby at her birth minute"
increasingly likely.

New behavior: birthday auto-fire runs every tick regardless of state.
If the guard fires while a lullaby is playing, the lullaby is closed
and `birth.wav` opens immediately. After `birth.wav` ends, the state
machine returns to Idle (the lullaby does NOT resume — explicit
choice; resuming would require tracking position state).

### 2. Always restart from lullaby1 on press-after-stop

Stopping mid-playlist (e.g. kid fell asleep during lullaby2) and
pressing Audio again later **always restarts from lullaby1**, never
resumes from the stopped position.

Rejected: tracking last-played position across stops. Adds NVS writes
or RAM-only state, more code paths in the state machine, and the user
experience of "sometimes it picks up mid-song" is jarring without
clear visual feedback.

### 3. Missing files: log error, transition to Idle

If `lullaby1.wav` is missing on a press, OR `lullaby2.wav` is missing
when the playlist tries to advance from lullaby1's EOF, the audio
module logs an error to serial and transitions to Idle. No fallback
to the other file, no backward-compat with old `lullaby.wav` filename.

Same defense as the current single-file design. SD card content is
verified at assembly time; missing files indicate a card prep error
that should be loud, not silent.

`birth.wav` missing at auto-fire is also a log-error-and-stay-Idle
case, with one important wrinkle: the NVS year stamp is written
**before** the file is opened. If NVS stamps but `birth.wav` is
missing, the year is recorded as fired and birth.wav will not retry
this year. This matches the original spec's intent — once the system
"decides" to fire, it doesn't double-up later in the year, even if
the actual playback failed.

## Architecture

The new design extracts a pure-logic playback state machine into a
testable header (`audio/playback.h`) and wires it into the ESP32
adapter (`audio.cpp`). This pattern matches `audio/fire_guard.h` —
keep firmware-platform-independent logic native-testable, keep
SD/I²S/NVS calls in the adapter.

### State + track tags

```cpp
namespace wc::audio {

enum class State : uint8_t { Idle, Playing };
enum class Track : uint8_t { None, LullabyOne, LullabyTwo, Birth };

}
```

`State` keeps the existing two-state shape. `Track` tags which file is
currently open during Playing — necessary so EOF handling can branch
(advance lullaby1 → lullaby2, vs. terminate at lullaby2 EOF, vs.
terminate at birth EOF without auto-resume).

When `State == Idle`, `Track == None`. Otherwise `Track` matches the
file open in `current_file_`.

### Playback state machine

Pure-logic functions in `audio/playback.h`:

```cpp
struct PlaybackEvent {
    enum class Kind {
        PlayLullabyRequested,
        StopRequested,
        FileEnded,
        BirthdayFired,
    } kind;
};

struct PlaybackTransition {
    enum class Action {
        None,                 // no-op
        OpenFile,             // open path, start I²S
        CloseFile,            // stop I²S, close current file
        SwitchFile,           // close current, open new path
    } action;
    const char* path;         // valid when action is OpenFile / SwitchFile
    State next_state;
    Track next_track;
};

PlaybackTransition next_transition(State, Track, PlaybackEvent);
```

Native tests cover the transition table:

| Current state    | Event                  | Action       | Next state | Next track    |
|------------------|------------------------|--------------|------------|---------------|
| Idle             | PlayLullabyRequested   | OpenFile     | Playing    | LullabyOne    |
| Playing/LullabyOne | FileEnded            | SwitchFile   | Playing    | LullabyTwo    |
| Playing/LullabyTwo | FileEnded            | CloseFile    | Idle       | None          |
| Playing/Birth    | FileEnded              | CloseFile    | Idle       | None          |
| Playing/*        | StopRequested          | CloseFile    | Idle       | None          |
| Idle             | StopRequested          | None         | Idle       | None          |
| Playing/*        | PlayLullabyRequested   | None         | (unchanged)| (unchanged)   |
| Idle             | BirthdayFired          | OpenFile     | Playing    | Birth         |
| Playing/*        | BirthdayFired          | SwitchFile   | Playing    | Birth         |

Note: `Playing/Birth + BirthdayFired` returns `SwitchFile` to re-open
`birth.wav` from the start. The NVS year-stamp gate ensures the
auto-fire guard doesn't return true a second time within the same
calendar year, so this row is unreachable in practice; the SwitchFile
behavior is benign defense-in-depth.

### Adapter responsibilities (`audio.cpp`)

The ESP32 adapter:

1. Calls `next_transition(state_, track_, event)` per relevant tick or
   button event.
2. Executes the returned `Action` via SD / I²S calls.
3. Updates `state_` and `track_` from the returned values.

The auto-fire guard runs every tick (not just when Idle). If the
guard fires:

- Pass `BirthdayFired` to `next_transition`.
- Stamp NVS year **before** executing OpenFile/SwitchFile (preserves
  fire-once semantics across power loss).

The button press handler in `main.cpp` continues to call
`audio::play_lullaby()` and `audio::stop()` — those public API
functions translate to `PlayLullabyRequested` and `StopRequested`
internally. No changes needed in `main.cpp`.

## SD card content

Each kid's SD card holds three WAV files at the root, all 16-bit PCM
44.1 kHz mono little-endian:

| File              | Emory                       | Nora                |
|-------------------|-----------------------------|----------------------|
| `/lullaby1.wav`   | "I Am Kind"                 | "I Am Kind"         |
| `/lullaby2.wav`   | "Three Little Birds"        | "Take on Me"        |
| `/birth.wav`      | birth-minute voice memo     | birth-minute voice memo |

The old `/lullaby.wav` filename is **deprecated** and will not be
read by the new firmware. SD card prep procedure (in
`docs/hardware/assembly-plan.md` parallel-tracks) updates accordingly.

Per-clock total ~25-30 MB across all three files. Any microSD ≥1 GB
suffices; class 10 or better recommended for read latency margin.

## Public API

The audio module's external API is unchanged in shape:

```cpp
void begin(const BirthConfig& birth);
void loop();
void play_lullaby();   // semantics: starts the 2-song playlist
void stop();           // semantics: stops everything, no resume
bool is_playing();
```

Only `play_lullaby()`'s semantics change (it now starts a playlist
rather than a single file). `main.cpp` and the buttons / wifi_provision
modules require no changes.

## Failure handling

| Condition | Behavior | Log line |
|---|---|---|
| `lullaby1.wav` missing on press | Stay Idle | `[audio] error: file /lullaby1.wav not found` |
| `lullaby2.wav` missing after lullaby1 EOF | Transition to Idle | `[audio] error: file /lullaby2.wav not found` |
| `birth.wav` missing on auto-fire | Transition to Idle; NVS stamp persists; no retry this year | `[audio] error: file /birth.wav not found` |
| Any file's WAV header invalid | Transition to Idle | Existing `parse_wav_header` error code logged |
| `i2s_write` returns non-OK error | Dispatched as FileEnded (graceful degrade): on lullaby1 advances to lullaby2; on lullaby2 / birth.wav transitions to Idle | Existing `[audio] error: i2s_write err=...` line |
| SD card removed mid-playback | `current_file_.read` returns ≤0 → treated as EOF → transitions per state machine | Existing `finished` log line |

## Test plan

### New: `firmware/test/test_audio_playback/`

Native Unity tests of the pure-logic `next_transition()` function. One
test case per row of the transition table above. ~12-15 individual
assertions.

### Updated: `firmware/test/test_audio_fire_guard/`

No logic changes to the guard itself. Add coverage that confirms the
guard returns `true` independent of `already_playing` value when:
- Year not yet recorded
- Time is known
- Minute matches

This documents the new "interrupt during playing" intent.

### Hardware checks: `firmware/test/hardware_checks/audio_checks.md`

Add manual test steps:

1. **Playlist progression** — press Audio at Idle. Verify lullaby1
   plays, then lullaby2 plays automatically without further input,
   then state returns to Idle (silence).
2. **Stop during lullaby1** — press Audio during lullaby1 → silence
   immediately. Press again → restart from lullaby1, NOT lullaby2.
3. **Stop during lullaby2** — press Audio during lullaby2 → silence
   immediately. Press again → restart from lullaby1.
4. **Birthday interrupt** — set RTC to birthday minute via temporary
   firmware bypass, press Audio, wait for lullaby to start, then
   advance RTC by 1 second past the birth minute. Verify lullaby
   stops and `birth.wav` plays. After birth.wav ends, lullaby does
   NOT resume.

## Migration notes for in-progress build

Sam's current build (Emory and Nora's clocks, frames glued, faces
glued, light channels printed, PCBs in JLCPCB transit as of 2026-05-02)
is unaffected by this firmware change at the hardware level. Changes
required:

1. Rename / re-record audio assets on each kid's SD card to use the
   new file names.
2. Re-flash both clocks once new firmware is on `main`. Flash happens
   anyway during PCB bench-bring-up, so this is a single re-flash, not
   an extra step.
3. Update `firmware/test-sketches/` is NOT required — the playlist
   logic lives in `firmware/lib/audio/`, not in the test sketches.

## Risks

- **Lullaby2 missing scenario** is more likely than missing-lullaby in
  the original design (more files, more places to mess up SD prep).
  Mitigation: documented log line, hardware-checks step that catches
  it during initial bench bring-up.
- **Birthday-interrupts behavior** is a behavior change vs. the
  original spec. Documented above; the `should_auto_fire` guard's
  `already_playing` parameter is no longer used by the audio module
  (always called with `false`), but the parameter is preserved in the
  pure-logic API for future extensibility.
- **NVS stamp racing with file open** — if NVS write succeeds but the
  filesystem operation to open `birth.wav` fails, the year is
  recorded fired without playback. Documented as intentional in
  Failure Handling above. This matches existing single-file behavior.

## Out of scope

- **Per-song volume control.** Volume is fixed at compile-time per
  `CLOCK_AUDIO_GAIN_Q8`. If lullaby1 and lullaby2 have different
  loudness, the user normalizes them at the audio-prep step before
  copying to SD. Firmware does not normalize.
- **More than two songs.** Playlist is hard-coded at exactly two
  files. Generalizing would require SD-directory enumeration or a
  manifest file; both add complexity without current demand.
- **Crossfade / gap timing between lullaby1 and lullaby2.** The
  transition is whatever the SD/I²S latency happens to be (sub-100ms
  in practice). No designed silence period.
- **Resuming on power-on if power was lost mid-lullaby.** Power-on
  always starts in Idle state; no replay of in-progress lullaby.
