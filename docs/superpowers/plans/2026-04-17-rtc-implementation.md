# RTC Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `rtc` firmware module — an RTClib/DS3231 adapter that exposes local-time `DateTime` fields to the display, an epoch setter for the future NTP module, and button-driven hour/minute advance with Mega-matched wrap semantics.

**Architecture:** Hexagonal split. Pure-logic wrap math (`advance_hour_fields`, `advance_minute_fields`) and pure-logic UTC-fields-to-epoch (`utc_epoch_from_fields`, Howard Hinnant `days_from_civil`) are native-testable with Unity. ESP32 adapter (`rtc.cpp`) wraps I²C + RTClib + `localtime_r`/`mktime` under `#ifdef ARDUINO`. Read path is thread-safe (pure arithmetic, no global-TZ mutation); write path uses `mktime(tm_isdst=-1)` which reads-but-doesn't-mutate TZ.

**Tech Stack:** Arduino-ESP32 core, RTClib 2.1.4, `<time.h>` (libc), PlatformIO, Unity C++ test framework.

---

## Spec reference

Implements `docs/superpowers/specs/2026-04-17-rtc-design.md`. Read the spec
in full before starting — it pins every decision (UTC on DS3231, thread-
safety rationale, wrap semantics, failure modes). The plan below
implements that spec exactly; if you hit a decision that feels
underspecified, re-read the spec before inventing.

## File structure

```
firmware/
  lib/rtc/                            # NEW
    include/
      rtc.h                           # ESP32-only public API
      rtc/
        date_time.h                   # DateTime struct (header-only, pure)
        advance.h                     # advance_hour_fields / advance_minute_fields
        epoch.h                       # utc_epoch_from_fields
    src/
      rtc.cpp                         # adapter (#ifdef ARDUINO)
      advance.cpp                     # pure-logic wrap math
      epoch.cpp                       # pure-logic days_from_civil
  test/                               # NEW test dirs
    test_rtc_advance/test_advance.cpp
    test_rtc_epoch/test_epoch.cpp
    hardware_checks/rtc_checks.md     # NEW
  platformio.ini                      # MODIFIED — add rtc include + pure-logic .cpps to native filter
  src/main.cpp                        # MODIFIED — replace hardcoded 14:23 scaffold with wc::rtc::now()
```

Pure-logic `.cpp` files (advance, epoch) go into `[env:native]`'s
`build_src_filter`. `rtc.cpp` is guarded with `#ifdef ARDUINO` — same
reason as `wifi_provision`, `buttons`, `display` adapters (PlatformIO
LDF `deep+` otherwise pulls it into the native build).

---

## Task 1: Library scaffold + DateTime struct + platformio native include path

**Files:**
- Create: `firmware/lib/rtc/include/rtc/date_time.h`
- Modify: `firmware/platformio.ini` (native build_flags only — add `-I lib/rtc/include`)

- [ ] **Step 1: Create the DateTime header**

Write `firmware/lib/rtc/include/rtc/date_time.h`:

```cpp
// firmware/lib/rtc/include/rtc/date_time.h
#pragma once

#include <cstdint>

namespace wc::rtc {

// Local wall-clock fields (output of now(), input to advance wrap
// math). UTC is an internal detail of the adapter and never escapes
// the rtc module's public API except via the uint32_t epoch argument
// to set_from_epoch().
struct DateTime {
    uint16_t year;    // full year, e.g. 2030. No 2-digit encoding.
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23
    uint8_t  minute;  // 0-59
    uint8_t  second;  // 0-59
};

constexpr bool operator==(const DateTime& a, const DateTime& b) {
    return a.year == b.year && a.month == b.month && a.day == b.day
        && a.hour == b.hour && a.minute == b.minute
        && a.second == b.second;
}

constexpr bool operator!=(const DateTime& a, const DateTime& b) {
    return !(a == b);
}

} // namespace wc::rtc
```

- [ ] **Step 2: Wire the rtc include path into `[env:native]`**

Edit `firmware/platformio.ini`. Find the existing `[env:native]` block's
`build_flags` that ends with `-I lib/display/include` and add a new
include line for rtc. Replace this block:

```ini
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
    -I lib/buttons/include
    -I lib/display/include
```

with:

```ini
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
    -I lib/buttons/include
    -I lib/display/include
    -I lib/rtc/include
```

Do NOT touch `build_src_filter` yet — no `.cpp` files exist to compile.
Do NOT touch `[esp32-base]` or the `emory`/`nora` envs; they already
have `-I lib/*/include` via PlatformIO's default library-include
behavior for any `firmware/lib/*/include` directory.

- [ ] **Step 3: Verify native build still passes with the header added**

Run: `cd firmware && pio test -e native`
Expected: all existing tests pass (124 tests green, same as pre-rtc).
`date_time.h` isn't included anywhere yet so it can't break anything.

- [ ] **Step 4: Commit**

```bash
cd firmware
git add lib/rtc/include/rtc/date_time.h platformio.ini
git commit -m "feat(firmware): rtc scaffold + DateTime struct"
```

---

## Task 2: Pure-logic `utc_epoch_from_fields` (TDD)

