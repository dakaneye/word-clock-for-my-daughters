# NTP Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `ntp` firmware module — a thin scheduler that, on a 24-hour ±30-min-jitter cadence (with capped exponential backoff on failure), queries `time.google.com`, validates the result, writes UTC seconds to the DS3231 via `wc::rtc::set_from_epoch()`, and stamps the last-sync time via `wc::wifi_provision::touch_last_sync()`.

**Architecture:** Hexagonal split. Pure-logic `validation` (range floor) and `schedule` (backoff curve, jittered deadline) are native-testable with Unity. ESP32 adapter (`ntp.cpp`) wraps `NTPClient` + `WiFi.hostByName` under `#ifdef ARDUINO`. Adapter polls `wifi_provision::state() == Online`; warm-boot reads NVS-stored last-sync to seed the scheduler so reboots inside the 24h window don't re-sync immediately.

**Tech Stack:** Arduino-ESP32 core, `arduino-libraries/NTPClient@^3.2.1` (already in `lib_deps`), PlatformIO, Unity C++ test framework.

---

## Spec reference

Implements `docs/superpowers/specs/2026-04-17-ntp-design.md`. Read the spec
in full before starting — it pins every decision (24h cadence, ±30 min
jitter, retry curve, era handling, validation floor, NTPClient pitfalls,
warm-boot resume). The plan below implements that spec exactly; if you
hit a decision that feels underspecified, re-read the spec before
inventing.

## File structure

```
firmware/
  lib/ntp/                            # NEW
    include/
      ntp.h                           # ESP32-only public API (begin, loop)
      ntp/
        validation.h                  # is_plausible_epoch + MIN_PLAUSIBLE_EPOCH
        schedule.h                    # next_backoff_ms,
                                      #   next_deadline_after_success
    src/
      ntp.cpp                         # adapter (#ifdef ARDUINO)
      validation.cpp                  # pure-logic range validation
      schedule.cpp                    # pure-logic cadence + backoff math
  lib/wifi_provision/                 # MODIFIED
    include/wifi_provision.h          #   add public touch_last_sync(uint64_t)
    src/wifi_provision.cpp            #   add wrapper + extend nvs_store fwd decl
    src/nvs_store.cpp                 #   add 1-line "internal" comment
  test/                               # NEW test dirs
    test_ntp_validation/test_validation.cpp
    test_ntp_schedule/test_schedule.cpp
    hardware_checks/ntp_checks.md     # NEW
  platformio.ini                      # MODIFIED — add ntp include + pure .cpps
  src/main.cpp                        # MODIFIED — add ntp::begin() + loop()
TODO.md                               # MODIFIED — mark ntp bullet [x]
```

Pure-logic `.cpp` files (`validation`, `schedule`) go into `[env:native]`'s
`build_src_filter`. `ntp.cpp` is guarded with `#ifdef ARDUINO` — same
reason as `wifi_provision`, `buttons`, `display`, `rtc` adapters
(PlatformIO LDF `deep+` otherwise pulls it into the native build).

## Build verification recipes (used throughout)

- Native test suite: `cd firmware && pio test -e native`
  - Baseline (post-rtc): **140 tests pass**.
  - After Tasks 2-4 land: **150 tests pass** (+3 validation +7 schedule).
- ESP32 build (Emory): `cd firmware && pio run -e emory`
- ESP32 build (Nora): `cd firmware && pio run -e nora`

Run the relevant build before every commit.

---

## Task 1: Module scaffold (empty stubs + platformio.ini wiring)

Sets up the `lib/ntp/` directory with empty stubs that compile cleanly
on all envs. No tests yet, no behavior. Locks the module shape into
the build system before any TDD starts.

**Files:**
- Create: `firmware/lib/ntp/include/ntp.h`
- Create: `firmware/lib/ntp/include/ntp/validation.h`
- Create: `firmware/lib/ntp/include/ntp/schedule.h`
- Create: `firmware/lib/ntp/src/ntp.cpp`
- Create: `firmware/lib/ntp/src/validation.cpp`
- Create: `firmware/lib/ntp/src/schedule.cpp`
- Modify: `firmware/platformio.ini`

- [ ] **Step 1: Create the public ESP32 header (stub)**

Write `firmware/lib/ntp/include/ntp.h`:

