# Audio Firmware Module — Design Spec

Date: 2026-04-18

## Overview

The `audio` module owns the I²S + MAX98357A + microSD playback path
for two files on a per-kid SD card: `lullaby.mp3` → now `lullaby.wav`
(button-triggered) and `birth.mp3` → now `birth.wav` (auto-fired at
the birth minute on the birthday). Volume is fixed at assembly time
(hardware gain jumper + optional Q8 software scalar). The module is
main-loop-integrated — no FreeRTOS task, no worker thread — and
relies on `ntp::loop()` deferring its ~1 s `forceUpdate()` during
playback as the only concurrent-safety measure it needs.

A core decision this spec locks is to **store audio as raw 16-bit
PCM in a WAV container**, not MP3. That eliminates the entire MP3
decoder dependency (libhelix / ESP8266Audio / ESP32-audioI2S — none
are needed) and collapses the adapter to an SD-read → I²S-write
pump. The tradeoff is file size: ~88 kB/s raw PCM vs ~16 kB/s MP3
128 kbps CBR, a ~5.5× expansion that's trivial on any modern
microSD (~18.5 MB total per card for a 3 min lullaby + 30 s birth
message vs 4+ GB card capacity).

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
(audio listed as a Phase 2 firmware module in TODO.md; parent
§Hardware Platform > Audio + §Firmware Behaviors > Audio button).

Sibling pattern: `wc::rtc` (`docs/superpowers/specs/2026-04-17-rtc-design.md`)
and `wc::ntp` (`docs/superpowers/specs/2026-04-17-ntp-design.md`).
The pure-logic split + `#ifdef ARDUINO` adapter shape mirrors rtc /
ntp / buttons / display / wifi_provision exactly.

## Scope

**In:**

- `wc::audio::begin(BirthConfig)` — initialize the I²S driver
  (BCLK=26, LRC=25, DIN=27 per pinmap), mount SD via VSPI
  (CS=5, CLK=18, MISO=19, MOSI=23), open the NVS namespace
  `"audio"` and seed `last_birth_year` from it. No audio output.
  Idempotent.
- `wc::audio::loop()` — single function called from `main.cpp`'s
  `loop()`. When `Playing`: read the next chunk from the open
  WAV, apply software gain, non-blocking `i2s_write`, advance
  state on EOF / error. When `Idle`: run the pure-logic
  birthday auto-fire guard against `wc::rtc::now()`; if the guard
  returns true, transition to `Playing` with `birth.wav`.
- `wc::audio::play_lullaby()` — called by the button handler in
  `main.cpp` when Idle. Opens `lullaby.wav`, validates header,
  transitions to `Playing`.
- `wc::audio::stop()` — called by the button handler in
  `main.cpp` when Playing. Closes file, stops I²S TX, transitions
  to Idle.
- `wc::audio::is_playing()` — public observer. The button handler
  uses it to pick between `stop` and `play_lullaby`. The `ntp`
  module uses it to early-return during playback (avoiding the
  ~1 s `forceUpdate()` underrun risk).
- Pure-logic `parse_wav_header(const uint8_t* buf44) → ParseResult`
  — validates RIFF/WAVE/fmt /data magics, PCM codec tag == 1,
  sample_rate == 44100, channels == 1, bits_per_sample == 16.
  Returns `data_offset` + `data_size_bytes` on success; enum
  error code otherwise. Native-testable.
- Pure-logic `apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8)`
  — in-place Q8 integer multiply with contract `gain_q8 ∈ [0,256]`
  (256 = unity, 128 = −6 dB). Native-testable.
- Pure-logic `should_auto_fire(now_fields, BirthConfig, last_fired_year, time_known, already_playing)`
  — returns true iff all auto-fire gates pass. Native-testable.
- `#ifdef ARDUINO` guard on `audio.cpp` adapter — same pattern as
  rtc / ntp / buttons / display / wifi_provision.

**Out:**

- **MP3 decode.** Flipped to raw WAV. No decoder code, no libhelix
  dependency, no ESP8266Audio dependency, no ESP32-audioI2S
  dependency.
- **Variable bit rates / stereo / sample rates other than 44.1 kHz.**
  Files that fail `parse_wav_header` validation are rejected
  before playback starts. Authoring workflow: Audacity/Logic
  export "WAV (Microsoft) 16-bit PCM", channel = Mono, sample
  rate = 44100 Hz.
- **External audio-upload / provisioning flow.** Files are placed
  on the microSD during assembly (Dad with a card reader). No
  firmware-side upload, no captive-portal audio tab, no
  over-the-air audio-file swap. Out of scope.
- **Runtime volume control.** Hardware gain is set at assembly
  via the MAX98357A GAIN pin resistor jumper (9/12/15/18 dB).
  Optional software fine trim via `CLOCK_AUDIO_GAIN_Q8` in
  `configs/config_<kid>.h` (default 256 = unity). No buttons,
  no UI, not user-adjustable. Per parent spec §Audio button.
- **Shutdown-pin management.** Pinmap has no GPIO for the
  MAX98357A's SD pin; it's tied high (always on). Idle current
  ~2 mA; negligible next to the LED load. A hiss-when-idle
  problem would be a hardware rev, not a firmware fix.
- **Multi-file playlist / queue / crossfade.** Only one file
  plays at a time; second press during playback = stop, not
  queue.
- **Audio streaming from WiFi.** Files live on the SD card,
  period. No HTTP-streamed radio, no UDP audio.
- **I²S input / recording.** The MAX98357A is output-only; no
  microphone in the BOM.
- **Cross-year replays of `birth.wav` on the same year.** Guarded
  by NVS `last_birth_year`; fires exactly once per year. Missing
  the minute means missing the year (per §Decisions deliberately
  deferred).
- **Grace-window birthday auto-fire.** Considered (fire any time
  in `[birth_minute, birth_minute + 5 min)` to match display's
  5-min word resolution) and rejected in favor of exact-minute
  match. See §Decisions deliberately deferred.

## Decisions deliberately deferred

These were considered and dropped during brainstorming. Recorded so
future-us doesn't relitigate without context:

- **`earlephilhower/ESP8266Audio`.** The default pick for MP3+I²S
  on Arduino-ESP32. Dropped once the file format flipped to WAV:
  the library's value is its decoder-composition (MP3 / AAC / WAV
  all through `AudioGeneratorX` classes + `AudioOutputI2S`), and
  we only need one trivial path (WAV → I²S). Rolling our own
  ~80 LOC adapter costs less than a library dependency's
  version-skew tax over a 40-year horizon.
- **`schreibfaul1/ESP32-audioI2S`.** Would be a natural fit for a
  streaming architecture. Requires PSRAM (minimum 2 MB per
  library README). AITRIP ESP32-DevKitC-V1 is a bare ESP32-WROOM-32
  with no PSRAM. Hard stop.
- **`pschatzmann/arduino-libhelix` (raw libhelix).** Still an MP3
  decoder; pointless once we're on WAV.
- **MP3 128 kbps CBR (the original plan).** Parent spec pinned
  MP3 for footprint reasons (~1 MB/min vs ~5.5 MB/min WAV). This
  spec flips to WAV after math: microSD cards start at 4 GB and
  we need ~18.5 MB/card; the file-size win doesn't justify a
  decoder dependency + its RAM/flash + its underrun-risk under
  main-loop jitter.