**Files:**
- Create: `firmware/test/test_rtc_epoch/test_epoch.cpp`
- Create: `firmware/lib/rtc/include/rtc/epoch.h`
- Create: `firmware/lib/rtc/src/epoch.cpp`
- Modify: `firmware/platformio.ini` (native `build_src_filter` — add `epoch.cpp`)

- [ ] **Step 1: Write the failing test file**

Write `firmware/test/test_rtc_epoch/test_epoch.cpp`:

```cpp
#include <ctime>
#include <unity.h>
#include "rtc/epoch.h"

using namespace wc::rtc;

void setUp(void) {}
void tearDown(void) {}

// Unix epoch starts at 1970-01-01 00:00:00 UTC by definition.
void test_utc_epoch_from_fields_at_unix_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0u,
        utc_epoch_from_fields({1970, 1, 1, 0, 0, 0}));
}

// RTClib's SECONDS_FROM_1970_TO_2000 boundary. Cross-verifiable via
// `date -u -d '2000-01-01 00:00:00' +%s` on any Unix host.
void test_utc_epoch_from_fields_at_2000_01_01(void) {
    TEST_ASSERT_EQUAL_UINT32(946684800u,
        utc_epoch_from_fields({2000, 1, 1, 0, 0, 0}));
}

// 2028 is a leap year — Feb 29 exists. A one-day offset between
// 2028-02-29 00:00:00 and 2028-03-01 00:00:00 must equal 86400 seconds.
void test_utc_epoch_from_fields_across_leap_day(void) {
    uint32_t feb29 = utc_epoch_from_fields({2028, 2, 29, 0, 0, 0});
    uint32_t mar01 = utc_epoch_from_fields({2028, 3, 1, 0, 0, 0});
    TEST_ASSERT_EQUAL_UINT32(86400u, mar01 - feb29);
}

// Emory's birthday minute projected as UTC for arithmetic test.
// `date -u -d '2030-10-06 18:10:00' +%s` = 1917720600.
void test_utc_epoch_from_fields_at_2030_10_06_18_10_00(void) {
    TEST_ASSERT_EQUAL_UINT32(1917720600u,
        utc_epoch_from_fields({2030, 10, 6, 18, 10, 0}));
}

// Cross-check against the host's libc timegm() at 10 arbitrary points
// spanning the daughters' 2026-2100 lifespan. Catches any arithmetic
// bug in days_from_civil against an independently-implemented reference.
void test_utc_epoch_from_fields_round_trips_through_gmtime(void) {
    const DateTime cases[] = {
        {2026,  1,  1,  0,  0,  0},
        {2026, 12, 31, 23, 59, 59},
        {2030,  6, 15, 12,  0,  0},
        {2032,  2, 29,  6, 30, 45},   // leap day
        {2040,  7,  4,  9, 15, 30},
        {2050, 11, 22, 18, 45, 12},
        {2060,  3,  3,  3,  3,  3},
        {2070,  8,  8,  8,  8,  8},
        {2080, 12, 25, 16, 20,  0},
        {2099,  6, 30,  0,  0,  1},
    };
    for (const auto& dt : cases) {
        struct tm t{};
        t.tm_year = dt.year - 1900;
        t.tm_mon  = dt.month - 1;
        t.tm_mday = dt.day;
        t.tm_hour = dt.hour;
        t.tm_min  = dt.minute;
        t.tm_sec  = dt.second;
        time_t expected = timegm(&t);
        TEST_ASSERT_EQUAL_UINT32(
            static_cast<uint32_t>(expected),
            utc_epoch_from_fields(dt));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_utc_epoch_from_fields_at_unix_zero);
    RUN_TEST(test_utc_epoch_from_fields_at_2000_01_01);
    RUN_TEST(test_utc_epoch_from_fields_across_leap_day);
    RUN_TEST(test_utc_epoch_from_fields_at_2030_10_06_18_10_00);
    RUN_TEST(test_utc_epoch_from_fields_round_trips_through_gmtime);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test to confirm it fails to compile**

Run: `cd firmware && pio test -e native -f test_rtc_epoch`
Expected: **compilation error** — `rtc/epoch.h: No such file or directory`.
This confirms the test wiring is hooked up and we're in red.

- [ ] **Step 3: Create the epoch header**

Write `firmware/lib/rtc/include/rtc/epoch.h`:

```cpp
// firmware/lib/rtc/include/rtc/epoch.h
#pragma once

#include <cstdint>
#include "rtc/date_time.h"

