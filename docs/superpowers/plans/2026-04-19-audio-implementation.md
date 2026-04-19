# Audio Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `audio` firmware module — a hexagonal-split audio path
that plays `lullaby.wav` on button press (press-again = stop) and
auto-plays `birth.wav` at the exact birth minute on the birthday,
once per year. Pure 16-bit PCM WAV decode: no MP3 library, no
PSRAM requirement. Main-loop-integrated pump with `ntp::loop()`
deferring its ~1 s `forceUpdate()` during playback.

**Architecture:** Hexagonal split. Pure-logic `wav` (RIFF header
validation), `gain` (Q8 PCM scaling), and `fire_guard` (birthday
auto-fire gates) are native-testable with Unity. ESP32 adapter
(`audio.cpp`) wraps `driver/i2s.h` + `SD.h` + `Preferences.h`
under `#ifdef ARDUINO`. Pumping is non-blocking `i2s_write` from
main loop; `audio::is_playing()` is the observer that `ntp::loop()`
reads to skip the 1 s `forceUpdate` during playback.

**Tech Stack:** Arduino-ESP32 core 2.0.x (via `espressif32@^6.8.0`),
ESP-IDF v4.4 I²S legacy driver (`driver/i2s.h`), SD (Arduino-ESP32
built-in), Preferences (NVS wrapper), PlatformIO, Unity C++ test
framework. **No new `lib_deps`.**

---

## Spec reference

Implements `docs/superpowers/specs/2026-04-18-audio-design.md`.
Read the spec in full before starting — every decision (file format
WAV not MP3, state machine shape, NVS namespace, I²S DMA sizing,
Q8 gain, NVS-write bool gating on auto-fire path, ntp deferral
coupling) is pinned there. If you hit something that feels
underspecified, re-read the spec before inventing.

## File structure

```
firmware/
  lib/audio/                          # NEW
    include/
      audio.h                         # ESP32-only public API
      audio/
        wav.h                         # parse_wav_header + constants + errors
        gain.h                        # apply_gain_q8 + CLOCK_AUDIO_GAIN_Q8 default
        fire_guard.h                  # should_auto_fire + NowFields + BirthConfig
    src/
      audio.cpp                       # adapter (#ifdef ARDUINO)
      wav.cpp                         # pure-logic RIFF header validation
      gain.cpp                        # pure-logic Q8 scalar
      fire_guard.cpp                  # pure-logic birthday guard
  lib/ntp/
    src/ntp.cpp                       # MODIFIED — add audio::is_playing() early-return
  test/                               # NEW test dirs
    test_audio_wav/test_wav.cpp
    test_audio_gain/test_gain.cpp
    test_audio_fire_guard/test_fire_guard.cpp
    hardware_checks/audio_checks.md   # NEW
  platformio.ini                      # MODIFIED — add audio include + pure .cpps
  src/main.cpp                        # MODIFIED — audio::begin + loop +
                                      #   button-handler precedence update
configs/
  config_emory.h                      # MODIFIED (optional) — CLOCK_AUDIO_GAIN_Q8
  config_nora.h                       # MODIFIED (optional) — CLOCK_AUDIO_GAIN_Q8
docs/superpowers/specs/
  2026-04-14-daughters-clocks-design.md   # MODIFIED — MP3 → WAV
TODO.md                               # MODIFIED — MP3 → WAV bullet, mark audio [x]
```

Pure-logic `.cpp` files (`wav`, `gain`, `fire_guard`) go into
`[env:native]`'s `build_src_filter`. `audio.cpp` is guarded with
`#ifdef ARDUINO` — same reason as `wifi_provision`, `buttons`,
`display`, `rtc`, `ntp` adapters (PlatformIO LDF `deep+`
otherwise pulls it into the native build).

## Build verification recipes (used throughout)

- Native test suite: `cd firmware && pio test -e native`
  - **Baseline (post-ntp): 150 tests pass.**
  - **After Tasks 2-4 land: 172 tests pass** (+10 wav +4 gain +8 fire_guard).
- ESP32 build (Emory): `cd firmware && pio run -e emory`
- ESP32 build (Nora):  `cd firmware && pio run -e nora`

Run the relevant build before every commit.

---

## Task 1: Module scaffold (empty stubs + platformio.ini wiring)

Sets up the `lib/audio/` directory with empty stubs that compile
cleanly on all envs. No tests yet, no behavior. Locks the module
shape into the build system before any TDD starts.

**Files:**
- Create: `firmware/lib/audio/include/audio.h`
- Create: `firmware/lib/audio/include/audio/wav.h`
- Create: `firmware/lib/audio/include/audio/gain.h`
- Create: `firmware/lib/audio/include/audio/fire_guard.h`
- Create: `firmware/lib/audio/src/audio.cpp`
- Create: `firmware/lib/audio/src/wav.cpp`
- Create: `firmware/lib/audio/src/gain.cpp`
- Create: `firmware/lib/audio/src/fire_guard.cpp`
- Modify: `firmware/platformio.ini`

- [ ] **Step 1: Create the public ESP32 header (stub)**

Write `firmware/lib/audio/include/audio.h`:

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
// last_birth_year. No audio output. Idempotent. Call once from
// setup() AFTER wifi_provision, rtc, ntp, display, and buttons
// have all begun.
void begin(const BirthConfig& birth);

// Drives the state machine. Call from main.cpp's loop().
// - When Playing: reads the next PCM chunk from SD, applies Q8
//   gain, writes to I²S with 0 timeout. On EOF / error: closes
//   file, transitions to Idle.
// - When Idle: runs should_auto_fire() against rtc::now();
//   transitions to Playing with birth.wav if the guard passes.
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