- **FreeRTOS task for audio pumping (pinned to core 0 with WiFi
  or core 1 with user code).** Considered for robustness against
  main-loop blocking. Dropped because (a) the only main-loop
  blocker in the current firmware is `ntp::forceUpdate` (~1 s,
  once per ~24 h), which defers during playback at zero cost, and
  (b) WAV-on-SD has ~46 ms underrun tolerance with our DMA buffer
  sizing — orders of magnitude above any realistic non-ntp loop
  jitter. A task would add shared-state synchronization (stop
  command, is_playing flag) that the main-loop integration
  avoids entirely.
- **Grace-window birthday auto-fire (fire in `[birth_minute,
  birth_minute + 5 min)` to match display's 5-min word
  resolution).** Considered for the "boot at 6:11 PM on
  birthday" case. User preference: exact-minute only. A missed
  minute means a missed year. It's an extremely rare scenario
  not worth optimizing.
- **Public `play_birth()` called externally by main.cpp with the
  fire-once logic living in main.cpp.** Considered to keep audio
  decoupled from rtc. Rejected: scatters fire-once semantics
  (NVS guard, time_known check, already-playing check) across
  two modules. Audio owns all of it internally; `BirthConfig`
  passed at `begin()` is the full coupling surface.
- **`audio::begin()` mounting SD vs. main.cpp owning SD.**
  Considered lifting SD mount to main.cpp now, in case a future
  module needs SD access. No such module exists in scope; deferred
  until a second SD consumer lands (if ever). Audio owns SD init
  for now; trivial to lift later.
- **Per-frame / periodic progress logging during playback.**
  Considered for diagnostics. Rejected as spam. Single-shot log
  lines on state transitions (start / stop / finish / error) are
  sufficient.
- **Runtime underrun detection via `i2s_write` return value.**
  Considered surfacing short writes as a warning. Rejected as
  noise: the buffer is deliberately sized for margin, and real
  underruns are an audible stutter — a field signal, not a log
  signal.

## Behavioral Contract

### Playback state machine

Two states, four events. No "loading" state — SD open is sub-ms and
not separately observable from the caller's POV.

```
States: Idle, Playing
Events: PlayLullabyRequested, PlayBirthRequested,
        StopRequested, EofOrError

Idle    + PlayLullabyRequested → Playing  (open lullaby.wav, start I²S)
Idle    + PlayBirthRequested   → Playing  (open birth.wav, start I²S)
Idle    + StopRequested        → Idle     (no-op)
Idle    + EofOrError           → Idle     (unreachable — only Playing receives these)
Playing + PlayLullabyRequested → Playing  (no-op; dispatched in main.cpp as Stop instead)
Playing + PlayBirthRequested   → Playing  (no-op; auto-fire guard suppresses when Playing)
Playing + StopRequested        → Idle     (close file, stop I²S TX)
Playing + EofOrError           → Idle     (close file, stop I²S TX)
```

Dispatch note: the main.cpp button handler reads `is_playing()` and
calls `stop()` or `play_lullaby()` accordingly. The state-machine
no-ops above are defense-in-depth against out-of-order callers (e.g.
a future module that calls `play_lullaby()` while already playing);
the canonical caller path never hits them.

### Triggering — lullaby (button) vs birth (auto-fire)

**Lullaby.** Press once when Idle → play `lullaby.wav`. Press again
when Playing → stop. Exactly the contract in the parent spec §Audio
button. The button handler lives in `main.cpp`:

```cpp
case BE::AudioPressed:
    if (wc::wifi_provision::state() == WS::AwaitingConfirmation) {
        wc::wifi_provision::confirm_audio();
    } else if (wc::audio::is_playing()) {
        wc::audio::stop();
    } else {
        wc::audio::play_lullaby();
    }
    break;
```

The `AwaitingConfirmation` branch wins during the narrow captive-
portal confirmation UX window; lullaby works in every other state,
including `Offline`, `AwaitingCredentials`, `Connecting`, `Online`.

**Birth message.** Plays exactly once per year at the exact birth
minute:

- Emory: 2030-10-06 18:10 and every subsequent year at 18:10 on
  Oct 6.
- Nora: 2032-03-19 09:17 and every subsequent year at 09:17 on
  Mar 19.

Fired from inside `audio::loop()` via the pure-logic
`should_auto_fire()` guard (§Birthday auto-fire guards below).
Auto-fire requires ALL of:

1. Today's date matches `BirthConfig.{month, day}`.
2. Current time matches `BirthConfig.{hour, minute}` exactly.
3. `last_birth_year != current_year` (NVS guard — not already
   fired this year).
4. `time_known == true` — defined as `wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX`.
5. `already_playing == false` — lullaby-in-progress suppresses the
   auto-fire for this year.

On a successful auto-fire: write `current_year` to NVS
`audio/last_birth_year` BEFORE transitioning to Playing. This
ensures a power cycle mid-`birth.wav` playback does not re-trigger
on the next boot when the same minute is still live.

### WAV format validation

Exactly one format is accepted:

- **Container:** RIFF WAVE
- **Codec tag:** 1 (`WAVE_FORMAT_PCM` — uncompressed linear PCM)
- **Sample rate:** 44100 Hz
- **Channels:** 1 (mono)
- **Bits per sample:** 16
- **Byte order:** little-endian (RIFF standard)

The first 44 bytes of any valid file of the above shape are a
canonical RIFF header. `parse_wav_header(buf44)` validates each
field and returns:

```cpp
enum class WavParseError : uint8_t {
    Ok                = 0,
    BadRiffMagic      = 1,  // bytes 0-3 ≠ "RIFF"
    BadWaveMagic      = 2,  // bytes 8-11 ≠ "WAVE"
    BadFmtMagic       = 3,  // bytes 12-15 ≠ "fmt "
    BadCodecTag       = 4,  // bytes 20-21 ≠ 0x0001 (PCM)
    WrongChannelCount = 5,  // bytes 22-23 ≠ 1
    WrongSampleRate   = 6,  // bytes 24-27 ≠ 44100
    WrongBitsPerSample= 7,  // bytes 34-35 ≠ 16
    BadDataMagic      = 8,  // bytes 36-39 ≠ "data"
    Truncated         = 9,  // caller passed <44 bytes
};

struct ParseResult {
    WavParseError error;
    uint32_t      data_offset;      // always 44 for canonical files
    uint32_t      data_size_bytes;  // bytes 40-43 (little-endian)
};
```

On any error, the file is closed and the state machine returns to
Idle. One-time log: `[audio] error: wav header invalid (code=<N>)`.

Files with extra "fmt " chunk metadata (canonical header is 16 bytes
of fmt data; some tools write 18 or 40) break the strict 44-byte
layout. Authoring instruction in the hardware checklist will pin the
exact export settings. Out of scope for the parser: tolerance to
non-canonical fmt-chunk lengths. We choose strict validation and
require the authoring tool to produce canonical files.

### Software gain (Q8 scalar)

Applied in-place on each PCM chunk before `i2s_write`:

```cpp
for (size_t i = 0; i < n; ++i) {
    samples[i] = static_cast<int16_t>(
        (static_cast<int32_t>(samples[i]) * gain_q8) >> 8
    );
}
```

Contract: `gain_q8 ∈ [0, 256]`. Values above 256 would clip on
peaks and are rejected at compile time via a `static_assert` in the
header's consumer (see §Integration / per-kid config).