namespace wc::rtc {

// Caller guarantees valid input ranges:
//   year  >= 1970, month ∈ [1, 12], day ∈ [1, 31],
//   hour  ∈ [0, 23], minute ∈ [0, 59], second ∈ [0, 59].
// Input fields MUST represent a UTC date-time; no TZ conversion
// happens here.
//
// Pure. Returns seconds since 1970-01-01 00:00:00 UTC, computed via
// Howard Hinnant's days_from_civil algorithm — no libc dependency,
// no TZ state access, safe to call from any task.
//
// Implementation note: the intermediate day-count fits in int32_t
// across the daughters' 2026-2106 lifespan; the output is uint32_t
// and overflows in 2106 (same ceiling as RTClib's DateTime(uint32_t)
// constructor, so widening here would not help the chip).
uint32_t utc_epoch_from_fields(DateTime utc);

} // namespace wc::rtc
```

- [ ] **Step 4: Re-run test, confirm still failing (now on linker)**

Run: `cd firmware && pio test -e native -f test_rtc_epoch`
Expected: **link error** — `undefined reference to wc::rtc::utc_epoch_from_fields`.
Moves us from "header missing" to "implementation missing."

- [ ] **Step 5: Create the epoch implementation**

Write `firmware/lib/rtc/src/epoch.cpp`:

```cpp
// firmware/lib/rtc/src/epoch.cpp
//
// Pure logic — no Arduino, no hardware, safe to compile in native env.
// Howard Hinnant's days_from_civil:
//   http://howardhinnant.github.io/date_algorithms.html#days_from_civil
// Returns number of days since the civil epoch 1970-01-01 for a given
// (year, month, day) triple. Tested in test_rtc_epoch against the
// host's timegm() at 10 spread-out dates to catch any transcription
// bug.
#include "rtc/epoch.h"

namespace wc::rtc {

namespace {
int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
    y -= m <= 2;
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}
}  // namespace

uint32_t utc_epoch_from_fields(DateTime utc) {
    int32_t days = days_from_civil(
        static_cast<int32_t>(utc.year),
        static_cast<uint32_t>(utc.month),
        static_cast<uint32_t>(utc.day));
    return static_cast<uint32_t>(days) * 86400u
         + static_cast<uint32_t>(utc.hour)   * 3600u
         + static_cast<uint32_t>(utc.minute) * 60u
         + static_cast<uint32_t>(utc.second);
}

} // namespace wc::rtc
```

- [ ] **Step 6: Wire `epoch.cpp` into `[env:native]` build_src_filter**

Edit `firmware/platformio.ini`. Find the existing `[env:native]`'s
`build_src_filter` ending with `+<../lib/display/src/renderer.cpp>`
and add an rtc line. Replace:

```ini
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
```

with:

```ini
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
```

- [ ] **Step 7: Run test — confirm all 5 pass**

Run: `cd firmware && pio test -e native -f test_rtc_epoch`
Expected: 5/5 pass in `test_rtc_epoch`. Full native suite (re-run
without `-f`) should show 124 pre-existing tests + 5 new = 129 total.

If the `test_utc_epoch_from_fields_round_trips_through_gmtime` test
fails on a host without BSD-extension `timegm` (rare — macOS and glibc
have it), the fallback is the per-platform `_mkgmtime` on MSVC. This
project runs native tests on darwin (macOS) per the environment; if
CI later moves to Windows, that test would need a compat shim. Not
blocking for v0.

- [ ] **Step 8: Commit**

```bash
cd firmware
git add lib/rtc/include/rtc/epoch.h lib/rtc/src/epoch.cpp \
        test/test_rtc_epoch/test_epoch.cpp platformio.ini
git commit -m "feat(firmware): pure-logic utc_epoch_from_fields (rtc)"
```

---

## Task 3: Pure-logic `advance_hour_fields` + `advance_minute_fields` (TDD)

**Files:**
- Create: `firmware/test/test_rtc_advance/test_advance.cpp`
- Create: `firmware/lib/rtc/include/rtc/advance.h`
- Create: `firmware/lib/rtc/src/advance.cpp`
- Modify: `firmware/platformio.ini` (native `build_src_filter` — add `advance.cpp`)

- [ ] **Step 1: Write all 11 failing tests**

Write `firmware/test/test_rtc_advance/test_advance.cpp`:

```cpp
#include <unity.h>
#include "rtc/advance.h"

using namespace wc::rtc;

void setUp(void) {}
void tearDown(void) {}

// Helper for concise equality assertions on a DateTime.
static void assert_dt_equal(const DateTime& expected,
                             const DateTime& actual) {
    TEST_ASSERT_EQUAL_UINT16(expected.year,   actual.year);
    TEST_ASSERT_EQUAL_UINT8 (expected.month,  actual.month);
    TEST_ASSERT_EQUAL_UINT8 (expected.day,    actual.day);
    TEST_ASSERT_EQUAL_UINT8 (expected.hour,   actual.hour);
    TEST_ASSERT_EQUAL_UINT8 (expected.minute, actual.minute);
    TEST_ASSERT_EQUAL_UINT8 (expected.second, actual.second);
}

// -- advance_hour_fields -------------------------------------------

void test_advance_hour_increments_by_one(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 45},
        advance_hour_fields({2030, 5, 15, 14, 23, 45}));
}

void test_advance_hour_wraps_23_to_0(void) {
    assert_dt_equal({2030, 5, 15, 0, 23, 45},
        advance_hour_fields({2030, 5, 15, 23, 23, 45}));
}

void test_advance_hour_no_day_carry_on_new_years_eve(void) {
    assert_dt_equal({2030, 12, 31, 0, 23, 45},
        advance_hour_fields({2030, 12, 31, 23, 23, 45}));
}

void test_advance_hour_preserves_second_at_zero(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 0},
        advance_hour_fields({2030, 5, 15, 14, 23, 0}));
}

void test_advance_hour_preserves_second_at_59(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 59},
        advance_hour_fields({2030, 5, 15, 14, 23, 59}));
}