```cpp
// firmware/lib/ntp/include/ntp.h
//
// ESP32-only public API for the ntp module. Depends on Arduino.h,
// NTPClient, WiFi. Include ONLY from translation units that compile
// under the Arduino toolchain (the emory / nora PlatformIO envs).
// Native tests include the pure-logic headers under ntp/ — never
// this one.
#pragma once

#include <Arduino.h>

namespace wc::ntp {

// Open the NTPClient's UDP socket and seed the scheduler from
// NVS-stored last-sync (so a warm reboot doesn't immediately
// re-sync if we're inside the 24h window). No network I/O.
// Idempotent.
//
// Order: must be called AFTER wc::wifi_provision::begin() (so the
// WiFi stack is initialized AND the NVS-backed last-sync read works).
// Position relative to wc::rtc::begin() doesn't matter functionally.
// In main.cpp it's placed after rtc::begin() by convention.
void begin();

// Drives the scheduler. Call from main.cpp's loop(). Polls
// wifi_provision::state(); when Online, checks the deadline; on
// hit, runs forceUpdate, validates, and on success calls
// rtc::set_from_epoch + wifi_provision::touch_last_sync.
//
// At most ~1 s blocking via NTPClient::forceUpdate() on sync
// attempts (~once per 24h on the happy path).
void loop();

} // namespace wc::ntp
```

- [ ] **Step 2: Create the pure-logic validation header (stub)**

Write `firmware/lib/ntp/include/ntp/validation.h`:

```cpp
// firmware/lib/ntp/include/ntp/validation.h
#pragma once

#include <cstdint>

namespace wc::ntp {

// 2026-01-01 00:00:00 UTC. Lowest plausible sync result; anything
// earlier is a packet error or the Y2106 wrap.
constexpr uint32_t MIN_PLAUSIBLE_EPOCH = 1767225600u;

// Pure. Returns true iff unix_epoch >= MIN_PLAUSIBLE_EPOCH. No
// upper bound — uint32 unix epoch maxes at 2106-02-07.
bool is_plausible_epoch(uint32_t unix_epoch);

} // namespace wc::ntp
```

- [ ] **Step 3: Create the pure-logic schedule header (stub)**

Write `firmware/lib/ntp/include/ntp/schedule.h`:

```cpp
// firmware/lib/ntp/include/ntp/schedule.h
#pragma once

#include <cstdint>

namespace wc::ntp {

// Pure. Capped exponential backoff for the Nth consecutive failure.
// Caller is expected to pass N >= 1 (the adapter only calls this
// after consecutive_failures += 1).
//
//   N=1 -> 30'000  ms (30 s)
//   N=2 -> 60'000  ms (1 m)
//   N=3 -> 120'000 ms (2 m)
//   N=4 -> 240'000 ms (4 m)
//   N=5 -> 480'000 ms (8 m)
//   N>=6 -> 1'800'000 ms (30 m)  // cap
uint32_t next_backoff_ms(uint32_t consecutive_failures);

// Pure. Returns the millis() deadline at which the next sync should
// fire after a successful sync. = success_ms + 24h +
// uniform(-30m, +30m), where the offset is jitter_sample mod
// 3'600'000 - 1'800'000.
uint64_t next_deadline_after_success(uint64_t success_ms,
                                     uint32_t jitter_sample);

} // namespace wc::ntp
```

- [ ] **Step 4: Create the validation .cpp stub**

Write `firmware/lib/ntp/src/validation.cpp`:

```cpp
// firmware/lib/ntp/src/validation.cpp
#include "ntp/validation.h"

namespace wc::ntp {

// Implementation lands in Task 2 (TDD).

} // namespace wc::ntp
```

- [ ] **Step 5: Create the schedule .cpp stub**

Write `firmware/lib/ntp/src/schedule.cpp`:

```cpp
// firmware/lib/ntp/src/schedule.cpp
#include "ntp/schedule.h"

namespace wc::ntp {

// Implementations land in Tasks 3 + 4 (TDD).

} // namespace wc::ntp
```