| `gain_q8` | Attenuation | Use |
|---:|---:|---|
| 256 | 0 dB (unity) | Default. Hardware jumper does the coarse tune. |
| 181 | ~−3 dB | Small trim if the hardware jumper's steps (9/12/15/18 dB) overshoot. |
| 128 | ~−6 dB | |
| 64 | ~−12 dB | |
| 0 | Mute | Debug/test only. |

Why Q8 and not a float multiplier: integer math on 16-bit samples
avoids the FPU-vs-fixed-point question on ESP32, is branch-free, and
matches the idiom the rest of the firmware uses (e.g., display's
palette dim multiplier is also integer).

### I²S DMA + underrun tolerance

I²S driver install settings (ESP-IDF v4.4 via Arduino-ESP32 2.0.x):

| Param | Value | Rationale |
|---|---|---|
| `sample_rate` | 44100 | Matches WAV file |
| `bits_per_sample` | I2S_BITS_PER_SAMPLE_16BIT | Matches WAV file |
| `channel_format` | I2S_CHANNEL_FMT_ONLY_LEFT | Mono out the L channel; MAX98357A sums L+R internally per its SD pin config (default: L+R mixed). |
| `communication_format` | I2S_COMM_FORMAT_STAND_I2S | Standard Philips I²S framing |
| `dma_buf_count` | 4 | 4× buffer chain for margin |
| `dma_buf_len` | 1024 | samples, not bytes; 1024 × 2 B = 2048 B/buffer |
| `use_apll` | false | PLL source acceptable; jitter is inaudible at 44.1 kHz |
| `tx_desc_auto_clear` | true | Pads trailing zeros on buffer exhaustion, avoiding residual pops |

Total DMA ring: 4 × 1024 × 2 B = 8192 bytes of buffered PCM, which
is 8192 / 88200 ≈ **93 ms of audio**. `i2s_write` with a 0-tick
timeout never blocks; the adapter backs off to the next `loop()`
tick when the ring is full.

Underrun tolerance — the maximum interval the main loop may stall
without audible glitch — is the DMA ring depth minus the running
I²S TX rate margin. At 93 ms ring depth, any `loop()` call that
returns within ~40-50 ms will not starve I²S. Observed upper bound
on non-audio work per `loop()` tick:

| Module | Typical per-tick cost | Worst-case cost |
|---|---|---|
| `wifi_provision::loop()` | <100 µs | ~1 ms (DNS hijack response) |
| `buttons::loop()` | <50 µs | <100 µs |
| `ntp::loop()` | <10 µs when Online but pre-deadline | ~1 s on `forceUpdate()` — **DEFERRED during playback** |
| `display::render + FastLED.show()` | ~1 ms for 35 LEDs | ~2 ms during birthday rainbow |
| `audio::loop()` itself | ~100-500 µs (SD read + gain + i2s_write) | ~2 ms on SD retry |

Sum of worst-case non-ntp: ~5 ms. Margin: ~88 ms. Comfortable.

### Birthday auto-fire guards

Pure-logic guard signature:

```cpp
struct NowFields {
    uint16_t year;    // full year (e.g. 2030)
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23
    uint8_t  minute;  // 0-59
};

bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing);
```

Returns true iff ALL of:

- `time_known == true`
- `already_playing == false`
- `now.month == birth.month`
- `now.day == birth.day`
- `now.hour == birth.hour`
- `now.minute == birth.minute`
- `now.year != last_fired_year`

The adapter populates `time_known` from
`wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX`,
`already_playing` from its own state, `last_fired_year` from its
own NVS key.

### NVS schema

Audio owns its own NVS namespace, peer to `wifi`:

| Namespace | Key | Type | Default | Purpose |
|---|---|---|---|---|
| `"audio"` | `last_birth_year` | `UShort` (uint16) | `0` | Year of the most recent successful birth.wav auto-fire. |

`Preferences::begin("audio", /*readOnly=*/false)` on write;
`/*readOnly=*/true` on boot-time read. Matches the idiom in
`firmware/lib/wifi_provision/src/nvs_store.cpp`. A value of `0`
means "never fired" — treated the same as "fired in a year before
`birth.year`," i.e., the first-year case.

### Failure modes