// -- advance_minute_fields -----------------------------------------

void test_advance_minute_adds_5_from_aligned(void) {
    assert_dt_equal({2030, 5, 15, 14, 30, 0},
        advance_minute_fields({2030, 5, 15, 14, 25, 45}));
}

void test_advance_minute_adds_5_then_floors(void) {
    // 23 + 5 = 28 → floor(28/5)*5 = 25.
    assert_dt_equal({2030, 5, 15, 14, 25, 0},
        advance_minute_fields({2030, 5, 15, 14, 23, 45}));
}

void test_advance_minute_at_55_carries_to_hour(void) {
    assert_dt_equal({2030, 5, 15, 15, 0, 0},
        advance_minute_fields({2030, 5, 15, 14, 55, 45}));
}

void test_advance_minute_at_23_55_wraps_hour_and_stays_on_same_day(void) {
    assert_dt_equal({2030, 5, 15, 0, 0, 0},
        advance_minute_fields({2030, 5, 15, 23, 55, 45}));
}

void test_advance_minute_at_57_carries_past_60_and_floors(void) {
    // 57 + 5 = 62 → 62 - 60 = 2 → floor(2/5)*5 = 0. Hour carries.
    assert_dt_equal({2030, 5, 15, 15, 0, 0},
        advance_minute_fields({2030, 5, 15, 14, 57, 45}));
}

void test_advance_minute_preserves_year_month_day_at_calendar_edge(void) {
    assert_dt_equal({2030, 12, 31, 14, 30, 0},
        advance_minute_fields({2030, 12, 31, 14, 25, 45}));
}