- [ ] **Step 6: Create the adapter .cpp stub (under #ifdef ARDUINO)**

Write `firmware/lib/ntp/src/ntp.cpp`:

```cpp
// firmware/lib/ntp/src/ntp.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag NTPClient/WiFi into the native build.
// Same pattern as the wifi_provision/buttons/display/rtc adapters.
#ifdef ARDUINO

#include "ntp.h"

namespace wc::ntp {

// Real implementation lands in Task 6.
void begin() {}
void loop()  {}

} // namespace wc::ntp

#endif // ARDUINO
```

- [ ] **Step 7: Wire ntp into `[env:native]` in platformio.ini**

Edit `firmware/platformio.ini`. Find the `[env:native]` block. Update
`build_flags` to add `-I lib/ntp/include`, and update
`build_src_filter` to include the two pure-logic .cpp files.

Replace the existing `[env:native]` block (lines 43-67):

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
lib_compat_mode = off
```

with:

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
lib_compat_mode = off
```

- [ ] **Step 8: Verify native suite still passes (140 tests)**

Run: `cd firmware && pio test -e native`
Expected: **140 tests pass** (no regression — we added headers + empty .cpp files but no tests yet).

- [ ] **Step 9: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. The empty `begin()` / `loop()` link in cleanly.

- [ ] **Step 10: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 11: Commit**

```bash
git add firmware/lib/ntp/ firmware/platformio.ini
git commit -m "feat(firmware): ntp module scaffold (empty stubs)"
```

---

## Task 2: Pure-logic validation — `is_plausible_epoch`

**Files:**
- Create: `firmware/test/test_ntp_validation/test_validation.cpp`
- Modify: `firmware/lib/ntp/src/validation.cpp`

- [ ] **Step 1: Write the failing tests**

Write `firmware/test/test_ntp_validation/test_validation.cpp`:

```cpp
#include <unity.h>
#include "ntp/validation.h"

using namespace wc::ntp;

void setUp(void)    {}
void tearDown(void) {}

// 0 is the unix epoch (1970-01-01); below the 2026-01-01 floor.
void test_plausible_zero_rejected(void) {
    TEST_ASSERT_FALSE(is_plausible_epoch(0));
}

// One second below the floor must reject — boundary condition.
void test_plausible_just_below_floor_rejected(void) {
    TEST_ASSERT_FALSE(is_plausible_epoch(MIN_PLAUSIBLE_EPOCH - 1));
}

// Exact floor accepted — boundary condition.
void test_plausible_floor_accepted(void) {
    TEST_ASSERT_TRUE(is_plausible_epoch(MIN_PLAUSIBLE_EPOCH));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plausible_zero_rejected);
    RUN_TEST(test_plausible_just_below_floor_rejected);
    RUN_TEST(test_plausible_floor_accepted);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_ntp_validation`
Expected: link error — `is_plausible_epoch` is declared in the header
but the .cpp stub only contains a comment, so the symbol is undefined.
(If by any chance it links and runs, all 3 tests will fail.)

- [ ] **Step 3: Implement `is_plausible_epoch`**

Replace the contents of `firmware/lib/ntp/src/validation.cpp` with:

```cpp
// firmware/lib/ntp/src/validation.cpp
#include "ntp/validation.h"

namespace wc::ntp {

bool is_plausible_epoch(uint32_t unix_epoch) {
    return unix_epoch >= MIN_PLAUSIBLE_EPOCH;
}

} // namespace wc::ntp
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_ntp_validation`
Expected: **3 tests pass.**

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **143 tests pass** (140 baseline + 3 new).

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/ntp/src/validation.cpp firmware/test/test_ntp_validation/
git commit -m "feat(firmware): pure-logic is_plausible_epoch (ntp)"
```

---

## Task 3: Pure-logic schedule — `next_backoff_ms`

**Files:**
- Create: `firmware/test/test_ntp_schedule/test_schedule.cpp`
- Modify: `firmware/lib/ntp/src/schedule.cpp`

- [ ] **Step 1: Write the failing tests**

Write `firmware/test/test_ntp_schedule/test_schedule.cpp`. (We're
seeding the file here; Task 4 will append more tests.)

```cpp
#include <unity.h>
#include "ntp/schedule.h"

using namespace wc::ntp;

void setUp(void)    {}
void tearDown(void) {}

// -- next_backoff_ms ----------------------------------------------

// First retry: 30 s = 30'000 ms.
void test_backoff_n1_thirty_seconds(void) {
    TEST_ASSERT_EQUAL_UINT32(30'000u, next_backoff_ms(1));
}

// Last pre-cap step: 8 m = 480'000 ms.
void test_backoff_n5_eight_minutes(void) {
    TEST_ASSERT_EQUAL_UINT32(480'000u, next_backoff_ms(5));
}

// First step at the cap: 30 m = 1'800'000 ms.
void test_backoff_n6_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(6));
}

// Saturation: arbitrary large N still returns the cap.
void test_backoff_n100_still_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(100));
}