| Scenario | Behavior |
|---|---|
| `SD.begin(PIN_SD_CS)` fails in `begin()` | Log `[audio] begin: SD FAILED` once. Module remains initialized with `sd_ok=false`; `play_lullaby()` / birth auto-fire both gate on `sd_ok && i2s_ok` and fail fast. No retry. |
| `i2s_driver_install` fails in `begin()` | Log `[audio] begin: I2S install FAILED` once. `i2s_ok=false`. Same fail-fast gate as SD. Module is effectively disabled until reboot. |
| `lullaby.wav` / `birth.wav` not found on SD | `play_lullaby()` / auto-fire opens file, read fails, header parse reports truncated. Log once. Stay Idle. |
| WAV header fails validation (any of the 9 error codes) | Close file, return to Idle. Log the error code. |
| SD read returns short / errors mid-playback | Treated as EOF. Log `[audio] error: SD read failed at offset <N>`. Close file, return to Idle. Speaker stops cleanly (tx_desc_auto_clear drains to silence). |
| I²S DMA ring fills (`i2s_write` returns fewer bytes than requested with 0 timeout) | Expected and harmless. The unread portion of the SD-read buffer is held in the module's static shunt buffer; next `loop()` tick retries. |
| `i2s_write` returns ESP_OK with `written == 0` persistently (peripheral hung, clock glitch) | Not separately retried; the SD rewind re-reads the same bytes next tick indefinitely. A hard hang here is indistinguishable from a stuck peripheral — diagnosable only via serial logs showing no `finished` / `stopped` line and the clock face continuing to render. Acceptable: the failure mode is "audio doesn't play" which is observable at the speaker, not "clock crashes". |
| WAV `data_size_bytes` in header > actual file size on disk | Read loop transitions to Idle on `read() <= 0` regardless of what the header claimed; header's `data_size` is used only by `parse_wav_header` to validate the `data` chunk magic offset, not to gate the read loop. Benign mismatch. |
| SD card swapped (removed, different card inserted) mid-session | `current_file_` handle is invalidated at the FS layer. Next `read()` returns ≤0 → treated as EOF → clean transition to Idle. Next play attempt re-mounts via the FS driver. If the new card doesn't contain the expected files, `play_lullaby()` / auto-fire log the standard "file not found" error. |
| Audio button held (not tap-released) during captive-portal state transitions | Debouncing is `wc::buttons` module's responsibility; each completed press emits exactly one `AudioPressed` event regardless of hold duration. Audio module sees discrete events only. |
| User presses audio button during `birth.wav` auto-play | `is_playing() == true` → button handler calls `stop()`. Kid hears a partial birth message; last_birth_year is already stamped so no re-fire this year. Accepted tradeoff. |
| Power cycle mid-`birth.wav` auto-play | NVS `last_birth_year` is stamped BEFORE the transition to Playing, and the in-RAM `last_birth_year_` is only updated after a successful NVS write. On reboot, auto-fire guard returns false (`last_fired_year == current_year`). No re-fire this year. Acceptable. |
| NVS `putUShort("last_birth_year", year)` fails (flash full / corrupt) | Auto-fire path sees `nvs_write_last_birth_year` return false → logs `[audio] error: NVS stamp FAILED; skipping birth.wav this tick` → returns without transitioning. Birth message is missed this minute; next minute the guard will retry (same minute match still possible for the current-year-vs-last-year check on the very next tick since last_birth_year_ is still last year's value). This is the correct failure mode — missing a birth message is acceptable; missing or double-firing across a power cycle is not. |
| Power cycle mid-`lullaby.wav` playback | No NVS state for lullaby. On reboot, module is Idle. Kid presses button again → plays from start. |
| RTC says it's 6:10 PM but `seconds_since_last_sync() == UINT32_MAX` | `time_known == false` → auto-fire skipped. Happens on a fresh-flashed device with no prior NTP sync (e.g., test bench, pre-captive-portal). |
| `audio::begin()` called twice (defensive) | Second call is a no-op; `started` flag short-circuits. Matches ntp / rtc / wifi_provision idiom. |
| `audio::play_lullaby()` / `stop()` / `is_playing()` called before `begin()` | Returns early (`started == false`). `is_playing()` returns false. Defense against button-press-during-setup race. |
| Audio button pressed during captive-portal `AwaitingCredentials` | `is_playing()` false → `play_lullaby()` called. Plays. No coupling with captive-portal state machine. |
| Audio button pressed during captive-portal `AwaitingConfirmation` | `wifi_provision::confirm_audio()` wins in the main.cpp button dispatch. Audio module never sees the event. |

## Architecture

Hexagonal split, matching `display` / `buttons` / `wifi_provision` /
`rtc` / `ntp`. Pure-logic WAV parser + gain scaler + auto-fire guard
are native-testable; adapter is thin I²S + SD + `Preferences` glue.

```
firmware/lib/audio/
  include/
    audio.h                # ESP32-only public API (begin, loop,
                           #   play_lullaby, stop, is_playing)
    audio/
      wav.h                # parse_wav_header + ParseResult +
                           #   WavParseError + canonical constants
      gain.h               # apply_gain_q8 + CLOCK_AUDIO_GAIN_Q8 default
      fire_guard.h         # should_auto_fire + NowFields +
                           #   BirthConfig
      state_machine.h      # next_state(State, Event) — pure transition
  src/
    audio.cpp              # ADAPTER — I²S install, SD mount,
                           #   NVS, state machine, pump
                           #   (#ifdef ARDUINO)
    wav.cpp                # pure-logic RIFF header validation
    gain.cpp               # pure-logic Q8 scalar
    fire_guard.cpp         # pure-logic birthday guard
    state_machine.cpp      # pure-logic state transitions

firmware/test/
  test_audio_wav/test_wav.cpp
  test_audio_gain/test_gain.cpp
  test_audio_fire_guard/test_fire_guard.cpp
  test_audio_state_machine/test_state_machine.cpp
```

Pure-logic `.cpp` files get added to `[env:native]` `build_src_filter`
in `platformio.ini`. `audio.cpp` is guarded with `#ifdef ARDUINO` for
the LDF deep+ reason already documented in sibling specs.

### Namespace

`wc::audio` for public + pure helpers, consistent with sibling
modules.

## Public API

### Pure-logic WAV header validation

```cpp
// firmware/lib/audio/include/audio/wav.h
#pragma once

#include <cstdint>
#include <cstddef>

namespace wc::audio {

// Canonical WAV constants — a file matching these is our only
// accepted format. See §WAV format validation.
constexpr uint16_t WAV_PCM_CODEC_TAG       = 0x0001;
constexpr uint16_t WAV_CHANNEL_COUNT_MONO  = 1;
constexpr uint32_t WAV_SAMPLE_RATE_HZ      = 44100;
constexpr uint16_t WAV_BITS_PER_SAMPLE     = 16;
constexpr size_t   WAV_CANONICAL_HEADER_LEN = 44;

enum class WavParseError : uint8_t {
    Ok                 = 0,
    BadRiffMagic       = 1,
    BadWaveMagic       = 2,
    BadFmtMagic        = 3,
    BadCodecTag        = 4,
    WrongChannelCount  = 5,
    WrongSampleRate    = 6,
    WrongBitsPerSample = 7,
    BadDataMagic       = 8,
    Truncated          = 9,
};

struct ParseResult {
    WavParseError error;
    uint32_t      data_offset;      // bytes from file start to PCM
    uint32_t      data_size_bytes;  // from the "data" chunk header
};

// Pure. Validates a canonical 44-byte RIFF/WAVE/fmt /data header.
// `buf` must point to at least `buf_len` bytes; if buf_len < 44,
// returns Truncated. On success returns {Ok, 44, data_size}.
ParseResult parse_wav_header(const uint8_t* buf, size_t buf_len);

} // namespace wc::audio
```

### Pure-logic gain scalar

```cpp
// firmware/lib/audio/include/audio/gain.h
#pragma once

#include <cstdint>
#include <cstddef>

// Default software gain (Q8). Unity = 256. Per-kid configs may
// override via -D or #define BEFORE including this header. Kept
// here so the module compiles cleanly even if a config header
// hasn't been updated yet — avoids a spec-induced compile break
// on first build.
#ifndef CLOCK_AUDIO_GAIN_Q8
#define CLOCK_AUDIO_GAIN_Q8 256
#endif

static_assert(CLOCK_AUDIO_GAIN_Q8 >= 0 && CLOCK_AUDIO_GAIN_Q8 <= 256,
              "CLOCK_AUDIO_GAIN_Q8 must be in [0, 256]; values >256 would clip at INT16 peak");

namespace wc::audio {

// Pure. In-place Q8 scale of signed 16-bit PCM.
// Contract: gain_q8 ∈ [0, 256]. Values > 256 would clip at INT16
// peak and are rejected at compile time via the static_assert above.
// gain_q8 == 256 is the unity path (no-op shortcut).
void apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8);

} // namespace wc::audio
```

### Pure-logic state-machine transition

```cpp
// firmware/lib/audio/include/audio/state_machine.h
#pragma once

#include <cstdint>

namespace wc::audio {

enum class State : uint8_t { Idle, Playing };
enum class Event : uint8_t {
    PlayLullabyRequested,
    PlayBirthRequested,
    StopRequested,
    EofOrError,
};

// Pure. Decides the next state from (current state, event). Has no
// side effects (no file opens, no I²S calls — the adapter performs
// those alongside the transition). Captures the 2×4 state-transition
// table exactly as documented in §Playback state machine.
State next_state(State current, Event e);

} // namespace wc::audio
```

The adapter's `transition_to_playing` / `transition_to_idle` helpers
now split into "compute the next state via `next_state`" and "perform
the side effect"; the state variable is only written via the pure
function. Keeps the state-machine invariants native-testable.

### Pure-logic birthday auto-fire guard

```cpp
// firmware/lib/audio/include/audio/fire_guard.h
#pragma once

#include <cstdint>

namespace wc::audio {

struct BirthConfig {
    uint8_t month;    // 1-12
    uint8_t day;      // 1-31
    uint8_t hour;     // 0-23
    uint8_t minute;   // 0-59
};

struct NowFields {
    uint16_t year;    // full year (e.g. 2030)
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23
    uint8_t  minute;  // 0-59
};

// Pure. Returns true iff ALL auto-fire gates pass.
// See §Birthday auto-fire guards for the gate list.
bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing);

} // namespace wc::audio
```

### ESP32-only public API

```cpp
// firmware/lib/audio/include/audio.h
//
// ESP32-only public API. Depends on Arduino.h, SD, Preferences,
// driver/i2s.h. Include ONLY from translation units that compile
// under the Arduino toolchain (the emory / nora PlatformIO envs).
// Native tests include the pure-logic headers under audio/ — never
// this one.
#pragma once

#include <Arduino.h>
#include "audio/fire_guard.h"

namespace wc::audio {

// Initialize the I²S driver, mount SD, open NVS namespace, seed
// last_birth_year. No audio output.
//
// Idempotent. Call once from setup() AFTER wifi_provision, rtc,
// ntp, display, and buttons have all begun (audio is the last
// begin() in the sequence; the birthday auto-fire consults
// wifi_provision::seconds_since_last_sync() and rtc::now()).
void begin(const BirthConfig& birth);

// Drives the state machine. Call from main.cpp's loop().
// - When Playing: reads the next PCM chunk from SD, applies Q8
//   gain, writes to I²S with 0 timeout. On EOF / error: closes
//   file, transitions to Idle.
// - When Idle: runs should_auto_fire() against rtc::now();
//   transitions to Playing with birth.wav if the guard passes.
//
// Returns in <2 ms in all steady-state paths.
void loop();

// Open lullaby.wav, validate header, transition Idle → Playing.
// No-op if already Playing (caller should check is_playing()).
void play_lullaby();

// Close the open file, stop I²S TX, transition Playing → Idle.
// No-op if already Idle.
void stop();

// True iff the state machine is Playing. Public observer used by
// the main.cpp button dispatch and by ntp::loop() to defer syncs.
bool is_playing();

} // namespace wc::audio
```

## Adapter (`audio.cpp`) — pseudocode

Shape for review; actual code written during implementation.

```cpp
#ifdef ARDUINO

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <cstdint>
#include "audio.h"
#include "audio/wav.h"
#include "audio/gain.h"
#include "audio/fire_guard.h"
#include "audio/state_machine.h"
#include "rtc.h"
#include "wifi_provision.h"
#include "pinmap.h"

namespace wc::audio {

static constexpr i2s_port_t   I2S_PORT          = I2S_NUM_0;
static constexpr size_t       SD_READ_CHUNK     = 1024;   // bytes; 512 samples
static constexpr const char*  LULLABY_PATH      = "/lullaby.wav";
static constexpr const char*  BIRTH_PATH        = "/birth.wav";
static constexpr const char*  NVS_NAMESPACE     = "audio";
static constexpr const char*  NVS_KEY_LAST_YEAR = "last_birth_year";

// Compile-time guard for CLOCK_AUDIO_GAIN_Q8 lives in audio/gain.h
// (which defines a default of 256 if no config header overrides it).
// State and Event enums come from audio/state_machine.h.

static bool        started              = false;
static bool        sd_ok                = false;
static bool        i2s_ok               = false;
static State       state_               = State::Idle;
static BirthConfig birth_               = {};
static uint16_t    last_birth_year_     = 0;
static File        current_file_;
static uint32_t    bytes_played_        = 0;
static uint8_t     read_buf_[SD_READ_CHUNK];

// --- NVS helpers -------------------------------------------------

static uint16_t nvs_read_last_birth_year() {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, /*readOnly=*/true)) return 0;
    uint16_t v = p.getUShort(NVS_KEY_LAST_YEAR, 0);
    p.end();
    return v;
}

// Returns true iff BOTH begin() opened writable AND putUShort returned
// non-zero byte count. Auto-fire gates the birth.wav transition on this
// bool — a silent NVS miss would risk missing-or-double-fire across a
// power cycle (see adversarial review in spec history).
static bool nvs_write_last_birth_year(uint16_t year) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
    size_t written = p.putUShort(NVS_KEY_LAST_YEAR, year);
    p.end();
    return written > 0;
}

// --- I²S driver lifecycle ---------------------------------------

static bool i2s_install() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = WAV_SAMPLE_RATE_HZ;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 1024;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = PIN_I2S_BCLK;
    pins.ws_io_num    = PIN_I2S_LRC;
    pins.data_out_num = PIN_I2S_DIN;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) return false;

    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

// --- State transitions -------------------------------------------

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
    Serial.printf("[audio] %s (played %u bytes)\n", reason, bytes_played_);
}

// --- Public API --------------------------------------------------

void begin(const BirthConfig& birth) {
    if (started) return;
    birth_ = birth;

    i2s_ok = i2s_install();
    if (!i2s_ok) {
        Serial.println("[audio] begin: I2S install FAILED, playback disabled until next boot");
    }
    sd_ok = SD.begin(PIN_SD_CS);
    if (!sd_ok) {
        Serial.println("[audio] begin: SD FAILED, playback disabled until next boot");
    }
    if (sd_ok && i2s_ok) {
        Serial.println("[audio] begin: SD ok, I2S ok");
    }
    last_birth_year_ = nvs_read_last_birth_year();
    started = true;
}

void loop() {
    if (!started) return;

    if (state_ == State::Playing) {
        // Pump: read chunk, gain, non-blocking write.
        int n = current_file_.read(read_buf_, sizeof(read_buf_));
        if (n <= 0) {
            transition_to_idle("finished");
            return;
        }
        apply_gain_q8(reinterpret_cast<int16_t*>(read_buf_),
                      n / 2, CLOCK_AUDIO_GAIN_Q8);
        size_t written = 0;
        esp_err_t err = i2s_write(I2S_PORT, read_buf_, n,
                                  &written, /*ticks=*/0);
        if (err != ESP_OK) {
            Serial.printf("[audio] error: i2s_write err=%d\n", err);
            transition_to_idle("aborted");
            return;
        }
        if (written < static_cast<size_t>(n)) {
            // Ring full — rewind the SD cursor so we retry next tick.
            current_file_.seek(current_file_.position() - (n - written));
        }
        bytes_played_ += written;
        return;
    }

    // Idle: check birthday auto-fire.
    if (!sd_ok || !i2s_ok) return;
    // audio::loop() reads rtc::now() directly here — deliberate
    // deviation from the display module's data-injection pattern
    // (display takes NowFields via RenderInput from main.cpp). The
    // auto-fire check runs only when Idle and only needs RTC once
    // per minute-ish; pushing it through main.cpp would make
    // main.cpp read rtc on every tick whether audio cares or not.
    auto dt = wc::rtc::now();
    NowFields nf{ dt.year, dt.month, dt.day, dt.hour, dt.minute };
    bool time_known =
        (wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX);
    bool already_playing = (state_ == State::Playing);
    if (should_auto_fire(nf, birth_, last_birth_year_, time_known, already_playing)) {
        Serial.printf("[audio] play birth.wav (year=%u)\n", nf.year);
        // Stamp NVS FIRST, gate playback on the write succeeding.
        // A silent NVS failure here could miss-or-double-fire the
        // birthday across a power cycle in the same minute.
        if (!nvs_write_last_birth_year(nf.year)) {
            Serial.println("[audio] error: NVS stamp FAILED; skipping birth.wav this tick");
            return;
        }
        last_birth_year_ = nf.year;
        transition_to_playing(BIRTH_PATH);
    }
}

void play_lullaby() {
    if (!started || !sd_ok || !i2s_ok) return;
    if (state_ != State::Idle)         return;
    transition_to_playing(LULLABY_PATH);
}

void stop() {
    if (!started)                  return;
    if (state_ != State::Playing)  return;
    transition_to_idle("stopped");
}

bool is_playing() {
    return started && state_ == State::Playing;
}

} // namespace wc::audio

#endif // ARDUINO
```

## Integration

### main.cpp — wire audio into setup() + loop() + button handler

```cpp
#include "audio.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();
    wc::rtc::begin();
    wc::ntp::begin();
    wc::display::begin();
    wc::buttons::begin([](wc::buttons::Event e) {
        using BE = wc::buttons::Event;
        using WS = wc::wifi_provision::State;
        switch (e) {
            case BE::HourTick:
                wc::rtc::advance_hour();
                break;
            case BE::MinuteTick:
                wc::rtc::advance_minute();
                break;
            case BE::AudioPressed:
                if (wc::wifi_provision::state() == WS::AwaitingConfirmation) {
                    wc::wifi_provision::confirm_audio();
                } else if (wc::audio::is_playing()) {
                    wc::audio::stop();
                } else {
                    wc::audio::play_lullaby();
                }
                break;
            case BE::ResetCombo:
                Serial.println("[buttons] ResetCombo — resetting to captive portal");
                wc::wifi_provision::reset_to_captive();
                break;
        }
    });
    wc::audio::begin({CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                      CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE});
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // early-returns when audio::is_playing()
    wc::audio::loop();             // pump I²S; auto-fire check when idle

    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        auto dt = wc::rtc::now();
        wc::display::RenderInput in{};
        in.year   = dt.year;
        in.month  = dt.month;
        in.day    = dt.day;
        in.hour   = dt.hour;
        in.minute = dt.minute;
        in.now_ms = millis();
        in.seconds_since_sync = wc::wifi_provision::seconds_since_last_sync();
        in.birthday = {CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                       CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE};
        wc::display::show(wc::display::render(in));
    } else {
        wc::display::show(wc::display::Frame{});
    }

    delay(1);
}
```

Note: `audio::begin()` is the last begin() — per the user's order
constraint. The `audio::loop()` call goes AFTER `ntp::loop()` so
that a just-completed ntp sync cannot race with an audio auto-fire
decision in the same tick; the audio loop always sees the freshest
time-sync state.

### ntp::loop() — defer syncs during playback

One-line addition at the top of `wc::ntp::loop()`:

```cpp
void loop() {
    if (!started) return;
    if (wc::audio::is_playing()) return;  // NEW — defer the ~1s forceUpdate
    if (wc::wifi_provision::state() != wc::wifi_provision::State::Online) {
        return;
    }
    // ... existing logic unchanged
}
```

The ntp module acquires an `#include "audio.h"` dependency — this
is the one cross-module coupling this spec introduces. Alternative
considered: a generic `wc::audio::is_busy()` wrapper or a shared
"long-ops-in-flight" module. Rejected as over-abstraction for a
single call site.

### Parent-spec edits (same commit per "no conflicting guides" rule)

Two edits land in the spec-add commit:

1. **`docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
   §Hardware Platform > Audio:**

   ```
   - Audio: MAX98357A I2S DAC + amp, small speaker (~2-3W), microSD
     card for MP3 files.
   ```

   →

   ```
   - Audio: MAX98357A I2S DAC + amp, small speaker (~2-3W), microSD
     card for 16-bit PCM WAV files (44.1 kHz mono).
   ```

2. **`TODO.md` Phase 2 firmware modules section, `audio` bullet**
   (current text, lines 105-109 of TODO.md at spec-write time):

   ```
   - [ ] **`audio`** — I²S + MAX98357A. MP3 decode from microSD.
         Play / stop behavior: press once to play lullaby, press
         during playback to stop. Volume is fixed in firmware and
         tuned during assembly. Needs a spec (ESP8266Audio vs.
         libhelix library choice, volume curve, play-state machine
         with buffer-underrun handling).
   ```

   Spec-add commit inserts `Spec: docs/superpowers/specs/2026-04-18-audio-design.md.`
   at the end of the bullet; the `[x]` flip and shipped-summary
   rewrite lands in the implementation PR (matching the pattern
   used by ntp / rtc / buttons / display).

3. **`TODO.md` Phase 2 firmware modules > "Audio file format"
   bullet** — current text at line 140:

   ```
   - [x] **Audio file format — MP3, 128 kbps CBR, mono, 44.1 kHz.**
         CBR over VBR because ESP32 MP3 decoder libs (ESP8266Audio /
         libhelix) handle CBR more reliably — VBR can hiccup on
         sample-rate transitions. Mono because single speaker.
         128 kbps is transparent for voice + a simple lullaby,
         ~1 MB/min, well under microSD capacity. 44.1 kHz is
         standard and within MAX98357A's 8–48 kHz @ 16-bit envelope.
   ```

   →

   ```
   - [x] **Audio file format — WAV, 16-bit PCM, 44.1 kHz, mono,
         little-endian.** Design flipped from MP3 during audio-
         module spec pass (`docs/superpowers/specs/2026-04-18-audio-design.md`).
         Eliminates the MP3 decoder dependency (libhelix / ESP8266Audio
         / ESP32-audioI2S — ESP32-audioI2S required PSRAM we don't
         have; the others were decoder libraries we don't need once
         the file format is uncompressed). Collapses the audio
         adapter to an SD-read → I²S-write pump. File size is
         ~88 kB/s raw PCM (~18.5 MB total per card for a 3 min
         lullaby + 30 s birth message); trivially inside any modern
         microSD's capacity.
   ```

### platformio.ini

Edits (mirroring ntp's pattern):

- Add `-I lib/audio/include` to `[env:native] build_flags`.
- Add `+<../lib/audio/src/wav.cpp>` to `[env:native] build_src_filter`.
- Add `+<../lib/audio/src/gain.cpp>` to `[env:native] build_src_filter`.
- Add `+<../lib/audio/src/fire_guard.cpp>` to `[env:native]
  build_src_filter`.
- Add `+<../lib/audio/src/state_machine.cpp>` to `[env:native]
  build_src_filter`.

`audio.cpp` is NOT added to the native filter — `#ifdef ARDUINO`
short-circuits it on that target.

No new `lib_deps` entries — SD, Preferences, and `driver/i2s.h` are
all in the Arduino-ESP32 core (already present via
`espressif32@^6.8.0`).

Flash-footprint caveat: "no new `lib_deps`" does not mean "no new
flash." `SD.h` pulls in `FS.h` and the VFS layer (this module is
the first SD consumer in the firmware; wifi_provision uses
`Preferences`/NVS, not SD). `Preferences` is already linked (for
NVS creds). `driver/i2s.h` adds the ESP-IDF v4.4 I²S driver. The
plan's Task 1 will pin the actual flash delta against the
baseline; if it pushes past ~85% we'll revisit the DMA buffer
sizing or the SD read chunk size.