- [ ] **Step 2: Create the pure-logic WAV header (stub)**

Write `firmware/lib/audio/include/audio/wav.h`:

```cpp
// firmware/lib/audio/include/audio/wav.h
#pragma once

#include <cstdint>
#include <cstddef>

namespace wc::audio {

// Canonical WAV constants — a file matching these is our only
// accepted format. See audio spec §WAV format validation.
constexpr uint16_t WAV_PCM_CODEC_TAG        = 0x0001;
constexpr uint16_t WAV_CHANNEL_COUNT_MONO   = 1;
constexpr uint32_t WAV_SAMPLE_RATE_HZ       = 44100;
constexpr uint16_t WAV_BITS_PER_SAMPLE      = 16;
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
// If buf_len < 44, returns Truncated. On success returns
// {Ok, 44, data_size}.
ParseResult parse_wav_header(const uint8_t* buf, size_t buf_len);

} // namespace wc::audio
```

- [ ] **Step 3: Create the pure-logic gain header (stub)**

Write `firmware/lib/audio/include/audio/gain.h`:

```cpp
// firmware/lib/audio/include/audio/gain.h
#pragma once

#include <cstdint>
#include <cstddef>

// Default software gain (Q8). Unity = 256. Per-kid configs may
// override via -D or #define BEFORE including this header. Kept
// here so the module compiles cleanly even if a config header
// hasn't been updated yet.
#ifndef CLOCK_AUDIO_GAIN_Q8
#define CLOCK_AUDIO_GAIN_Q8 256
#endif

static_assert(CLOCK_AUDIO_GAIN_Q8 >= 0 && CLOCK_AUDIO_GAIN_Q8 <= 256,
              "CLOCK_AUDIO_GAIN_Q8 must be in [0, 256]; values >256 would clip at INT16 peak");

namespace wc::audio {

// Pure. In-place Q8 scale of signed 16-bit PCM.
// Contract: gain_q8 ∈ [0, 256]. gain_q8 == 256 is unity (no-op shortcut).
void apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8);

} // namespace wc::audio
```

- [ ] **Step 4: Create the pure-logic fire-guard header (stub)**

Write `firmware/lib/audio/include/audio/fire_guard.h`:

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
// See audio spec §Birthday auto-fire guards for the gate list.
bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing);

} // namespace wc::audio
```

- [ ] **Step 5: Create the wav.cpp stub**

Write `firmware/lib/audio/src/wav.cpp`:

```cpp
// firmware/lib/audio/src/wav.cpp
#include "audio/wav.h"

namespace wc::audio {

// Implementation lands in Task 2 (TDD).

} // namespace wc::audio
```

- [ ] **Step 6: Create the gain.cpp stub**

Write `firmware/lib/audio/src/gain.cpp`:

```cpp
// firmware/lib/audio/src/gain.cpp
#include "audio/gain.h"

namespace wc::audio {

// Implementation lands in Task 3 (TDD).

} // namespace wc::audio
```

- [ ] **Step 7: Create the fire_guard.cpp stub**

Write `firmware/lib/audio/src/fire_guard.cpp`:

```cpp
// firmware/lib/audio/src/fire_guard.cpp
#include "audio/fire_guard.h"

namespace wc::audio {

// Implementation lands in Task 4 (TDD).

} // namespace wc::audio
```

- [ ] **Step 8: Create the adapter .cpp stub (under #ifdef ARDUINO)**

Write `firmware/lib/audio/src/audio.cpp`:

```cpp
// firmware/lib/audio/src/audio.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag SD/driver-i2s into the native build.
// Same pattern as the wifi_provision / buttons / display / rtc / ntp
// adapters.
#ifdef ARDUINO

// Compile-time guard: this module targets the Arduino-ESP32 framework
// specifically (uses <driver/i2s.h>, <SD.h>). If a future toolchain
// change defines ARDUINO but on a different arch, fail loud instead
// of silently compiling to nothing useful.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "audio adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include "audio.h"

namespace wc::audio {

// Real implementation lands in Task 6.
void begin(const BirthConfig& /*birth*/) {}
void loop()                             {}
void play_lullaby()                     {}
void stop()                             {}
bool is_playing()                       { return false; }

} // namespace wc::audio

#endif // ARDUINO
```

- [ ] **Step 9: Wire audio into `[env:native]` in platformio.ini**

Edit `firmware/platformio.ini`. Find the `[env:native]` block. Update
`build_flags` to add `-I lib/audio/include`, and update
`build_src_filter` to include the three pure-logic .cpp files.

Replace the existing `[env:native]` block (lines 43-70) with:

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
    -I lib/rtc/include
    -I lib/ntp/include
    -I lib/audio/include
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
    +<../lib/rtc/src/epoch.cpp>
    +<../lib/rtc/src/advance.cpp>
    +<../lib/ntp/src/validation.cpp>
    +<../lib/ntp/src/schedule.cpp>
    +<../lib/audio/src/wav.cpp>
    +<../lib/audio/src/gain.cpp>
    +<../lib/audio/src/fire_guard.cpp>
lib_compat_mode = off
```

- [ ] **Step 10: Verify native suite still passes (150 tests)**

Run: `cd firmware && pio test -e native`
Expected: **150 tests pass** (no regression — we added headers + empty .cpp files but no tests yet).

- [ ] **Step 11: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. The empty public API links in cleanly.

- [ ] **Step 12: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 13: Commit**

```bash
git add firmware/lib/audio/ firmware/platformio.ini
git commit -m "feat(firmware): audio module scaffold (empty stubs)"
```

---

## Task 2: Pure-logic WAV header validation — `parse_wav_header`