int main(int, char**) {
    UNITY_BEGIN();
    // advance_hour_fields
    RUN_TEST(test_advance_hour_increments_by_one);
    RUN_TEST(test_advance_hour_wraps_23_to_0);
    RUN_TEST(test_advance_hour_no_day_carry_on_new_years_eve);
    RUN_TEST(test_advance_hour_preserves_second_at_zero);
    RUN_TEST(test_advance_hour_preserves_second_at_59);
    // advance_minute_fields
    RUN_TEST(test_advance_minute_adds_5_from_aligned);
    RUN_TEST(test_advance_minute_adds_5_then_floors);
    RUN_TEST(test_advance_minute_at_55_carries_to_hour);
    RUN_TEST(test_advance_minute_at_23_55_wraps_hour_and_stays_on_same_day);
    RUN_TEST(test_advance_minute_at_57_carries_past_60_and_floors);
    RUN_TEST(test_advance_minute_preserves_year_month_day_at_calendar_edge);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails to compile**

Run: `cd firmware && pio test -e native -f test_rtc_advance`
Expected: **compilation error** — `rtc/advance.h: No such file or directory`.

- [ ] **Step 3: Create the advance header**

Write `firmware/lib/rtc/include/rtc/advance.h`:

```cpp
// firmware/lib/rtc/include/rtc/advance.h
#pragma once

#include "rtc/date_time.h"

namespace wc::rtc {

// Caller guarantees valid input ranges:
//   year  >= 2023, month ∈ [1, 12], day ∈ [1, 31],
//   hour  ∈ [0, 23], minute ∈ [0, 59], second ∈ [0, 59].
// Wrap math trusts these and does not re-validate — invalid input
// yields unspecified but non-crashing output. The only legitimate
// runtime source is wc::rtc::now(), which produces valid ranges by
// construction via localtime_r.

// Pure. Returns a new DateTime with hour advanced by 1
// (wrapping 23 -> 0). Minute, second, year, month, day UNCHANGED.
// No date carry on hour wrap.
// Consumers: rtc adapter's advance_hour() read-modify-write path.
DateTime advance_hour_fields(DateTime in);

// Pure. Returns a new DateTime with "minute button" semantics:
//
//   raw = in.minute + 5
//   if raw >= 60:
//       raw -= 60
//       hour = (in.hour + 1) % 24     // carry to hour
//   out.minute = (raw / 5) * 5        // floor to 5-min block
//   out.second = 0                    // reset
//   year/month/day preserved          // NO day carry
//
// Matches Mega reference's clockAdvanceMinute5.
DateTime advance_minute_fields(DateTime in);

} // namespace wc::rtc
```

- [ ] **Step 4: Re-run test, confirm linker fails**

Run: `cd firmware && pio test -e native -f test_rtc_advance`
Expected: **link error** — `undefined reference to wc::rtc::advance_hour_fields` and `...advance_minute_fields`.

- [ ] **Step 5: Create the advance implementation**

Write `firmware/lib/rtc/src/advance.cpp`:

```cpp
// firmware/lib/rtc/src/advance.cpp
//
// Pure logic — no Arduino, no hardware. Mirrors the Mega reference's
// clockAdvanceHour / clockAdvanceMinute5 wrap semantics:
//   hour   — (h+1) % 24, no date carry
//   minute — add 5, carry to hour at 60, no date carry, second -> 0,
//            then floor to 5-min block
// See rtc/advance.h for the caller-validity contract.
#include "rtc/advance.h"

namespace wc::rtc {

DateTime advance_hour_fields(DateTime in) {
    in.hour = static_cast<uint8_t>((in.hour + 1) % 24);
    return in;
}

DateTime advance_minute_fields(DateTime in) {
    uint8_t raw = static_cast<uint8_t>(in.minute + 5);
    if (raw >= 60) {
        raw = static_cast<uint8_t>(raw - 60);
        in.hour = static_cast<uint8_t>((in.hour + 1) % 24);
    }
    in.minute = static_cast<uint8_t>((raw / 5) * 5);
    in.second = 0;
    return in;
}

} // namespace wc::rtc
```

- [ ] **Step 6: Wire `advance.cpp` into `[env:native]` build_src_filter**

Edit `firmware/platformio.ini`. Find the `build_src_filter` block
(which now ends with `+<../lib/rtc/src/epoch.cpp>` from Task 2) and
append the advance line. The block becomes:

```ini
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
```

- [ ] **Step 7: Run test — confirm all 11 pass**

Run: `cd firmware && pio test -e native -f test_rtc_advance`
Expected: 11/11 pass.

- [ ] **Step 8: Run full native suite**

Run: `cd firmware && pio test -e native`
Expected: **140 tests pass** (124 pre-rtc + 5 epoch + 11 advance).

- [ ] **Step 9: Commit**

```bash
cd firmware
git add lib/rtc/include/rtc/advance.h lib/rtc/src/advance.cpp \
        test/test_rtc_advance/test_advance.cpp platformio.ini
git commit -m "feat(firmware): pure-logic advance_hour/minute_fields (rtc)"
```

---

## Task 4: ESP32 adapter (`rtc.h` + `rtc.cpp`)

The adapter is NOT native-testable — it depends on RTClib + `<Arduino.h>`
+ `<Wire.h>`. Verification is: (a) the ESP32 environments build cleanly
for both `emory` and `nora`, (b) the manual hardware checklist in
Task 6 exercises runtime behavior once parts are on the breadboard.

**Files:**
- Create: `firmware/lib/rtc/include/rtc.h`
- Create: `firmware/lib/rtc/src/rtc.cpp`

- [ ] **Step 1: Create the public API header**

Write `firmware/lib/rtc/include/rtc.h`:

```cpp
// firmware/lib/rtc/include/rtc.h
//
// ESP32-only public API. Depends on Arduino.h, RTClib, and libc
// time.h. Include ONLY from translation units that compile under
// the Arduino toolchain (the emory / nora PlatformIO envs).
// Native-env tests include the pure-logic headers under rtc/ —
// never this header.
#pragma once

#include <Arduino.h>
#include <cstdint>
#include "rtc/date_time.h"

namespace wc::rtc {

// Initialize I²C (SDA=21, SCL=22) and the DS3231 via RTClib.
// Idempotent — safe to call more than once.
//
// MUST be called AFTER wc::wifi_provision::begin() so that
// setenv("TZ", ...) + tzset() have run — otherwise now() silently
// returns UTC fields (libc default TZ is UTC). begin() emits a
// soft warning via Serial if TZ is unset at call time.
//
// If the DS3231 is not present on the bus, logs an error and
// returns. now() will still return whatever the chip yields
// (typically the last-written value or a 2000-01-01 default on a
// dead battery) so the rest of the firmware keeps running.
void begin();

// Read current LOCAL wall-clock fields from the DS3231.
// Internally: read UTC fields from chip, convert to UTC epoch via
// the pure utc_epoch_from_fields() helper (no TZ mutation),
// localtime_r() using the current POSIX TZ, return.
DateTime now();

// Write UTC epoch (seconds since 1970-01-01 UTC) to the DS3231.
// Caller (ntp module) is responsible for:
//   - Validating the epoch is sane before calling.
//   - Calling wifi_provision::nvs_store::touch_last_sync(unix_seconds)
//     separately on success — rtc does NOT track last-sync.
void set_from_epoch(uint32_t unix_seconds);

// Button callback. Read current UTC, project to local, apply
// advance_hour_fields(), project local back to UTC, write.
// See rtc/advance.h for wrap semantics.
void advance_hour();

// Button callback. Same round-trip as advance_hour, but with
// advance_minute_fields() in the middle.
void advance_minute();

} // namespace wc::rtc
```

- [ ] **Step 2: Create the adapter implementation**

Write `firmware/lib/rtc/src/rtc.cpp`:

```cpp
// firmware/lib/rtc/src/rtc.cpp
//
// Guarded with #ifdef ARDUINO — same pattern as wifi_provision /
// buttons / display adapters. PlatformIO LDF's deep+ mode would
// otherwise pull this TU into the native test build, where RTClib
// and Arduino.h don't exist.
#ifdef ARDUINO

#if !defined(ARDUINO_ARCH_ESP32)
  #error "rtc requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include <cstdlib>
#include "rtc.h"
#include "rtc/advance.h"
#include "rtc/date_time.h"
#include "rtc/epoch.h"
#include "pinmap.h"     // PIN_I2C_SDA, PIN_I2C_SCL

namespace wc::rtc {

static RTC_DS3231 ds3231;
static bool started = false;

// Interpret DateTime fields as LOCAL per the current TZ and return
// the corresponding UTC epoch. mktime respects tzset + POSIX DST
// rule when tm_isdst = -1. Only called from the advance paths
// (button-press frequency) — never from the hot render path.
// mktime reads the TZ state but does not mutate it, and
// wifi_provision::start_sta() calls tzset from the main-loop only,
// so no concurrent-mutation race applies here.
static time_t local_fields_to_utc_epoch(const DateTime& local) {
    struct tm t{};
    t.tm_year  = local.year - 1900;
    t.tm_mon   = local.month - 1;
    t.tm_mday  = local.day;
    t.tm_hour  = local.hour;
    t.tm_min   = local.minute;
    t.tm_sec   = local.second;
    t.tm_isdst = -1;
    return mktime(&t);
}

static DateTime from_tm(const struct tm& t) {
    return DateTime{
        static_cast<uint16_t>(t.tm_year + 1900),
        static_cast<uint8_t>(t.tm_mon + 1),
        static_cast<uint8_t>(t.tm_mday),
        static_cast<uint8_t>(t.tm_hour),
        static_cast<uint8_t>(t.tm_min),
        static_cast<uint8_t>(t.tm_sec),
    };
}

void begin() {
    if (started) return;
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (!ds3231.begin()) {
        Serial.println("[rtc] ERROR: DS3231 not found on I²C bus");
        // Intentionally continue — lets the rest of the firmware
        // keep running on bad-time rather than halting.
    }
    if (getenv("TZ") == nullptr) {
        Serial.println("[rtc] warning: TZ env var unset at begin() "
                       "— expected only during captive-portal boot; "
                       "if seen on warm boot, setup() ordering is wrong");
    }
    started = true;
}

DateTime now() {
    ::DateTime dt_utc = ds3231.now();       // RTClib's DateTime
    DateTime utc{
        static_cast<uint16_t>(dt_utc.year()),
        static_cast<uint8_t>(dt_utc.month()),
        static_cast<uint8_t>(dt_utc.day()),
        static_cast<uint8_t>(dt_utc.hour()),
        static_cast<uint8_t>(dt_utc.minute()),
        static_cast<uint8_t>(dt_utc.second()),
    };
    // Pure-arithmetic UTC fields → UTC epoch. No TZ mutation; safe
    // to call concurrently with the WiFi task's time functions.
    uint32_t utc_epoch = utc_epoch_from_fields(utc);
    time_t t = static_cast<time_t>(utc_epoch);
    struct tm local_tm{};
    localtime_r(&t, &local_tm);             // reads TZ state, no mutation
    return from_tm(local_tm);
}

void set_from_epoch(uint32_t unix_seconds) {
    ds3231.adjust(::DateTime(unix_seconds));
}

void advance_hour() {
    DateTime local    = now();
    DateTime advanced = advance_hour_fields(local);
    time_t   new_utc  = local_fields_to_utc_epoch(advanced);
    ds3231.adjust(::DateTime(static_cast<uint32_t>(new_utc)));
}

void advance_minute() {
    DateTime local    = now();
    DateTime advanced = advance_minute_fields(local);
    time_t   new_utc  = local_fields_to_utc_epoch(advanced);
    ds3231.adjust(::DateTime(static_cast<uint32_t>(new_utc)));
}

} // namespace wc::rtc

#endif // ARDUINO
```

- [ ] **Step 3: Build for `emory` env, expect success**

Run: `cd firmware && pio run -e emory`
Expected: `SUCCESS` with `RAM` + `Flash` percentages reported.
Nothing in main.cpp calls the rtc module yet, so it's linked-but-unused.
The intent of this step is to catch syntax errors and unresolved
symbols before wiring it into main.

If you see `undefined reference to wc::rtc::...` — check that `rtc.cpp`
is outside `#ifdef ARDUINO` guards where it shouldn't be (the whole
file IS guarded; function definitions inside should still be visible
to the linker for the ARDUINO build).