int main(int, char**) {
    UNITY_BEGIN();
    // next_backoff_ms
    RUN_TEST(test_backoff_n1_thirty_seconds);
    RUN_TEST(test_backoff_n5_eight_minutes);
    RUN_TEST(test_backoff_n6_at_cap);
    RUN_TEST(test_backoff_n100_still_at_cap);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd firmware && pio test -e native -f test_ntp_schedule`
Expected: link error — `next_backoff_ms` declared but not defined.

- [ ] **Step 3: Implement `next_backoff_ms`**

Replace the contents of `firmware/lib/ntp/src/schedule.cpp` with:

```cpp
// firmware/lib/ntp/src/schedule.cpp
#include "ntp/schedule.h"

namespace wc::ntp {

uint32_t next_backoff_ms(uint32_t consecutive_failures) {
    // Lookup table avoids the shift-by-(-1) UB hazard if a caller
    // ever passes 0 in violation of the contract. Index = N - 1.
    static constexpr uint32_t CURVE[] = {
        30'000u,    // N=1: 30 s
        60'000u,    // N=2: 1 m
        120'000u,   // N=3: 2 m
        240'000u,   // N=4: 4 m
        480'000u,   // N=5: 8 m
        1'800'000u, // N>=6: cap at 30 m
    };
    constexpr uint32_t MAX_IDX = (sizeof(CURVE) / sizeof(CURVE[0])) - 1;

    // Clamp underflow defensively: N=0 is a contract violation, but
    // returning the floor (30 s) is safer than UB.
    uint32_t idx = (consecutive_failures == 0) ? 0u
                                                : (consecutive_failures - 1u);
    if (idx > MAX_IDX) idx = MAX_IDX;
    return CURVE[idx];
}

} // namespace wc::ntp
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd firmware && pio test -e native -f test_ntp_schedule`
Expected: **4 tests pass.**

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **147 tests pass** (143 + 4 new).

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/ntp/src/schedule.cpp firmware/test/test_ntp_schedule/
git commit -m "feat(firmware): pure-logic next_backoff_ms (ntp)"
```

---

## Task 4: Pure-logic schedule — `next_deadline_after_success`

**Files:**
- Modify: `firmware/test/test_ntp_schedule/test_schedule.cpp`
- Modify: `firmware/lib/ntp/src/schedule.cpp`

- [ ] **Step 1: Append the 3 jitter tests + register them in main()**

Edit `firmware/test/test_ntp_schedule/test_schedule.cpp`. Add the
three new test functions ABOVE the existing `int main()` (after the
last `next_backoff_ms` test). Then add three new `RUN_TEST` lines in
`main()`. The complete file should look like:

```cpp
#include <unity.h>
#include "ntp/schedule.h"

using namespace wc::ntp;

void setUp(void)    {}
void tearDown(void) {}

// -- next_backoff_ms ----------------------------------------------

void test_backoff_n1_thirty_seconds(void) {
    TEST_ASSERT_EQUAL_UINT32(30'000u, next_backoff_ms(1));
}

void test_backoff_n5_eight_minutes(void) {
    TEST_ASSERT_EQUAL_UINT32(480'000u, next_backoff_ms(5));
}

void test_backoff_n6_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(6));
}

void test_backoff_n100_still_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(100));
}

// -- next_deadline_after_success ----------------------------------

// jitter_sample = 0 -> jitter offset = -30 min.
void test_deadline_jitter_zero_minus_30min(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint64_t THIRTY_MIN_MS = 1'800'000ULL;
    TEST_ASSERT_EQUAL_UINT64(T0 + TWENTY_FOUR_HOURS_MS - THIRTY_MIN_MS,
        next_deadline_after_success(T0, 0u));
}

// jitter_sample = 1'800'000 -> jitter offset = 0 (24h on the dot).
void test_deadline_jitter_mid_no_offset(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    TEST_ASSERT_EQUAL_UINT64(T0 + TWENTY_FOUR_HOURS_MS,
        next_deadline_after_success(T0, 1'800'000u));
}

// jitter_sample = 3'599'999 -> jitter offset = +30 min - 1 ms.
void test_deadline_jitter_max_plus_30min_minus_1ms(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint64_t THIRTY_MIN_MS = 1'800'000ULL;
    constexpr uint64_t ONE_MS = 1ULL;
    TEST_ASSERT_EQUAL_UINT64(
        T0 + TWENTY_FOUR_HOURS_MS + THIRTY_MIN_MS - ONE_MS,
        next_deadline_after_success(T0, 3'599'999u));
}

int main(int, char**) {
    UNITY_BEGIN();
    // next_backoff_ms
    RUN_TEST(test_backoff_n1_thirty_seconds);
    RUN_TEST(test_backoff_n5_eight_minutes);
    RUN_TEST(test_backoff_n6_at_cap);
    RUN_TEST(test_backoff_n100_still_at_cap);
    // next_deadline_after_success
    RUN_TEST(test_deadline_jitter_zero_minus_30min);
    RUN_TEST(test_deadline_jitter_mid_no_offset);
    RUN_TEST(test_deadline_jitter_max_plus_30min_minus_1ms);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify the 3 new ones fail**

Run: `cd firmware && pio test -e native -f test_ntp_schedule`
Expected: 4 backoff tests pass, 3 deadline tests fail with link error
(`next_deadline_after_success` declared but not defined).

- [ ] **Step 3: Implement `next_deadline_after_success`**

Append to `firmware/lib/ntp/src/schedule.cpp`. The full file should
now read:

```cpp
// firmware/lib/ntp/src/schedule.cpp
#include "ntp/schedule.h"

namespace wc::ntp {

uint32_t next_backoff_ms(uint32_t consecutive_failures) {
    static constexpr uint32_t CURVE[] = {
        30'000u, 60'000u, 120'000u, 240'000u, 480'000u, 1'800'000u,
    };
    constexpr uint32_t MAX_IDX = (sizeof(CURVE) / sizeof(CURVE[0])) - 1;

    uint32_t idx = (consecutive_failures == 0) ? 0u
                                                : (consecutive_failures - 1u);
    if (idx > MAX_IDX) idx = MAX_IDX;
    return CURVE[idx];
}

uint64_t next_deadline_after_success(uint64_t success_ms,
                                     uint32_t jitter_sample) {
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint32_t JITTER_SPAN_MS = 3'600'000u;  // 1h total span
    constexpr uint32_t JITTER_HALF_MS = 1'800'000u;  // ±30 min
    uint32_t jitter = jitter_sample % JITTER_SPAN_MS;
    return success_ms + TWENTY_FOUR_HOURS_MS + jitter - JITTER_HALF_MS;
}

} // namespace wc::ntp
```

- [ ] **Step 4: Run tests to verify all pass**

Run: `cd firmware && pio test -e native -f test_ntp_schedule`
Expected: **7 tests pass** (4 backoff + 3 deadline).

- [ ] **Step 5: Run the full native suite (regression check)**

Run: `cd firmware && pio test -e native`
Expected: **150 tests pass** (147 + 3 new). This locks in the
target-state count from the spec.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/ntp/src/schedule.cpp firmware/test/test_ntp_schedule/test_schedule.cpp
git commit -m "feat(firmware): pure-logic next_deadline_after_success (ntp)"
```

---

## Task 5: Public `wifi_provision::touch_last_sync` wrapper

Adds the public-API surface that ntp's adapter calls. Also extends the
forward-decl block in `wifi_provision.cpp` so the wrapper can call into
`nvs_store::touch_last_sync`. No native test — this is a delegation
wrapper around ESP32-only NVS code; verified via the build.

**Files:**
- Modify: `firmware/lib/wifi_provision/include/wifi_provision.h`
- Modify: `firmware/lib/wifi_provision/src/wifi_provision.cpp`
- Modify: `firmware/lib/wifi_provision/src/nvs_store.cpp`

- [ ] **Step 1: Add the public declaration to wifi_provision.h**

Edit `firmware/lib/wifi_provision/include/wifi_provision.h`. After
the existing `confirm_audio()` declaration (line 29), add the
`touch_last_sync` declaration so the file ends like this:

```cpp
// Fire Event::AudioButtonConfirmed into the state machine.
// Caller: the buttons module when Audio is pressed during captive-portal
// AwaitingConfirmation state.
void confirm_audio();

// Stamp NVS with a successful NTP sync at unix_seconds (UTC).
// Called by the ntp module on a successful sync. Display reads
// the corresponding seconds_since_last_sync() to drive the amber
// stale-sync tint.
void touch_last_sync(uint64_t unix_seconds);

} // namespace wc::wifi_provision
```

- [ ] **Step 2: Extend the nvs_store forward-decl block**

Edit `firmware/lib/wifi_provision/src/wifi_provision.cpp`. Find the
forward-decl block at the top (around lines 23-34) that opens with
`namespace wc::wifi_provision::nvs_store {` and currently does NOT
declare `touch_last_sync`. Replace the block with:

```cpp
// Forward decls to the adapter namespaces (defined in sibling .cpp files).
// Only the symbols actually called from this TU are declared here.
namespace wc::wifi_provision::nvs_store {
    bool has_credentials();
    struct StoredCredentials { String ssid; String pw; String tz; };
    StoredCredentials read();
    bool write(const FormBody& body);
    bool touch_last_sync(uint64_t unix_seconds);
    uint64_t last_sync();
    void clear();
}
```

(The 3-line comment that previously said "nvs_store exposes
touch_last_sync() too, but the NTP module that calls it doesn't
exist yet — its forward decl will land with that module." gets
absorbed; the NTP module now exists, the forward decl now lives here.)

- [ ] **Step 3: Implement the public wrapper**

Edit `firmware/lib/wifi_provision/src/wifi_provision.cpp`. After the
existing `confirm_audio()` definition near the bottom of the
`namespace wc::wifi_provision { ... }` block (look for it via
`grep -n confirm_audio`), add the `touch_last_sync` wrapper. The new
function:

```cpp
void touch_last_sync(uint64_t unix_seconds) {
    nvs_store::touch_last_sync(unix_seconds);
}
```

The return value of `nvs_store::touch_last_sync` is intentionally
discarded — the spec's failure-modes table classifies a failed NVS
write as "logged, not a correctness bug" since the RTC has already
been updated by the caller. ntp itself doesn't need the bool result.

- [ ] **Step 4: Add the "internal" comment in nvs_store.cpp**

Edit `firmware/lib/wifi_provision/src/nvs_store.cpp`. Find the
`touch_last_sync` definition at line 96. Add a 1-line internal-use
comment immediately above it. The function should now look like:

```cpp
// Internal — external callers go through wc::wifi_provision::touch_last_sync.
bool touch_last_sync(uint64_t unix_seconds) {
    if (!open_writable()) return false;
    bool ok = prefs().putULong64("last_sync", unix_seconds) > 0;
    close();
    return ok;
}
```

- [ ] **Step 5: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. The new public wrapper compiles + links
against the existing nvs_store function via the forward decl.

- [ ] **Step 6: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 7: Verify the native suite still passes (no regression)**

Run: `cd firmware && pio test -e native`
Expected: **150 tests pass** (no change — wifi_provision adapter
code is `#ifdef ARDUINO`-guarded and the public-header addition is
not used by any native test).

- [ ] **Step 8: Commit**

```bash
git add firmware/lib/wifi_provision/
git commit -m "feat(firmware): public wifi_provision::touch_last_sync wrapper for ntp"
```

---

## Task 6: ntp ESP32 adapter (`begin` + `loop`)

Implements the full adapter per `docs/superpowers/specs/2026-04-17-ntp-design.md`
§Adapter pseudocode. Wraps NTPClient + WiFi + ntp's pure-logic helpers
+ rtc::set_from_epoch + wifi_provision::touch_last_sync. No native
test — adapter is ESP32-only, verified via build + the manual hardware
checks added in Task 8.

**Files:**
- Modify: `firmware/lib/ntp/src/ntp.cpp`

- [ ] **Step 1: Replace the adapter stub with the full implementation**

Replace the contents of `firmware/lib/ntp/src/ntp.cpp` with:

```cpp
// firmware/lib/ntp/src/ntp.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag NTPClient/WiFi into the native build.
// Same pattern as the wifi_provision/buttons/display/rtc adapters.
#ifdef ARDUINO

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

namespace wc::ntp {

static WiFiUDP   udp;
static NTPClient client(udp,
                        "time.google.com",
                        /*timeOffset=*/0,
                        /*updateInterval=*/0 /* bypassed; we call forceUpdate */);

static bool      started               = false;
static bool      ever_synced           = false;
static uint32_t  consecutive_failures  = 0;
static uint64_t  next_deadline_ms      = 0;     // 0 = "fire on next Online"
static uint32_t  prev_millis           = 0;
static uint64_t  extended_now_ms       = 0;     // monotonic across millis() wraps

// Wraps-extended monotonic ms. Avoids the 49.7-day millis() wrap
// bug class for the scheduler's deadline comparisons.
static uint64_t now_ms_extended() {
    uint32_t cur = millis();
    extended_now_ms += static_cast<uint32_t>(cur - prev_millis);
    prev_millis = cur;
    return extended_now_ms;
}

void begin() {
    if (started) return;
    client.begin();
    prev_millis = millis();

    // Warm-boot resume: seed the scheduler from NVS so we don't
    // re-sync immediately if we're still inside the 24h window
    // from a previous successful sync. Without this, every reboot
    // (e.g. flaky USB cable) hammers time.google.com.
    uint32_t sync_age = wc::wifi_provision::seconds_since_last_sync();
    if (sync_age != UINT32_MAX) {
        ever_synced = true;
        constexpr uint32_t NOMINAL_SECS = 86'400u;  // 24h
        uint32_t remaining_secs = (sync_age < NOMINAL_SECS)
                                  ? (NOMINAL_SECS - sync_age) : 0u;
        next_deadline_ms = static_cast<uint64_t>(remaining_secs) * 1000ULL;
        // Skip jitter on resume — the cumulative jitter across many
        // reboots is its own decorrelation source.
    }
    // Else: cold boot, never synced. ever_synced stays false,
    // next_deadline_ms stays 0, scheduler fires on first Online tick.

    started = true;
}

void loop() {
    if (!started) return;
    if (wc::wifi_provision::state() != wc::wifi_provision::State::Online) {
        return;
    }

    uint64_t now = now_ms_extended();
    if (now < next_deadline_ms && (ever_synced || consecutive_failures > 0)) {
        return;  // not yet
    }

    // Pre-flight: hostname resolution. Avoids NTPClient #73 crash.
    // Logs WiFi.status() so DNS-down vs WiFi-just-dropped is
    // distinguishable in field diagnostics.
    IPAddress resolved;
    if (WiFi.hostByName("time.google.com", resolved) != 1) {
        Serial.printf("[ntp] DNS resolution failed (WiFi.status=%d, "
                      "consecutive_failures=%u)\n",
                      WiFi.status(), consecutive_failures + 1);
        consecutive_failures += 1;
        next_deadline_ms = now + next_backoff_ms(consecutive_failures);
        return;
    }

    bool ok = client.forceUpdate();
    if (!ok) {
        Serial.printf("[ntp] forceUpdate() failed "
                      "(consecutive_failures=%u)\n",
                      consecutive_failures + 1);
        consecutive_failures += 1;
        next_deadline_ms = now + next_backoff_ms(consecutive_failures);
        return;
    }

    uint32_t epoch = client.getEpochTime();
    if (!is_plausible_epoch(epoch)) {
        Serial.printf("[ntp] implausible epoch %u "
                      "(consecutive_failures=%u)\n",
                      epoch, consecutive_failures + 1);
        consecutive_failures += 1;
        next_deadline_ms = now + next_backoff_ms(consecutive_failures);
        return;
    }

    wc::rtc::set_from_epoch(epoch);
    wc::wifi_provision::touch_last_sync(static_cast<uint64_t>(epoch));

    ever_synced = true;
    consecutive_failures = 0;
    next_deadline_ms = next_deadline_after_success(now, esp_random());
    Serial.printf("[ntp] sync ok, epoch=%u, next in ~24h\n", epoch);
}

} // namespace wc::ntp

#endif // ARDUINO
```

- [ ] **Step 2: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. The adapter pulls in NTPClient, WiFi,
rtc, and wifi_provision — all already in `lib_deps` or in adjacent
libs. Watch the build output for any unresolved-symbol errors; if
`wc::wifi_provision::touch_last_sync` doesn't link, Task 5 wasn't
applied.

- [ ] **Step 3: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 4: Verify the native suite still passes**

Run: `cd firmware && pio test -e native`
Expected: **150 tests pass** (no change — `ntp.cpp` is
`#ifdef ARDUINO`-guarded).

- [ ] **Step 5: Commit**

```bash
git add firmware/lib/ntp/src/ntp.cpp
git commit -m "feat(firmware): ntp ESP32 adapter (NTPClient + scheduler)"
```

---

## Task 7: Wire ntp into `main.cpp`

Adds `wc::ntp::begin()` to `setup()` (after `rtc::begin()`) and
`wc::ntp::loop()` to `loop()` (alongside the other module loops).
This is the moment the module starts running on real hardware.

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Add the include**

Edit `firmware/src/main.cpp`. Add `#include "ntp.h"` to the includes
at the top. The include block should now be:

```cpp
#include <Arduino.h>
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "ntp.h"
#include "rtc.h"
#include "wifi_provision.h"
```

- [ ] **Step 2: Add `wc::ntp::begin()` to `setup()`**

In `setup()`, find the line `wc::rtc::begin();` (line 14). Add
`wc::ntp::begin();` immediately after the trailing comment block
that follows `wc::rtc::begin()`. The `setup()` function's begin
sequence should become:

```cpp
    wc::wifi_provision::begin();   // runs setenv/tzset on warm boot
    wc::rtc::begin();              // AFTER wifi_provision — load-bearing
                                   // so TZ is set before first now()
    wc::ntp::begin();              // AFTER wifi_provision; warm-boot
                                   // resume reads NVS-stored last-sync
    wc::display::begin();
```

- [ ] **Step 3: Add `wc::ntp::loop()` to `loop()`**

In `loop()`, find the existing `wc::buttons::loop();` line (line 45).
Add `wc::ntp::loop();` immediately after it. The loop's call sequence
should become:

```cpp
void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // sync scheduler; no-op when not Online

    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
```

(The render block is unchanged.)

- [ ] **Step 4: Verify Emory builds clean**

Run: `cd firmware && pio run -e emory`
Expected: build succeeds. Note the resulting flash + RAM usage in
the output line that starts with `RAM:` — should be slightly higher
than baseline but well within the existing budget (was ~63.8% flash
post-wifi_provision; ntp adds an NTPClient instance + ~150 LOC of
adapter).

- [ ] **Step 5: Verify Nora builds clean**

Run: `cd firmware && pio run -e nora`
Expected: build succeeds.

- [ ] **Step 6: Verify the native suite still passes**

Run: `cd firmware && pio test -e native`
Expected: **150 tests pass.** (`main.cpp` is `src_dir`, native build
filter excludes it.)

- [ ] **Step 7: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): bring-up wire-up of ntp in main.cpp"
```

---

## Task 8: Hardware manual-check document

Creates the manual checklist that runs during breadboard bring-up
once the WiFi + DS3231 paths are alive. Mirrors the existing
`wifi_provision_checks.md` / `buttons_checks.md` / `display_checks.md`
/ `rtc_checks.md` shape.

**Files:**
- Create: `firmware/test/hardware_checks/ntp_checks.md`

- [ ] **Step 1: Write the hardware-checks document**

Write `firmware/test/hardware_checks/ntp_checks.md`:

```markdown
# NTP Module — Manual Hardware Checks