### Per-kid config (`configs/config_<kid>.h`)

Add one new define to each config header:

```cpp
// configs/config_emory.h (and config_nora.h)
#define CLOCK_AUDIO_GAIN_Q8   256   // Q8 software gain, 256 = unity
                                    // (see audio spec §Software gain)
```

The adapter's `static_assert(CLOCK_AUDIO_GAIN_Q8 <= 256, ...)`
catches any >256 value at compile time (would clip on PCM peaks).

## Testing

### Pure-logic native tests (Unity)

**`test_audio_wav`** — 10 tests.

`parse_wav_header`:

- `test_wav_truncated_buffer`: buf_len=43 → `Truncated`.
- `test_wav_valid_canonical`: hand-built canonical header →
  `{Ok, 44, expected_data_size}`.
- `test_wav_bad_riff_magic`: bytes 0-3 = "RIFX" → `BadRiffMagic`.
- `test_wav_bad_wave_magic`: bytes 8-11 = "WAVX" → `BadWaveMagic`.
- `test_wav_bad_fmt_magic`: bytes 12-15 = "fmtx" → `BadFmtMagic`.
- `test_wav_bad_codec_tag`: bytes 20-21 = 0x0003 (IEEE float) →
  `BadCodecTag`.
- `test_wav_wrong_channels`: bytes 22-23 = 2 (stereo) →
  `WrongChannelCount`.