**Files:**
- Create: `firmware/test/test_audio_wav/test_wav.cpp`
- Modify: `firmware/lib/audio/src/wav.cpp`

- [ ] **Step 1: Write the failing tests**

Write `firmware/test/test_audio_wav/test_wav.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "audio/wav.h"

using namespace wc::audio;

// --- Canonical WAV header fixture -------------------------------------
//
// RIFF-ChunkID      [0..3]   "RIFF"
// ChunkSize         [4..7]   little-endian uint32 (filesize - 8)
// Format            [8..11]  "WAVE"
// Subchunk1ID       [12..15] "fmt "
// Subchunk1Size     [16..19] little-endian uint32 (16 for PCM)
// AudioFormat       [20..21] little-endian uint16 (1 for PCM)
// NumChannels       [22..23] little-endian uint16
// SampleRate        [24..27] little-endian uint32
// ByteRate          [28..31] little-endian uint32 (sample_rate * channels * bits/8)
// BlockAlign        [32..33] little-endian uint16 (channels * bits/8)
// BitsPerSample     [34..35] little-endian uint16
// Subchunk2ID       [36..39] "data"
// Subchunk2Size     [40..43] little-endian uint32 (payload bytes)
static void build_canonical_header(uint8_t* h, uint32_t data_size) {
    std::memset(h, 0, 44);
    std::memcpy(h + 0,  "RIFF", 4);
    uint32_t chunk_size = data_size + 36;
    h[4]  = chunk_size & 0xFF;
    h[5]  = (chunk_size >> 8)  & 0xFF;
    h[6]  = (chunk_size >> 16) & 0xFF;
    h[7]  = (chunk_size >> 24) & 0xFF;
    std::memcpy(h + 8,  "WAVE", 4);
    std::memcpy(h + 12, "fmt ", 4);
    h[16] = 16;  // Subchunk1Size
    h[20] = 1;   // AudioFormat = PCM
    h[22] = 1;   // NumChannels = 1
    // SampleRate 44100
    h[24] = 44100 & 0xFF;
    h[25] = (44100 >> 8)  & 0xFF;
    h[26] = (44100 >> 16) & 0xFF;
    h[27] = (44100 >> 24) & 0xFF;
    // ByteRate 88200
    h[28] = 88200 & 0xFF;
    h[29] = (88200 >> 8)  & 0xFF;
    h[30] = (88200 >> 16) & 0xFF;
    h[31] = (88200 >> 24) & 0xFF;
    h[32] = 2;   // BlockAlign
    h[34] = 16;  // BitsPerSample
    std::memcpy(h + 36, "data", 4);
    h[40] = data_size & 0xFF;
    h[41] = (data_size >> 8)  & 0xFF;
    h[42] = (data_size >> 16) & 0xFF;
    h[43] = (data_size >> 24) & 0xFF;
}

void setUp(void)    {}
void tearDown(void) {}

void test_wav_truncated_buffer(void) {
    uint8_t buf[44] = {0};
    ParseResult r = parse_wav_header(buf, 43);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::Truncated,
                            (uint8_t)r.error);
}

void test_wav_valid_canonical(void) {
    uint8_t h[44];
    build_canonical_header(h, /*data_size=*/88200);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::Ok, (uint8_t)r.error);
    TEST_ASSERT_EQUAL_UINT32(44u,    r.data_offset);
    TEST_ASSERT_EQUAL_UINT32(88200u, r.data_size_bytes);
}

void test_wav_bad_riff_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 0, "RIFX", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadRiffMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_wave_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 8, "WAVX", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadWaveMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_fmt_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 12, "fmtx", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadFmtMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_codec_tag(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[20] = 3;  // IEEE float, not PCM
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadCodecTag,
                            (uint8_t)r.error);
}

void test_wav_wrong_channels(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[22] = 2;  // stereo
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongChannelCount,
                            (uint8_t)r.error);
}

void test_wav_wrong_sample_rate(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    // 48000 little-endian
    h[24] = 0x80; h[25] = 0xBB; h[26] = 0x00; h[27] = 0x00;
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongSampleRate,
                            (uint8_t)r.error);
}

void test_wav_wrong_bits_per_sample(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[34] = 24;
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongBitsPerSample,
                            (uint8_t)r.error);
}

void test_wav_bad_data_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 36, "dbta", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadDataMagic,
                            (uint8_t)r.error);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_truncated_buffer);
    RUN_TEST(test_wav_valid_canonical);
    RUN_TEST(test_wav_bad_riff_magic);
    RUN_TEST(test_wav_bad_wave_magic);
    RUN_TEST(test_wav_bad_fmt_magic);
    RUN_TEST(test_wav_bad_codec_tag);
    RUN_TEST(test_wav_wrong_channels);
    RUN_TEST(test_wav_wrong_sample_rate);
    RUN_TEST(test_wav_wrong_bits_per_sample);
    RUN_TEST(test_wav_bad_data_magic);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_audio_wav`
Expected: link error — `parse_wav_header` is declared in the header
but the .cpp stub only contains a comment, so the symbol is undefined.

- [ ] **Step 3: Implement `parse_wav_header`**

Replace the contents of `firmware/lib/audio/src/wav.cpp` with:

```cpp
// firmware/lib/audio/src/wav.cpp
#include "audio/wav.h"

#include <cstring>

namespace wc::audio {

// Little-endian read helpers. The RIFF spec is strictly LE, and the
// daughters' clocks are run on ESP32 (LE) — but reading byte-by-byte
// keeps the parser portable and makes the native-test build work on
// any host byte order.
static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32_le(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])        |
           (static_cast<uint32_t>(p[1]) << 8)  |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

ParseResult parse_wav_header(const uint8_t* buf, size_t buf_len) {
    ParseResult r{};
    if (buf_len < WAV_CANONICAL_HEADER_LEN) {
        r.error = WavParseError::Truncated;
        return r;
    }
    if (std::memcmp(buf + 0,  "RIFF", 4) != 0) {
        r.error = WavParseError::BadRiffMagic;  return r;
    }
    if (std::memcmp(buf + 8,  "WAVE", 4) != 0) {
        r.error = WavParseError::BadWaveMagic;  return r;
    }
    if (std::memcmp(buf + 12, "fmt ", 4) != 0) {
        r.error = WavParseError::BadFmtMagic;   return r;
    }
    if (read_u16_le(buf + 20) != WAV_PCM_CODEC_TAG) {
        r.error = WavParseError::BadCodecTag;   return r;
    }
    if (read_u16_le(buf + 22) != WAV_CHANNEL_COUNT_MONO) {
        r.error = WavParseError::WrongChannelCount; return r;
    }
    if (read_u32_le(buf + 24) != WAV_SAMPLE_RATE_HZ) {
        r.error = WavParseError::WrongSampleRate;   return r;
    }
    if (read_u16_le(buf + 34) != WAV_BITS_PER_SAMPLE) {
        r.error = WavParseError::WrongBitsPerSample; return r;
    }
    if (std::memcmp(buf + 36, "data", 4) != 0) {
        r.error = WavParseError::BadDataMagic;  return r;
    }

    r.error           = WavParseError::Ok;
    r.data_offset     = static_cast<uint32_t>(WAV_CANONICAL_HEADER_LEN);
    r.data_size_bytes = read_u32_le(buf + 40);
    return r;
}

} // namespace wc::audio
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_audio_wav`
Expected: **10 tests pass.**

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **160 tests pass** (150 baseline + 10 new).

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/audio/src/wav.cpp firmware/test/test_audio_wav/
git commit -m "feat(firmware): pure-logic parse_wav_header (audio)"
```

---

## Task 3: Pure-logic Q8 gain scalar — `apply_gain_q8`

**Files:**
- Create: `firmware/test/test_audio_gain/test_gain.cpp`
- Modify: `firmware/lib/audio/src/gain.cpp`

- [ ] **Step 1: Write the failing tests**

Write `firmware/test/test_audio_gain/test_gain.cpp`:

```cpp
#include <unity.h>
#include <climits>
#include "audio/gain.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