These checks exercise the ESP32 paths that the native test suite
cannot reach: real NTPClient UDP exchange, real `WiFi.hostByName`
resolution, real DS3231 round-trip after `set_from_epoch`. Run
during breadboard bring-up after the WiFi (`wifi_provision_checks.md`)
and DS3231 (`rtc_checks.md`) checklists are green.

Spec: `docs/superpowers/specs/2026-04-17-ntp-design.md`
(see §Hardware manual check for source).

## Prerequisites

- ESP32 flashed with the current emory or nora env build.
- Captive-portal flow completed at least once (NVS holds valid
  WiFi creds + TZ).
- DS3231 wired and known-good (rtc_checks.md passed).
- Serial monitor open at 115200 baud.

## Checks

1. **First-ever sync.** Wipe NVS (hold Hour+Audio for 10 s →
   `reset_to_captive`). Walk through captive portal, submit creds.
   After the `[wifi_provision] state -> Online` log line, watch for
   `[ntp] sync ok, epoch=<value>, next in ~24h` within a few
   seconds. Confirm `<value>` is a plausible 2026+ timestamp
   (>= 1767225600). Confirm `wc::rtc::now()` from a follow-up sketch
   reports wall-clock time correctly in the user's TZ.

2. **Sync persists.** Power cycle. After re-`Online`, confirm
   `seconds_since_last_sync()` reflects the wait time (not
   `UINT32_MAX`), and the new sync runs per the 24h schedule (does
   NOT fire immediately unless overdue). Verify by waiting at least
   a minute past `Online` and confirming no new `[ntp] sync ok`
   line appears.