- [ ] **Step 4: Build for `nora` env, expect success**

Run: `cd firmware && pio run -e nora`
Expected: `SUCCESS`. Both per-kid builds compile the same rtc module.

- [ ] **Step 5: Re-run full native suite to confirm native env is unaffected**

Run: `cd firmware && pio test -e native`
Expected: **140 tests pass** (unchanged from Task 3). The adapter
under `#ifdef ARDUINO` is invisible to the native build.

- [ ] **Step 6: Commit**

```bash
cd firmware
git add lib/rtc/include/rtc.h lib/rtc/src/rtc.cpp
git commit -m "feat(firmware): rtc ESP32 adapter (DS3231 via RTClib)"
```

---

## Task 5: Wire rtc into main.cpp — replace hardcoded 14:23 scaffold

**Files:**
- Modify: `firmware/src/main.cpp`

Today `firmware/src/main.cpp` paints a hardcoded May 15 2027, 14:23
frame whenever `wifi_provision::state() == Online` (lines 48-64). It
routes HourTick / MinuteTick to `Serial.println` placeholders
(lines 19-24). Once rtc lands, both become real.

- [ ] **Step 1: Replace main.cpp in its entirety**

Write `firmware/src/main.cpp`:

```cpp
// firmware/src/main.cpp
#include <Arduino.h>
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "rtc.h"
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();   // runs setenv/tzset on warm boot
    wc::rtc::begin();              // AFTER wifi_provision — load-bearing
                                   // so TZ is set before first now()
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
        // Blank while not Online (captive portal runs here). The
        // future main.cpp spec may paint something prettier, e.g.
        // a slow-pulsing decor word to cue "setup mode".
        wc::display::show(wc::display::Frame{});
    }

    delay(1);  // yield to IDLE for watchdog + WiFi
}
```