void test_gain_unity_256_identity(void) {
    int16_t buf[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    int16_t expected[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    apply_gain_q8(buf, 5, 256);
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT16(expected[i], buf[i]);
    }
}

void test_gain_half_128_halves(void) {
    int16_t buf[1] = { 32000 };
    apply_gain_q8(buf, 1, 128);
    TEST_ASSERT_EQUAL_INT16(16000, buf[0]);
}

void test_gain_zero_mutes(void) {
    int16_t buf[2] = { 32000, -32000 };
    apply_gain_q8(buf, 2, 0);
    TEST_ASSERT_EQUAL_INT16(0, buf[0]);
    TEST_ASSERT_EQUAL_INT16(0, buf[1]);
}

void test_gain_no_clipping_at_unity(void) {
    // Prove the multiply-and-shift path does not wrap at int16 boundaries.
    int16_t buf[2] = { INT16_MAX, INT16_MIN };
    apply_gain_q8(buf, 2, 256);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, buf[0]);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, buf[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_gain_unity_256_identity);
    RUN_TEST(test_gain_half_128_halves);
    RUN_TEST(test_gain_zero_mutes);
    RUN_TEST(test_gain_no_clipping_at_unity);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_audio_gain`
Expected: link error — `apply_gain_q8` declared but not defined.

- [ ] **Step 3: Implement `apply_gain_q8`**

Replace the contents of `firmware/lib/audio/src/gain.cpp` with:

```cpp
// firmware/lib/audio/src/gain.cpp
#include "audio/gain.h"

namespace wc::audio {

void apply_gain_q8(int16_t* samples, size_t n, uint16_t gain_q8) {
    if (gain_q8 == 256) return;  // unity shortcut
    for (size_t i = 0; i < n; ++i) {
        samples[i] = static_cast<int16_t>(
            (static_cast<int32_t>(samples[i]) * gain_q8) >> 8
        );
    }
}

} // namespace wc::audio
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_audio_gain`
Expected: **4 tests pass.**

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **164 tests pass** (160 + 4 new).

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/audio/src/gain.cpp firmware/test/test_audio_gain/
git commit -m "feat(firmware): pure-logic apply_gain_q8 (audio)"
```

---

## Task 4: Pure-logic birthday auto-fire guard — `should_auto_fire`

**Files:**
- Create: `firmware/test/test_audio_fire_guard/test_fire_guard.cpp`
- Modify: `firmware/lib/audio/src/fire_guard.cpp`

- [ ] **Step 1: Write the failing tests**

Write `firmware/test/test_audio_fire_guard/test_fire_guard.cpp`:

```cpp
#include <unity.h>
#include "audio/fire_guard.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

// Fixture — Emory's config. 2030 = first birthday we'd auto-fire.
static constexpr BirthConfig EMORY = {
    /*month=*/10, /*day=*/6, /*hour=*/18, /*minute=*/10
};

static NowFields make_now(uint16_t y, uint8_t m, uint8_t d,
                          uint8_t h, uint8_t mm) {
    return NowFields{y, m, d, h, mm};
}

void test_fire_happy_path(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2029, /*time_known=*/true,
        /*already_playing=*/false));
}

void test_fire_wrong_month(void) {
    NowFields n = make_now(2030, 11, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_day(void) {
    NowFields n = make_now(2030, 10, 7, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_hour(void) {
    NowFields n = make_now(2030, 10, 6, 17, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_minute(void) {
    NowFields n = make_now(2030, 10, 6, 18, 11);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_already_fired_this_year(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2030, true, false));
}

void test_fire_unknown_time(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029,
        /*time_known=*/false, false));
}

void test_fire_already_playing(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true,
        /*already_playing=*/true));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fire_happy_path);
    RUN_TEST(test_fire_wrong_month);
    RUN_TEST(test_fire_wrong_day);
    RUN_TEST(test_fire_wrong_hour);
    RUN_TEST(test_fire_wrong_minute);
    RUN_TEST(test_fire_already_fired_this_year);
    RUN_TEST(test_fire_unknown_time);
    RUN_TEST(test_fire_already_playing);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_audio_fire_guard`
Expected: link error — `should_auto_fire` declared but not defined.

- [ ] **Step 3: Implement `should_auto_fire`**

Replace the contents of `firmware/lib/audio/src/fire_guard.cpp` with:

```cpp
// firmware/lib/audio/src/fire_guard.cpp
#include "audio/fire_guard.h"

namespace wc::audio {

bool should_auto_fire(const NowFields& now,
                      const BirthConfig& birth,
                      uint16_t last_fired_year,
                      bool time_known,
                      bool already_playing) {
    if (!time_known)               return false;
    if (already_playing)           return false;
    if (now.year  == last_fired_year) return false;
    if (now.month != birth.month)  return false;
    if (now.day   != birth.day)    return false;
    if (now.hour  != birth.hour)   return false;
    if (now.minute != birth.minute) return false;
    return true;
}

} // namespace wc::audio
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_audio_fire_guard`
Expected: **8 tests pass.**

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **172 tests pass** (164 + 8 new). This locks in the
target-state count from the spec.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/audio/src/fire_guard.cpp firmware/test/test_audio_fire_guard/
git commit -m "feat(firmware): pure-logic should_auto_fire (audio)"
```

---

## Task 5: Audio ESP32 adapter (`begin` + `loop` + `play_lullaby` + `stop` + `is_playing`)

Implements the full adapter per `docs/superpowers/specs/2026-04-18-audio-design.md`
§Adapter pseudocode. Wraps driver/i2s.h + SD + Preferences +
audio's pure-logic helpers + rtc::now() + wifi_provision::seconds_since_last_sync().
No native test — adapter is ESP32-only, verified via build + the
manual hardware checks added in Task 9.

**Files:**
- Modify: `firmware/lib/audio/src/audio.cpp`

- [ ] **Step 1: Replace the adapter stub with the full implementation**

Replace the contents of `firmware/lib/audio/src/audio.cpp` with:

```cpp
// firmware/lib/audio/src/audio.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag SD/driver-i2s into the native build.
// Same pattern as the wifi_provision / buttons / display / rtc / ntp
// adapters.
#ifdef ARDUINO

#if !defined(ARDUINO_ARCH_ESP32)
  #error "audio adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <cstdint>
#include "audio.h"
#include "audio/wav.h"
#include "audio/gain.h"
#include "audio/fire_guard.h"
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

enum class State : uint8_t { Idle, Playing };

static bool        started              = false;
static bool        sd_ok                = false;
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
// a non-zero byte count. Auto-fire gates the birth.wav transition on
// this bool — a silent NVS miss would risk missing-or-double-fire
// across a power cycle.
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
    Serial.printf("[audio] %s (played %u bytes)\n",
                  reason, static_cast<unsigned>(bytes_played_));
}

// --- Public API --------------------------------------------------

void begin(const BirthConfig& birth) {
    if (started) return;
    birth_ = birth;

    if (!i2s_install()) {
        Serial.println("[audio] begin: I2S install FAILED (subsequent i2s_write calls will fail)");
    }
    sd_ok = SD.begin(PIN_SD_CS);
    if (!sd_ok) {
        Serial.println("[audio] begin: SD FAILED, playback disabled until next boot");
    } else {
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
                      static_cast<size_t>(n) / 2,
                      CLOCK_AUDIO_GAIN_Q8);
        size_t written = 0;
        esp_err_t err = i2s_write(I2S_PORT, read_buf_,
                                  static_cast<size_t>(n),
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
    if (!sd_ok) return;
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
    if (should_auto_fire(nf, birth_, last_birth_year_,
                         time_known, already_playing)) {
        Serial.printf("[audio] play birth.wav (year=%u)\n", nf.year);
        // Stamp NVS FIRST, gate playback on the write succeeding.
        if (!nvs_write_last_birth_year(nf.year)) {
            Serial.println("[audio] error: NVS stamp FAILED; skipping birth.wav this tick");
            return;
        }
        last_birth_year_ = nf.year;
        transition_to_playing(BIRTH_PATH);
    }
}

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

bool is_playing() {
    return started && state_ == State::Playing;
}

} // namespace wc::audio