3. **WiFi drop mid-loop.** While running, kill the WiFi (turn off
   AP). Observe serial: ntp module quietly stops attempting (no
   spam). Restore WiFi. Confirm next attempt fires per the
   prevailing schedule (NOT immediately on reconnect unless
   overdue).

4. **DNS failure handling.** Block `time.google.com` at the router
   (DNS sinkhole, e.g. via Pi-hole or `/etc/hosts` of an AP-side
   resolver). Observe `[ntp] DNS resolution failed (WiFi.status=...,
   consecutive_failures=...)` in serial on next attempt. Confirm
   `consecutive_failures` increments each cycle and the cadence
   slows: 30 s → 1 m → 2 m → 4 m → 8 m → 30 m. Unblock; confirm
   next attempt succeeds and the count resets to 0.

5. **Implausible-epoch rejection.** Hard to inject naturally;
   skipped unless a custom test NTP server is set up. Documented
   so a future me knows it's covered by native tests already
   (`test_ntp_validation`).

6. **24-hour cadence sanity.** Power on. Note epoch + millis at
   first sync. Wait 24h ± 30m. Confirm a second sync fires within
   that window. (Long-running test; may be deferred to burn-in
   phase.)

7. **Stale-sync tint.** Sync once successfully. Disconnect WiFi for
   >24h (or block `time.google.com` for the duration). Confirm
   display amber tint activates per the display module's contract.
   This is a cross-module integration check; the tint logic itself
   lives in display.