Note the deliberate changes from the scaffold:
- New `#include "rtc.h"`.
- `wc::rtc::begin()` inserted between `wifi_provision::begin()` and
  `display::begin()` — sequencing is documented as load-bearing in
  the spec.
- HourTick / MinuteTick case bodies now call the rtc advance functions
  instead of logging placeholders.
- The hardcoded `in.year = 2027 … in.minute = 23` block is replaced
  with a read of `wc::rtc::now()` and field-by-field copy into
  `RenderInput`. The display renderer is unchanged — it consumes
  local fields exactly like before.

- [ ] **Step 2: Build for `emory` env, expect success**

Run: `cd firmware && pio run -e emory`
Expected: `SUCCESS`. The build should be slightly larger than
pre-rtc (rtc adapter now linked + called).

- [ ] **Step 3: Build for `nora` env, expect success**

Run: `cd firmware && pio run -e nora`
Expected: `SUCCESS`.

- [ ] **Step 4: Re-run full native suite**

Run: `cd firmware && pio test -e native`
Expected: **140 tests pass** (unchanged — main.cpp is not compiled
by the native env, only the ESP32 envs).

- [ ] **Step 5: Commit**

```bash
cd firmware
git add src/main.cpp
git commit -m "feat(firmware): bring-up wire-up of rtc in main.cpp"
```

---

## Task 6: Hardware manual-check markdown

**Files:**
- Create: `firmware/test/hardware_checks/rtc_checks.md`

Not automatable — runs during the breadboard bring-up session once
DS3231 is wired per `docs/hardware/breadboard-bring-up-guide.md`
§Step 6. Same shape as `buttons_checks.md` / `display_checks.md`.

- [ ] **Step 1: Create the rtc hardware check document**

Write `firmware/test/hardware_checks/rtc_checks.md`:

```markdown
<!-- firmware/test/hardware_checks/rtc_checks.md -->
# rtc — manual hardware verification

Run these after the DS3231 is wired onto the breadboard per
`docs/hardware/breadboard-bring-up-guide.md` §Step 6. I²C pins
SDA=GPIO 21, SCL=GPIO 22. CR2032 installed (after the charging-
resistor removal in #1 below).

Flash: `pio run -e emory -t upload`
Serial: `pio device monitor -e emory`

## 1. Charging-resistor removal — pre-flight SAFETY CHECK

**Do this BEFORE inserting a CR2032 coin cell.** The ZS-042 DS3231
module ships with a 200 Ω + 1N4148 trickle-charge circuit intended
for rechargeable LIR2032 cells. A non-rechargeable CR2032 installed
on this circuit can **leak, vent, or explode**. See
`docs/hardware/pinout.md` §Critical issues #1.

- [ ] Locate the 200 Ω resistor near the battery holder on the
      ZS-042 PCB, in series between Vcc and battery+.
- [ ] Desolder it, cut the trace, or snip it out with diagonals.
      Verify no 200 Ω path remains between Vcc and battery+.
- [ ] Insert a fresh CR2032 (+ side up).

Do this on every DS3231 board. One per clock.

## 2. I²C probe — DS3231 detected at 0x68

- [ ] With the DS3231 wired (SDA=21, SCL=22, VCC=3.3V, GND) and
      CR2032 installed, flash: `pio run -e emory -t upload`.
- [ ] Open serial: `pio device monitor -e emory`.
- [ ] Boot log should NOT show `[rtc] ERROR: DS3231 not found on
      I²C bus`. If it does: re-seat the SDA/SCL wires, verify 3.3V
      at the DS3231 VCC pad, verify the pullup resistors on the
      DS3231 module (4.7 kΩ–10 kΩ between SDA-Vcc and SCL-Vcc).
- [ ] Also verify: no `[rtc] warning: TZ env var unset` on a warm
      boot (creds already provisioned). That warning is expected
      only during captive-portal boot; on a warm boot it means
      `setup()` ordering is wrong (rtc::begin ran before
      wifi_provision::begin).

## 3. Round-trip read/write

Temporarily patch `setup()` in `firmware/src/main.cpp` just before
the `wc::buttons::begin(...)` line:

```cpp
wc::rtc::set_from_epoch(1776542400u);  // 2026-04-17 22:00:00 UTC
```

And add inside `loop()` just before `delay(1)`:

```cpp
static uint32_t last_log = 0;
if (millis() - last_log >= 1000) {
    auto dt = wc::rtc::now();
    Serial.printf("[rtc] %04u-%02u-%02u %02u:%02u:%02u\n",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
    last_log = millis();
}
```

Reflash.

- [ ] First log line (after Online) projects 2026-04-17 22:00:00 UTC
      through the current TZ. With Pacific PDT (-7h at that date):
      `2026-04-17 15:00:00` (or ±1 minute depending on the
      NVS-to-tzset-to-first-read delay).
- [ ] Subsequent log lines increment `second` by 1 per tick.
- [ ] After 60 s: `minute` increments from 00 to 01; `second`
      rolls back to 00.

Revert the patch when this check passes.

## 4. Button advance — hour

- [ ] Leave the logging patch from #3 in place. Remove the
      `set_from_epoch` patch. Flash.
- [ ] Note the current `hour` from the log. Press the Hour button
      (GPIO 32) once. The next log line shows `hour` incremented
      by 1; `minute`, `day`, `month`, `year` preserved.
- [ ] Press Hour 23 more times (total 24 presses). Verify the
      `hour` value returned to its initial value and `day` /
      `month` / `year` are unchanged across the full cycle.
      Confirms `(hour + 1) % 24` with no date carry.

## 5. Button advance — minute

- [ ] Press the Minute button (GPIO 33). Log shows `minute` jumps
      by 5 and rounds to a 5-minute block (e.g. from :23 → :25,
      NOT :28). `second` zeroes.
- [ ] Press Minute repeatedly until :55. Press Minute one more
      time. Log shows `minute` wraps to `:00` and `hour`
      increments by 1. Date unchanged.
- [ ] If current time is near 23:55, one more Minute press should
      produce `hour=0` on the SAME day (no date carry).

Revert the `loop()` logging patch after this check passes. Keep
the patch out of committed code.

## 6. Battery-backed retention

- [ ] After a successful NTP sync (confirmed by log showing
      wall-clock time in the correct TZ), unplug USB power.
- [ ] Wait **5 minutes**. Short outages can be carried by the
      DS3231's onboard decoupling capacitance alone; 5 minutes
      forces the CR2032 to actually source current.
- [ ] Re-plug USB. Flash monitoring open: boot log should show
      `now()` resuming approximately `(time-before-unplug) + 5min`,
      NOT 1970-01-01 or 2000-01-01 (either would indicate the
      oscillator reset). Confirms CR2032 is providing backup.

## 7. NTP handoff — deferred until ntp module ships

Once the `ntp` firmware module is wired, boot the clock on WiFi
and confirm:

- [ ] `[rtc]` log line wall-clock time matches a reference clock
      (your phone) within ~10 seconds of boot.
- [ ] Over the next 6 hours (the NTP sync cadence), the clock
      stays visually aligned with the reference. Small sub-minute
      drift is expected; hour/minute drift is a bug.

## 8. DST transition — opportunistic

Hard to time in bring-up unless a transition is near. If one is:

- [ ] Temporarily `set_from_epoch(unix_seconds_5min_before_transition)`
      using the patch from #3.
- [ ] Observe log lines across the transition boundary without
      any further writes. Local fields should jump by ±1 hour at
      the transition moment; UTC on the chip stays correct.

Skip if no transition is within bring-up's window. The spec's
open-issues item (`mktime` + `tzset` DST behavior on ESP32
newlib) is verified here when the opportunity arises.
```

- [ ] **Step 2: Commit**

```bash
cd firmware
git add test/hardware_checks/rtc_checks.md
git commit -m "docs(firmware): manual hardware checks for rtc module"
```

---

## Task 7: Update TODO.md — mark rtc complete

**Files:**
- Modify: `TODO.md` (repo root, not firmware root)

- [ ] **Step 1: Find the rtc entry in TODO.md**

Current entry in `TODO.md` §"Phase 2 firmware modules" (around line 86):

```
- [ ] **`rtc`** — DS3231 read/write via RTClib; remove the ZS-042 battery-
      charging resistor before inserting CR2032. Thin adapter — minimal spec.
```

- [ ] **Step 2: Replace with a completed entry matching the display
      + buttons + wifi_provision format**

Find the unchecked `- [ ] **rtc** — …` line and replace with:

```
- [x] **`rtc`** — DS3231 read/write via RTClib. Shipped:
      `firmware/lib/rtc/` with pure-logic wrap math
      (`advance_hour_fields`, `advance_minute_fields`) and pure-logic
      UTC-fields→epoch (`utc_epoch_from_fields`, Howard Hinnant
      days_from_civil), plus an ESP32 adapter. 16 native tests (5
      epoch + 11 advance); full suite 140/140. UTC stored on chip;
      libc `localtime_r` applies POSIX TZ set by wifi_provision.
      Spec: `docs/superpowers/specs/2026-04-17-rtc-design.md`.
      Plan: `docs/superpowers/plans/2026-04-17-rtc-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/rtc_checks.md`.
```

- [ ] **Step 3: Commit**

```bash
# Run from repo root, NOT firmware/
git add TODO.md
git commit -m "docs(todo): mark rtc module complete"
```

---

## Done criteria

- [ ] All 7 tasks complete with per-task commits.
- [ ] `cd firmware && pio test -e native` reports 140/140 pass.
- [ ] `cd firmware && pio run -e emory` and `pio run -e nora` both
      succeed.
- [ ] Hardware checks #1 and #2 (pre-flight charging-resistor removal
      + I²C probe) pass on the breadboard before declaring the
      module shippable. Checks #3-#8 are run when parts and schedule
      allow but are NOT blocking for the module's native-side ship.
- [ ] `TODO.md` rtc row flipped to `[x]`.