#endif // ARDUINO
```

- [ ] **Step 2: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. The adapter pulls in SD, Preferences,
and driver/i2s.h — all already in Arduino-ESP32 core. Watch the
build output for any unresolved-symbol errors; pure-logic symbols
(`parse_wav_header`, `apply_gain_q8`, `should_auto_fire`) must all
link via the same `lib/audio/src/*.cpp` files that the adapter
includes from its own library.

- [ ] **Step 3: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 4: Verify the native suite still passes (172 tests)**

Run: `cd firmware && pio test -e native`
Expected: **172 tests pass** (no change — `audio.cpp` is
`#ifdef ARDUINO`-guarded).

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/audio/src/audio.cpp
git commit -m "feat(firmware): audio ESP32 adapter (I²S + SD + NVS + state machine)"
```

---

## Task 6: Wire audio into `main.cpp`

Adds `wc::audio::begin()` to `setup()` (AFTER `buttons::begin()` —
audio is the last begin() per spec), `wc::audio::loop()` to
`loop()` (alongside the other module loops), and updates the
`AudioPressed` button-handler precedence to the three-branch form
specced in §Integration.

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Add the include**

Edit `firmware/src/main.cpp`. Add `#include "audio.h"` to the
existing include block at the top. The include block should now
read (alphabetical order, matching the current convention):

```cpp
#include <Arduino.h>
#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "ntp.h"
#include "rtc.h"
#include "wifi_provision.h"
```

- [ ] **Step 2: Add `wc::audio::begin()` at the END of `setup()`**

In `setup()`, after the `wc::buttons::begin([...])` callback block
closes (line 43 in the current file), add:

```cpp
    wc::audio::begin({CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                      CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE});
```

The `CLOCK_BIRTH_*` macros come from the auto-included per-kid
config header (`configs/config_emory.h` or `configs/config_nora.h`),
already in use by the display RenderInput code. Same values —
passing them into audio's BirthConfig struct.

- [ ] **Step 3: Update the `AudioPressed` button-handler case**

In the button callback registered with `wc::buttons::begin(...)`,
locate the existing `case BE::AudioPressed:` branch:

```cpp
            case BE::AudioPressed:
                if (wc::wifi_provision::state() == WS::AwaitingConfirmation) {
                    wc::wifi_provision::confirm_audio();
                } else {
                    Serial.println("[buttons] AudioPressed (audio module not yet wired)");
                }
                break;
```

Replace it with the three-branch precedence:

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

- [ ] **Step 4: Add `wc::audio::loop()` to `loop()`**

In `loop()`, the current call order is:

```cpp
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // sync scheduler; no-op when not Online
```

Add `wc::audio::loop();` immediately AFTER `wc::ntp::loop();`:

```cpp
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // sync scheduler; no-op when not Online
    wc::audio::loop();             // pump I²S when Playing; auto-fire check when Idle
```

Call order matters: `ntp::loop()` will gain an early-return on
`wc::audio::is_playing()` in Task 7, so audio's is_playing() state
must be up-to-date when ntp runs. Since `audio::loop()` updates
state_ inline, putting it after ntp means ntp sees last tick's
state. That's fine — the "race" is a lullaby-start that ntp misses
by one tick (≤2 ms), which is well within ntp's 24h ± 30min cadence
tolerance.

- [ ] **Step 5: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. Note the resulting flash + RAM usage in
the `RAM:` / `Flash:` line — should be higher than baseline (was
~67.6% flash post-ntp; audio adds ~10-30 KB depending on SD.h/VFS
overhead — if it pushes past 85%, consult the spec §Open issues on
DMA buffer / SD read chunk size before trimming).

- [ ] **Step 6: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 7: Verify the native suite still passes (172 tests)**

Run: `cd firmware && pio test -e native`
Expected: **172 tests pass.** (`main.cpp` is `src_dir`; native
build filter excludes it.)

- [ ] **Step 8: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): bring-up wire-up of audio in main.cpp"
```

---

## Task 7: `ntp::loop()` — defer syncs during audio playback

One-line early-return at the top of `ntp::loop()` so the ~1 s
`forceUpdate()` busy-wait cannot trigger an I²S DMA underrun
during lullaby / birth playback. See audio spec §Integration >
`ntp::loop()` and §Decisions deliberately deferred.

**Files:**
- Modify: `firmware/lib/ntp/src/ntp.cpp`

- [ ] **Step 1: Add the audio include**

Edit `firmware/lib/ntp/src/ntp.cpp`. Add `#include "audio.h"` to
the existing include block (around line 22). The include block
becomes:

```cpp
#include <Arduino.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>
#include <cstdint>
#include "ntp.h"
#include "ntp/schedule.h"
#include "ntp/validation.h"
#include "wifi_provision.h"
#include "rtc.h"
#include "audio.h"
```

No header dependency cycle: `audio.cpp` does not include `ntp.h`
(verified against the adapter include list).

- [ ] **Step 2: Add the is_playing early-return at the top of loop()**

In `firmware/lib/ntp/src/ntp.cpp` `void loop()`, locate the
current top:

```cpp
void loop() {
    if (!started) return;
    if (wc::wifi_provision::state() != wc::wifi_provision::State::Online) {
        return;
    }
```

Insert the audio-defer check BETWEEN the `started` check and the
`Online` check:

```cpp
void loop() {
    if (!started) return;
    if (wc::audio::is_playing()) return;   // defer sync; ~1s forceUpdate
                                           // would underrun I²S DMA
    if (wc::wifi_provision::state() != wc::wifi_provision::State::Online) {
        return;
    }
```

- [ ] **Step 3: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. Confirms the adapter-to-adapter include
graph resolves without cycles.

- [ ] **Step 4: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 5: Verify the native suite still passes (172 tests)**

Run: `cd firmware && pio test -e native`
Expected: **172 tests pass.** `ntp.cpp` is `#ifdef ARDUINO`-guarded,
so the new include doesn't affect native builds.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/ntp/src/ntp.cpp
git commit -m "feat(firmware): ntp defers syncs during audio playback"
```

---

## Task 8: Parent-spec + TODO.md edits (MP3 → WAV)

Per `feedback_no_conflicting_guides.md`: the audio spec flipped the
locked "MP3 128 kbps CBR" decision to "WAV 16-bit PCM 44.1 kHz
mono", and the parent design spec + TODO.md still reference MP3.
Fix both in one commit.

**Files:**
- Modify: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
- Modify: `TODO.md`

- [ ] **Step 1: Update the parent design spec §Hardware Platform > Audio**

Edit `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`.
Find line 33 (or the nearest surrounding line starting with
"- **Audio:** MAX98357A..."):

```markdown
- **Audio:** MAX98357A I2S DAC + amp, small speaker (~2-3W), microSD card for MP3 files.
```

Replace with:

```markdown
- **Audio:** MAX98357A I2S DAC + amp, small speaker (~2-3W), microSD card for 16-bit PCM WAV files (44.1 kHz mono). Format was flipped from MP3 during audio-module spec (`docs/superpowers/specs/2026-04-18-audio-design.md`) — eliminates the MP3 decoder dependency.
```

- [ ] **Step 2: Update the TODO.md audio-file-format bullet**

Edit `TODO.md`. Find the locked `[x]` bullet starting with
"**Audio file format — MP3, 128 kbps CBR, mono, 44.1 kHz.**"
(around line 140). Replace the entire bullet + body text with:

```markdown
- [x] **Audio file format — WAV, 16-bit PCM, 44.1 kHz, mono,
      little-endian.** Design flipped from MP3 during audio-module
      spec pass (`docs/superpowers/specs/2026-04-18-audio-design.md`).
      Eliminates the MP3 decoder dependency (libhelix / ESP8266Audio /
      ESP32-audioI2S — ESP32-audioI2S required PSRAM we don't have;
      the others were decoder libraries we don't need once the file
      format is uncompressed). Collapses the audio adapter to an
      SD-read → I²S-write pump. File size is ~88 kB/s raw PCM
      (~18.5 MB total per card for a 3 min lullaby + 30 s birth
      message); trivially inside any modern microSD's capacity.
```

- [ ] **Step 3: Add spec ref to the TODO.md audio bullet (still `[ ]`)**

In `TODO.md`, find the audio bullet (still unchecked — will land
`[x]` in Task 10):

```markdown
- [ ] **`audio`** — I²S + MAX98357A. MP3 decode from microSD.
      Play / stop behavior: press once to play lullaby, press
      during playback to stop. Volume is fixed in firmware and
      tuned during assembly. Needs a spec (ESP8266Audio vs.
      libhelix library choice, volume curve, play-state machine
      with buffer-underrun handling).
```

Append the spec link (don't check `[x]` yet):

```markdown
- [ ] **`audio`** — I²S + MAX98357A. MP3 decode from microSD.
      Play / stop behavior: press once to play lullaby, press
      during playback to stop. Volume is fixed in firmware and
      tuned during assembly. Needs a spec (ESP8266Audio vs.
      libhelix library choice, volume curve, play-state machine
      with buffer-underrun handling).
      Spec: `docs/superpowers/specs/2026-04-18-audio-design.md`.
```

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/specs/2026-04-14-daughters-clocks-design.md TODO.md
git commit -m "docs: flip audio file format MP3 → WAV + add audio spec ref"
```

---

## Task 9: Hardware manual-check document

Creates the manual checklist that runs during breadboard bring-up
once the MAX98357A + speaker + microSD path has been wired. Mirrors
the existing `wifi_provision_checks.md` / `buttons_checks.md` /
`display_checks.md` / `rtc_checks.md` / `ntp_checks.md` shape.

**Files:**
- Create: `firmware/test/hardware_checks/audio_checks.md`

- [ ] **Step 1: Write the hardware-checks document**

Write `firmware/test/hardware_checks/audio_checks.md`:

```markdown
# Audio Module — Manual Hardware Checks

These checks exercise the ESP32 paths that the native test suite
cannot reach: real I²S DMA + MAX98357A + speaker output, real SD
card open / read / close via VSPI, real `Preferences`/NVS round-trip
for `last_birth_year`, real `rtc::now()` for the auto-fire guard.
Run during breadboard bring-up after the MAX98357A + microSD paths
have been wired (`docs/hardware/breadboard-bring-up-guide.md`
§MAX98357A + Speaker and §MicroSD) and the ntp
(`ntp_checks.md`) and rtc (`rtc_checks.md`) checklists are green.

Spec: `docs/superpowers/specs/2026-04-18-audio-design.md`.

## Prerequisites

- ESP32 flashed with the current emory or nora env build.
- Captive-portal flow completed at least once (NVS holds valid
  WiFi creds + TZ; `seconds_since_last_sync()` is non-UINT32_MAX).
- MicroSD prepared with canonical WAV files at the card root:
  - `/lullaby.wav` — 44.1 kHz / 16-bit / mono / PCM, ~3 min.
  - `/birth.wav`   — same format, ~30 s.
  - Authoring recipe: Audacity > File > Export > Export as WAV;
    Encoding = "Signed 16-bit PCM", Sample Rate = 44100 Hz,
    Channels = Mono.
- MAX98357A hardware gain jumper set at the assembly default
  (9 dB unless bench measurement says otherwise).
- `CLOCK_AUDIO_GAIN_Q8` left at the default 256 for the first
  bring-up pass.
- Speaker connected to MAX98357A output terminals.
- Serial monitor open at 115200 baud.

## Checks

1. **Boot path observable.** Power on. Expect
   `[audio] begin: SD ok, I2S ok` in serial. If `SD FAILED`
   appears, re-seat the card and re-check VSPI wiring per
   `docs/hardware/pinout.md`.

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
   lower peak level (authoring fix), or lower `CLOCK_AUDIO_GAIN_Q8`
   to 181 (−3 dB) / 128 (−6 dB) via `configs/config_<kid>.h` and
   rebuild. Hardware jumper change (lower-gain resistor on
   MAX98357A) is the alternative if software trim isn't enough.
   Document the setting that worked in `docs/hardware/assembly-plan.md`.

5. **SD card removed mid-playback.** Start lullaby. Pull the
   microSD card from the slot. Expect either
   `[audio] error: i2s_write err=...` OR
   `[audio] aborted (played <N> bytes)` in serial, followed by
   silence. Re-seat the card; press the button again → plays
   successfully (no reboot needed).

6. **Missing file graceful.** Rename `/lullaby.wav` →
   `/xxx.wav` on the card, re-insert. Press audio button. Expect
   `[audio] error: file /lullaby.wav not found`; no sound. Stay
   Idle.

7. **Invalid WAV graceful.** Place a stereo 48 kHz WAV (or a
   renamed .mp3) at `/lullaby.wav`. Press audio button. Expect
   `[audio] error: wav header invalid (code=<N>)`; no sound.

8. **Birth auto-fire (simulated).** Pre-set the DS3231 via a
   bring-up sketch to 6:09:50 PM on Oct 6 of the current year
   (adjust month/day/time for the active env's `CLOCK_BIRTH_*`).
   Reset `audio/last_birth_year` to 0 via a wipe sketch OR by
   flashing a one-shot debug build that calls `Preferences::clear()`
   on the `"audio"` namespace at boot. Wait through 6:10:00 PM.
   Expect `[audio] play birth.wav (year=<current year>)` within
   one `loop()` tick of the minute roll. Confirm audible playback;
   wait for `[audio] finished`.

9. **Birth auto-fire fires once per year.** After check 8 passes,
   stay powered and wait until the next minute (6:11 PM). Expect
   no additional `[audio] play` log. Power-cycle. Expect no
   `[audio] play birth.wav` on next boot (NVS `last_birth_year`
   now matches the current year).

10. **Unknown-time gate.** Wipe NVS (hold Hour + Audio 10 s →
    `reset_to_captive`). Captive portal comes up; do NOT submit
    credentials. Pre-set the DS3231 to 6:10 PM on Oct 6 via the
    bring-up sketch. Verify no auto-fire — `time_known` is false
    because `seconds_since_last_sync()` reads UINT32_MAX. No
    `[audio] play birth.wav` log should appear.

11. **ntp deferral during playback.** Start lullaby playback. In
    serial, confirm no `[ntp] sync ok` / `[ntp] forceUpdate()`
    log lines fire during the ~3 min playback window, even if a
    deadline would otherwise fire. After the lullaby finishes,
    confirm ntp resumes its normal cadence.

12. **Underrun sanity.** During playback, stress the main loop by
    adding a temporary `Serial.println(millis());` every loop tick.
    Playback should remain clean. Audible glitches here would
    point to DMA ring sizing being too tight — rare at
    4 × 1024 buffers (~93 ms of audio).

## Pass criteria

Checks 1–7 and 11 must pass before considering the module
hardware-verified. 8–10 require a controlled pre-set DS3231 and
may be deferred until a real birthday during burn-in. 12 is
diagnostic-only.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/audio_checks.md
git commit -m "docs(firmware): manual hardware checks for audio module"
```

---

## Task 10: Mark TODO.md `audio` bullet complete

Updates the project's live TODO so the audio module is checked
off with a one-line shipped-summary mirroring how ntp / rtc /
display / buttons got marked complete.

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Update the audio bullet from `[ ]` to `[x]` with shipped notes**

Edit `TODO.md`. Find the existing audio bullet (still unchecked;
updated in Task 8 to include the spec link). Replace the entire
bullet with:

```markdown
- [x] **`audio`** — I²S + MAX98357A + microSD WAV playback.
      Shipped: `firmware/lib/audio/` with pure-logic WAV header
      validation (10 tests), Q8 gain scalar (4 tests), birthday
      auto-fire guard (8 tests), and an ESP32 adapter wrapping
      `driver/i2s.h` + `SD.h` + `Preferences` with a 2-state
      play-state machine. Button-press plays lullaby / stops
      playback; birth.wav auto-fires exactly once per year at
      the birth minute (NVS-gated). 22 native tests; full suite
      172/172. Both envs build clean. Spec:
      `docs/superpowers/specs/2026-04-18-audio-design.md`.
      Plan: `docs/superpowers/plans/2026-04-19-audio-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/audio_checks.md`.
```

- [ ] **Step 2: Commit**

```bash
git add TODO.md
git commit -m "docs(todo): mark audio module complete"
```

---

## Self-review notes

Spec coverage cross-check:

| Spec section | Implemented in |
|---|---|
| `parse_wav_header` + constants + errors | Task 2 |
| `apply_gain_q8` + CLOCK_AUDIO_GAIN_Q8 default | Task 3 |
| `should_auto_fire` (birthday auto-fire gates) | Task 4 |
| ESP32 adapter (I²S install, SD mount, NVS, state machine, pump, rtc::now seam) | Task 5 |
| main.cpp wire-up (begin + loop + button precedence) | Task 6 |
| ntp deferral during playback | Task 7 |
| Parent-spec + TODO.md MP3 → WAV edits (`no conflicting guides` rule) | Task 8 |
| Hardware checklist | Task 9 |
| TODO.md audio bullet marked complete | Task 10 |

After all tasks: **10 commits, 22 pure-logic tests, 172 native
suite, 2 ESP32 envs green.**

Optional per-kid configs (`configs/config_emory.h` + `config_nora.h`
additions of `CLOCK_AUDIO_GAIN_Q8`) are NOT part of any task — the
default in `audio/gain.h` (256 = unity) compiles cleanly. Add the
per-kid override only if bench tuning during assembly requires it
(documented in hardware-check Task 9 step 4).