## Pass criteria

Checks 1–4 must pass before considering the module hardware-verified.
5 is documented-only. 6 + 7 are deferred to burn-in.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/ntp_checks.md
git commit -m "docs(firmware): manual hardware checks for ntp module"
```

---

## Task 9: Mark TODO.md complete

Updates the project's live TODO so the ntp module is checked off
with a one-line shipped-summary mirroring how rtc / display / buttons
got marked complete.

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Update the ntp bullet from `[ ]` to `[x]` with shipped notes**

Edit `TODO.md`. Find the existing ntp bullet (the one updated in the
spec commit; currently reads "every 24 hours (±30 min jitter)"). Replace
the bullet with:

```markdown
- [x] **`ntp`** — NTPClient on boot + every 24 hours (±30 min jitter);
      uses RTC continuously between syncs. Shipped: `firmware/lib/ntp/`
      with pure-logic validation (epoch floor 2026-01-01) + scheduler
      (capped exponential backoff 30s→30m, jittered 24h cadence) and
      ESP32 adapter wrapping NTPClient + WiFi.hostByName. 10 native
      tests; full suite 150/150. Server: `time.google.com`. Spec:
      `docs/superpowers/specs/2026-04-17-ntp-design.md`. Plan:
      `docs/superpowers/plans/2026-04-17-ntp-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/ntp_checks.md`.
```

- [ ] **Step 2: Commit**

```bash
git add TODO.md
git commit -m "docs(todo): mark ntp module complete"
```

---

## Self-review notes

Spec coverage cross-check:

| Spec section | Implemented in |
|---|---|
| `is_plausible_epoch` + MIN_PLAUSIBLE_EPOCH | Task 2 |
| `next_backoff_ms` (capped exponential) | Task 3 |
| `next_deadline_after_success` (24h ± 30m jitter) | Task 4 |
| `wifi_provision::touch_last_sync` public wrapper | Task 5 |
| `nvs_store::touch_last_sync` internal-comment | Task 5 |
| ESP32 adapter (warm-boot resume, scheduler, sync path, observability logs) | Task 6 |
| main.cpp wire-up (begin + loop) | Task 7 |
| Hardware checklist | Task 8 |
| TODO.md mark complete | Task 9 |
| Parent-spec cadence edits + spec commit | Already landed in `71b5a78` |

After all tasks: **9 commits, 150 native tests, 2 ESP32 envs green.**