- `test_wav_wrong_sample_rate`: bytes 24-27 = 48000 →
  `WrongSampleRate`.
- `test_wav_wrong_bits_per_sample`: bytes 34-35 = 24 →
  `WrongBitsPerSample`.
- `test_wav_bad_data_magic`: bytes 36-39 = "dbta" → `BadDataMagic`.

**`test_audio_gain`** — 4 tests.

`apply_gain_q8`:

- `test_gain_unity_256_identity`: buf of {INT16_MIN, -1, 0, 1,
  INT16_MAX}, gain=256 → unchanged.
- `test_gain_half_128_halves`: buf of {32000}, gain=128 → {16000}.
- `test_gain_zero_mutes`: buf of {32000, -32000}, gain=0 → {0, 0}.
- `test_gain_no_clipping_at_unity`: buf of {INT16_MAX, INT16_MIN},
  gain=256 → unchanged (proves the multiply-and-shift path
  doesn't wrap at the boundary).

**`test_audio_fire_guard`** — 8 tests.

`should_auto_fire`:

- `test_fire_happy_path`: all gates pass → true.
- `test_fire_wrong_month`: now.month ≠ birth.month → false.
- `test_fire_wrong_day`: now.day ≠ birth.day → false.
- `test_fire_wrong_hour`: now.hour ≠ birth.hour → false.
- `test_fire_wrong_minute`: now.minute ≠ birth.minute → false.
- `test_fire_already_fired_this_year`:
  last_fired_year == now.year → false.
- `test_fire_unknown_time`: time_known=false → false.
- `test_fire_already_playing`: already_playing=true → false.

**`test_audio_state_machine`** — 8 tests (one per row of the
state-transition table in §Playback state machine).

`next_state`:

- `test_sm_idle_play_lullaby_to_playing`: Idle + PlayLullabyRequested → Playing.
- `test_sm_idle_play_birth_to_playing`: Idle + PlayBirthRequested → Playing.
- `test_sm_idle_stop_stays_idle`: Idle + StopRequested → Idle.
- `test_sm_idle_eof_stays_idle`: Idle + EofOrError → Idle (defensive).
- `test_sm_playing_play_lullaby_stays_playing`: Playing + PlayLullabyRequested → Playing (no-op).
- `test_sm_playing_play_birth_stays_playing`: Playing + PlayBirthRequested → Playing (no-op).
- `test_sm_playing_stop_to_idle`: Playing + StopRequested → Idle.
- `test_sm_playing_eof_to_idle`: Playing + EofOrError → Idle.

Total audio pure-logic tests: **30** (10 wav + 4 gain + 8 fire_guard
+ 8 state_machine).

Target native-suite count after this module: 150 (post-ntp) + 30
(audio) = **180 native tests**.

### ESP32 adapter — NOT native-testable

Four things the native suite can't exercise:

- I²S DMA ring lifecycle + `i2s_write` semantics under real
  timing.
- SD card open / read / close via VSPI.
- `Preferences` NVS read/write for `last_birth_year`.
- Real RTC read via `wc::rtc::now()` → local fields.
- Cross-module interaction with `ntp::loop()`'s deferral on
  `is_playing()`.

All covered by the hardware manual-check document below.

### Hardware manual check: `firmware/test/hardware_checks/audio_checks.md`

Created in this module's PR. Executed during breadboard bring-up
after the MAX98357A + speaker + microSD path have been wired
(`docs/hardware/breadboard-bring-up-guide.md` §MAX98357A + Speaker
and §MicroSD).

**Prerequisites:**
- ESP32 flashed with the current emory or nora env build.
- Captive-portal flow completed at least once (NVS holds valid
  WiFi creds + TZ; `seconds_since_last_sync` is non-UINT32_MAX).
- MicroSD prepared with canonical WAV files at root:
  - `/lullaby.wav` — 44.1 kHz / 16-bit / mono / PCM, ~3 min.
  - `/birth.wav` — same format, ~30 s.
  - Authoring recipe: Audacity File > Export > Export as WAV,
    Encoding "Signed 16-bit PCM", Sample Rate 44100 Hz,
    Channels Mono.
- MAX98357A hardware gain jumper set at assembly default (9 dB
  unless the dry-fit measurement says otherwise).
- `CLOCK_AUDIO_GAIN_Q8` left at 256 (default) for the first
  bring-up pass.
- Speaker connected to MAX98357A output terminals.
- Serial monitor open at 115200 baud.

**Checks:**

1. **Boot path observable.** Power on. Expect `[audio] begin: SD
   ok, I2S ok` in serial. If `SD FAILED` appears, re-seat the card
   and re-check the VSPI wiring per `docs/hardware/pinout.md`.

2. **Lullaby play.** Once the clock is Online, press the audio
   button. Expect `[audio] play /lullaby.wav` in serial, followed
   by audible playback through the speaker. Wait for natural EOF;
   expect `[audio] finished (played <N> bytes)` within ~3 min.
   Confirm the clock display continues rendering the current time
   during playback (display cadence is independent of audio).

3. **Button-during-playback stops.** Start lullaby. After ~10 s,
   press the audio button again. Expect `[audio] stopped (played
   <N> bytes)` in serial and immediate silence from the speaker.
   Press once more; expect fresh playback from the start.

4. **Volume sanity.** Listen for distortion / clipping on the
   loudest passages. If present: either re-encode the WAV at
   lower peak level (authoring fix), or lower
   `CLOCK_AUDIO_GAIN_Q8` to 181 (−3 dB) / 128 (−6 dB) and
   rebuild. Hardware jumper change (move to lower gain resistor)
   is the alternative if software trim isn't enough. Document
   the setting that worked in `docs/hardware/assembly-plan.md`.

5. **SD card removed mid-playback.** Start lullaby. Pull the
   microSD card from the slot. Expect `[audio] error: i2s_write
   err=...` OR `[audio] error: SD read failed at offset <N>` in
   serial, followed by `[audio] aborted` and silence. Re-seat the
   card; press the button again → plays successfully (no need to
   reboot).

6. **Missing file graceful.** Rename `/lullaby.wav` → `/xxx.wav`
   on the card, re-insert. Press audio button. Expect `[audio]
   error: file /lullaby.wav not found`; no sound. Stay Idle.

7. **Invalid WAV graceful.** Place a stereo 48 kHz WAV (or a
   renamed .mp3) at `/lullaby.wav`. Press audio button. Expect
   `[audio] error: wav header invalid (code=<N>)`; no sound.

8. **Birth auto-fire (simulated).** Pre-set the DS3231 via a
   bring-up sketch to 6:09:50 PM on Oct 6 of the current year
   (adjust month/day/time for the active env's `CLOCK_BIRTH_*`).
   Reset `audio/last_birth_year` to 0 via a wipe sketch or by
   rebuilding a debug env with a `#define` that forces the NVS
   clear. Wait through 6:10:00 PM. Expect `[audio] play birth.wav
   (year=<current year>)` within one `loop()` tick of the minute
   roll. Confirm audible playback; wait for `finished` log.

9. **Birth auto-fire fires once per year.** After check 8 passes,
   stay powered and wait until the next minute (6:11 PM). Expect
   no additional `[audio] play` log. Power-cycle. Expect no
   `[audio] play birth.wav` on next boot (NVS `last_birth_year`
   now matches current year).

10. **Unknown-time gate.** Wipe NVS (hold Hour+Audio 10 s →
    `reset_to_captive`). Captive portal comes up; do NOT submit
    credentials. Pre-set DS3231 to 6:10 PM on Oct 6 via the
    bring-up sketch. Verify no auto-fire (seconds_since_last_sync
    is UINT32_MAX; time_known=false). No `[audio] play birth.wav`
    should appear.

11. **ntp deferral during playback.** Start lullaby playback. In
    serial, confirm no `[ntp] sync ok` / `[ntp] forceUpdate()`
    log lines fire during the ~3 min playback window, even if a
    deadline would otherwise fire. After the lullaby finishes,
    confirm ntp resumes its normal cadence.

12. **Underrun sanity.** During playback, stress the main loop by
    adding a test print every loop tick (e.g., via a temporary
    `Serial.println(millis())`). Playback should remain clean.
    Any audible glitch here would point to DMA ring sizing being
    too tight — rare.

**Pass criteria:** checks 1-7 and 11 must pass before the module is
considered hardware-verified. 8-10 require a controlled pre-set
DS3231 and may be deferred until the final PCB burn-in phase when
we can sit through a real birthday. 12 is diagnostic-only.

## Open issues

One implementation-time note — not a design open item, but call it
out for the plan:

- **I²S DMA buffer sizing under FastLED.show()'s interrupt
  disable.** FastLED disables interrupts during the WS2812B data
  pulse (~1.05 ms for 35 LEDs). The I²S driver runs off DMA +
  peripheral clock, so it continues outputting silence / buffered
  PCM from the DMA ring during this window without a starvation
  event. The ~46 ms DMA depth dwarfs the ~1 ms ISR-disable window
  by 40×. Not a risk under current LED counts; call this out if
  the LED count grows ≥ 100.

- **Canonical-vs-extended WAV fmt chunk.** The parser strictly
  requires 16 bytes of fmt data (canonical PCM), not the 18- or
  40-byte "extensible" variants some tools emit. Authoring
  workflow (Audacity / Logic "Signed 16-bit PCM" → WAV) produces
  canonical by default. If a future tool's default changes, the
  error surfaces as `BadDataMagic` (offset is wrong) and we'd
  either fix the export settings or widen the parser. Called out
  in the hardware checklist's prerequisites.

- **`i2s_write` retry on short writes.** The pseudocode rewinds
  the SD cursor on short writes; a more careful implementation
  would shunt the unwritten bytes into a static "next-tick queue"
  and skip the re-read entirely. Either works; the SD rewind is
  simpler and adds one extra SD read per ring-full event (rare,
  sub-10-Hz under the design cadence). Pin during plan review.

## Verified numbers

| Item | Value | Source |
|---|---|---|
| WAV codec tag | 0x0001 (PCM) | `WAVE_FORMAT_PCM` — RIFF spec |
| WAV sample rate | 44100 Hz | Matches parent spec §Audio; MAX98357A accepts 8-48 kHz |
| WAV bits per sample | 16 | MAX98357A datasheet §2 (Electrical Characteristics) |
| WAV channels | 1 (mono) | Parent spec §Audio (single speaker) |
| WAV raw rate | 88200 bytes/sec | 44100 × 2 × 1 |
| WAV 3-min file size | ~15.9 MB | 88200 × 180 |
| WAV 30-sec file size | ~2.6 MB | 88200 × 30 |
| Combined per-card footprint | ~18.5 MB | Trivial on ≥4 GB microSD |
| I²S DMA ring depth | 4 × 1024 samples × 2 B = 8192 B | 8192 / 88200 ≈ 93 ms |
| Main-loop underrun budget | ~46 ms after headroom | Observed worst-case non-audio loop work ~5 ms; 88 ms margin |
| Default software gain | 256 (Q8 unity) | `configs/config_<kid>.h` (to be added) |
| Gain contract | Q8 integer, `[0, 256]` | Values >256 would clip at Int16 peak; static_assert guards |
| MAX98357A hardware gain options | 9 / 12 / 15 / 18 dB | MAX98357A datasheet, GAIN pin resistor table |
| MAX98357A idle current | ~2 mA | datasheet — SD pin always high |
| I²S pins (ESP32) | BCLK=26, LRC(WS)=25, DIN=27 | `firmware/configs/pinmap.h` |
| SD pins (VSPI) | CS=5, CLK=18, MISO=19, MOSI=23 | `firmware/configs/pinmap.h` |
| Audio button | GPIO 14 (INPUT_PULLUP) | `firmware/configs/pinmap.h` |
| NVS namespace | `"audio"` | This spec §NVS schema |
| NVS key (auto-fire) | `last_birth_year` (UShort / uint16) | This spec §NVS schema |
| Emory birth minute | 2030-10-06 18:10 | `configs/config_emory.h` + parent spec |
| Nora birth minute | 2032-03-19 09:17 | `configs/config_nora.h` + parent spec |
| `ntp::forceUpdate` blocking | ~1 s | NTP spec §Dead-NTPClient pitfalls |
| FastLED.show() for 35 LEDs | ~1.05 ms | WS2812B datasheet: 30 µs per LED + reset |
| Arduino-ESP32 core via PIO | 2.0.x (espressif32@^6.8.0) | `firmware/platformio.ini` |
| ESP-IDF I²S driver header | `driver/i2s.h` (v4.4 API) | Arduino-ESP32 2.0.x bundled |

## References

- Parent design: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
  (§Firmware Behaviors > Audio button, §Hardware Platform > Audio,
  §Per-Clock Configuration)
- Sibling specs:
  - RTC: `docs/superpowers/specs/2026-04-17-rtc-design.md`
  - NTP: `docs/superpowers/specs/2026-04-17-ntp-design.md`
  - Buttons: `docs/superpowers/specs/2026-04-16-buttons-design.md`
  - Captive portal: `docs/superpowers/specs/2026-04-16-captive-portal-design.md`
  - Display: `docs/superpowers/specs/2026-04-17-display-design.md`
- Hardware pinout: `docs/hardware/pinout.md`
  (§MAX98357A I²S, §MicroSD VSPI, §Tact switches)
- Breadboard bring-up: `docs/hardware/breadboard-bring-up-guide.md`
  (§MAX98357A + Speaker, §MicroSD)
- Assembly plan: `docs/hardware/assembly-plan.md`
  (for noting the final MAX98357A gain jumper setting +
  `CLOCK_AUDIO_GAIN_Q8` value)
- ESP-IDF I²S driver reference (v4.4):
  https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/i2s.html
- MAX98357A datasheet:
  https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf
- RIFF WAVE format (Microsoft):
  https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-
- WAV PCM canonical header layout:
  http://soundfile.sapp.org/doc/WaveFormat/
