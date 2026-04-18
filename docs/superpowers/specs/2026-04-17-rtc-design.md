# RTC Firmware Module — Design Spec

Date: 2026-04-17

## Overview

The `rtc` module owns the DS3231 hardware read/write path and projects
UTC fields to local time via libc. It is mostly an RTClib wrapper: a
thin ESP32 adapter plus a small pure-logic layer that handles the
button-advance wrap math (hour wrap, minute wrap with add-5 / floor-5
and hour carry). `wifi_provision` owns the POSIX TZ string and the
`setenv/tzset` call; `display` consumes the local fields `rtc::now()`
produces.

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
(rtc listed as a Phase 2 firmware module in TODO.md; parent §Time sync).

Reference implementation: `~/dev/personal/word-clock/word-clock/clock.{h,cpp}`
(the Arduino Mega word-clock's RTC adapter). The daughters' rtc module
mirrors its API shape (minus 12h conversion) and wrap semantics with
two material departures driven by Phase 2 context on this project:

1. **UTC on DS3231, not local.** `wifi_provision` already calls
   `setenv/tzset`; libc handles the local projection. Storing UTC on
   the chip means DST transitions don't require an NTP round-trip to
   correct, and a TZ change (daughter relocates, runs reset-to-captive,
   picks a new zone) takes effect on the next read — no RTC write
   needed.
2. **Pure-logic split for wrap math.** The daughters' template
   (display / buttons / wifi_provision) extracts pure C++ into a
   native-testable layer. rtc follows suit — one extra `.h` + `.cpp`
   + test file vs. the Mega reference's flat adapter, and the
   advance-wrap edge cases become unit-testable on macOS.

## Scope

**In:**
- `wc::rtc::begin()` — initialize I²C (SDA=21, SCL=22) and the DS3231
  via RTClib.
- `wc::rtc::now()` — read current UTC fields from chip, project to
  local via `localtime_r()` respecting the POSIX TZ set by
  `wifi_provision`, return `DateTime` (local fields).
- `wc::rtc::set_from_epoch(uint32_t)` — NTP writeback. Argument is
  UTC seconds since 1970-01-01 (what `NTPClient` returns directly).
- `wc::rtc::advance_hour()` / `wc::rtc::advance_minute()` — button
  read-modify-write via the pure-logic wrap helpers.
- Pure-logic wrap math `advance_hour_fields(DateTime)` /
  `advance_minute_fields(DateTime)` — native-testable with Unity.
- Pure-logic `utc_epoch_from_fields(DateTime)` — UTC fields → UTC
  seconds-since-1970, using Howard Hinnant's `days_from_civil`
  algorithm (~15 LOC, no libc, thread-safe). Used on every `now()`
  call; avoids mutating global TZ state from a hot-path function.
  Native-testable.
- `#ifdef ARDUINO` guard on the ESP32 adapter — same pattern as
  `wifi_provision` / `buttons` / `display` adapters. PlatformIO LDF's
  `deep+` mode otherwise pulls library sources into the native build.

**Out:**
- **NTP fetch.** `ntp` module's job (lands next). rtc only accepts
  the epoch it's handed.
- **Last-sync tracking.** Already lives in `wifi_provision`
  (`nvs_store::touch_last_sync` / `seconds_since_last_sync` at
  `firmware/lib/wifi_provision/src/nvs_store.cpp:96`). rtc does not
  duplicate it.
- **POSIX TZ string storage + `setenv/tzset`.** `wifi_provision` owns
  it (NVS + `start_sta()` at `firmware/lib/wifi_provision/src/wifi_provision.cpp:140`).
- **DS3231 battery-charging-resistor removal.** Physical hardware
  task (ZS-042 module ships with a 200Ω + 1N4148 trickle-charge
  circuit unsafe for non-rechargeable CR2032; see
  `docs/hardware/pinout.md` §Critical issues #1). Called out in the
  manual hardware checklist as a pre-flight check, not a firmware
  concern.
- **DS3231 alarms, SRAM usage, temperature register reads.** Not
  used; RTClib's default `RTC_DS3231` suffices.
- **Drift correction / calibration.** DS3231 is accurate to ~2 ppm
  (seconds per week); NTP every 6 h dwarfs any systematic drift.
- **Date carry on button advance.** Hour 23 → hour 0 does NOT advance
  day; minute 59 → 0 with hour carry, but hour carry does NOT cascade
  to day. QLOCKTWO convention, matches Mega reference.
- **12h / AM-PM conversion.** `time_to_words()` takes 0-23; RTClib's
  `hour()` returns 0-23 directly. Mega's `ClockTime::isPM` was needed
  because its `timeToWords` took 12h input — not applicable here.

## Behavioral Contract

### DateTime fields

All fields returned by `now()` are **local time** — the POSIX TZ rule
active at call time (via `tzset()`) is applied. Callers (display,
time_to_words) never see UTC.

```cpp
struct DateTime {
    uint16_t year;    // full year, e.g. 2030
    uint8_t  month;   // 1-12
    uint8_t  day;     // 1-31
    uint8_t  hour;    // 0-23 (local)
    uint8_t  minute;  // 0-59
    uint8_t  second;  // 0-59
};
```

### Read path: `now()`

1. Adapter calls `RTClib::RTC_DS3231::now()` → UTC fields from chip.
2. Convert UTC fields to a `time_t` UTC epoch via the pure-logic
   helper `utc_epoch_from_fields()` (Howard Hinnant's
   `days_from_civil` algorithm). **No TZ manipulation, no libc
   `mktime`/`timegm` dependency, no global state mutation.**
3. `localtime_r(&utc_epoch, &local_tm)` populates `local_tm` with
   TZ-adjusted fields, respecting the DST rule in the POSIX TZ string.
   `localtime_r` reads the `tz` state but does not mutate it, so it's
   safe to call concurrently with WiFi-task time calls.
4. Pack `local_tm` fields into `wc::rtc::DateTime` and return.

**Thread-safety rationale.** ESP32 Arduino runs on FreeRTOS with
the WiFi stack on its own task. `rtc::now()` runs many times per
second during display render. An earlier draft converted UTC fields
to an epoch via `setenv("TZ","UTC0"); mktime(); restore` — that
idiom mutates a process-global env var that the WiFi task reads via
`localtime()` / `time()`, creating a microsecond race window on
every read. The pure-arithmetic `utc_epoch_from_fields()` eliminates
the race entirely. The write path (`advance_*`) still uses `mktime`
(with `tm_isdst=-1`) but is called only on button press — orders of
magnitude rarer — and `mktime` only *reads* the TZ state, so no
mutation occurs on the hot path or on the button path.

**Sequencing constraint (load-bearing):** `wc::rtc::begin()` MUST run
AFTER `wc::wifi_provision::begin()` in `setup()`.
`wifi_provision::start_sta()` is where
`setenv("TZ", posix, 1); tzset();` runs on warm boot
(`firmware/lib/wifi_provision/src/wifi_provision.cpp:140`). Without
that, `localtime_r` uses libc's default TZ (UTC) and `now()` silently
returns UTC fields — a "clock is off by N hours with no log line"
failure mode. `begin()` emits a `Serial.println` soft warning if
`getenv("TZ") == nullptr` so bring-up catches misordering fast.

Cold boot into captive portal (no stored creds) legitimately has no
TZ set until the user submits the form — the soft warning fires but
is expected in that state. No consumer reads `rtc::now()` during
captive-portal states — `main.cpp` renders the display only when
`wifi_provision::state() == Online` (see `firmware/src/main.cpp:48`).

### Write path: `set_from_epoch(unix_seconds)`

- `unix_seconds` is UTC seconds since 1970-01-01 (what `NTPClient`
  returns directly).
- Adapter writes UTC fields to DS3231 via
  `rtc.adjust(DateTime(unix_seconds))` — RTClib's `DateTime(uint32_t)`
  constructor handles the epoch-to-UTC-fields split internally.
- **No last-sync side effect.** Caller (ntp module) invokes
  `wifi_provision::nvs_store::touch_last_sync(unix_seconds)`
  separately — two responsibilities, two calls.
- **No range validation.** ntp module is responsible for filtering
  NTP server responses before calling (reject epoch 0, reject
  obviously-bogus pre-2023 values).
- **`uint32_t` overflow in 2106.** Daughters' clocks age out around
  2070-2100 per the parent spec; RTClib is `uint32_t`-based at its
  own layer, so widening rtc's API wouldn't help. Accepted.

### Advance: `advance_hour()`

Read-modify-write through the pure-logic helper:

1. Adapter calls `now()` (returns local fields).
2. Call `advance_hour_fields(local)`:
   - `out.hour = (in.hour + 1) % 24`
   - All other fields (year, month, day, minute, second) preserved.
   - **No date carry.** Hour 23 → hour 0 on the same date.
3. Convert the advanced local fields back to a UTC epoch via
   `mktime()` with `tm_isdst = -1` — libc uses the current TZ + DST
   rule to determine the offset, so the round-trip is DST-safe.
4. Write UTC fields to DS3231 via `rtc.adjust(DateTime(new_utc_epoch))`.

Rationale for no date carry: QLOCKTWO convention + Mega reference. A
user pressing Hour is setting the time, not advancing the calendar.
Pressing Hour 24 times restores the same display.

### Advance: `advance_minute()`

1. Read local via `now()`.
2. Call `advance_minute_fields(local)`:
   - `raw = in.minute + 5`
   - If `raw >= 60`: `raw -= 60; hour = (in.hour + 1) % 24` **(carry
     to hour, no cascade to day)**
   - `out.minute = (raw / 5) * 5` **(floor to 5-min block)**
   - `out.second = 0` **(reset)**
   - All other fields preserved.
3. Convert local → UTC via `mktime` as above, write to DS3231.

**Rationale for "add 5, floor to 5" (not raw add-5):**
`time_to_words()` floors to 5-min blocks internally
(`firmware/lib/core/include/time_to_words.h:16`). If a user at :23
presses Minute, they see :25 (the next visible 5-min block), not :28
(which rounds back to :25 for display — making the button feel dead
for 2 of every 5 presses). Matches Mega's `clockAdvanceMinute5`.

**Rationale for hour carry:** at :55, pressing Minute should produce
:00 + hour+1, not :00 + same hour (visually un-advances an hour from
the user's perspective). Departs from the "QLOCKTWO in-place wrap"
alternative in the parent task prompt, matching Mega instead.

**Rationale for no day carry:** 23:55 + Minute → 00:00 same date.
User is setting time, not date — consistent with `advance_hour`.

**Rationale for `second = 0`:** matches Mega. Zeroing on advance
gives consistent block alignment across back-to-back presses
(otherwise two presses at different real-time offsets could land on
different 5-min blocks).

### DST transitions

- POSIX TZ string (e.g., `PST8PDT,M3.2.0,M11.1.0`) encodes the DST
  transition rule statically.
- `tzset()` applies the rule once; every subsequent `localtime_r`
  call uses the current-DST offset.
- `rtc::now()` is correct across a DST transition the moment the
  transition fires — no NTP round-trip required, no WiFi required.
- `set_from_epoch` accepts UTC regardless of local DST state.

### TZ change (user resets and picks a different zone)

- Hour + Audio held 10 s → `wifi_provision::reset_to_captive()` →
  NVS clear → `ESP.restart()`.
- On next boot, the new TZ string is read from NVS by `wifi_provision`
  and passed to `setenv/tzset` during `start_sta()`.
- `rtc::now()` picks up the new TZ on the very next read — no DS3231
  write needed. (Chip holds UTC; only the projection changes.)

### Failure modes

| Scenario | Behavior |
|---|---|
| DS3231 not found on I²C at `begin()` | Logs `[rtc] ERROR: DS3231 not found on I²C bus`. `begin()` still returns. `now()` returns whatever the RTClib library yields on a dead bus (implementation-defined, likely a zero / default `DateTime`). Display continues to render. Recovery: fix wiring, power cycle. |
| `rtc::begin()` called before `wifi_provision::begin()` on warm boot | `begin()` soft-warns via `Serial.println("[rtc] warning: TZ env var unset at begin()")`. `now()` would return UTC fields silently until `tzset` eventually runs. Main.cpp enforces correct ordering; warning is a bring-up safety net. |
| `rtc::now()` called before `tzset` | libc default TZ = UTC. `now()` returns UTC fields silently (off by ±0-14 h depending on user's real zone). Sequencing constraint prevents this on warm boot; cold-boot-into-captive-portal avoids it by not calling `now()` until Online. |
| `set_from_epoch(0)` | Writes 1970-01-01 00:00:00 UTC to chip. `now()` then returns (approximately) 1970 projected into the current TZ. Caller's problem to filter. |
| Battery dead + power cycle | DS3231 oscillator loses state. `rtc.now()` returns a default (often 2000-01-01 or the last written value depending on retention). Display renders what it gets. Next NTP sync (≤6 h on warm boot with WiFi) corrects it. |
| newlib `mktime` doesn't respect `TZ` env var for DST | Implementation-time risk **on the write path only** (advance_*). If `mktime` produces wrong UTC epochs from local fields after `tzset`, button-press writes are off. Read path (`now()`) is unaffected — it uses the pure `utc_epoch_from_fields`. Fallback: manual offset arithmetic keyed off the POSIX TZ string. Flagged in Open Issues. |

## Architecture

Hexagonal split, matching `display` / `buttons` / `wifi_provision`.
Pure-logic wrap math is native-testable; adapter is thin RTClib +
libc glue.

```
firmware/lib/rtc/
  include/
    rtc.h                    # ESP32-only public API
                             #   (begin, now, set_from_epoch,
                             #    advance_hour, advance_minute)
    rtc/
      date_time.h            # DateTime struct (header-only, pure)
      advance.h              # pure-logic advance_hour_fields /
                             #   advance_minute_fields
      epoch.h                # pure-logic utc_epoch_from_fields
                             #   (UTC fields → uint32_t seconds)
  src/
    rtc.cpp                  # ADAPTER — RTClib + localtime_r +
                             #   mktime (#ifdef ARDUINO)
    advance.cpp              # pure-logic wrap math
    epoch.cpp                # pure-logic days_from_civil arithmetic

firmware/test/
  test_rtc_advance/test_advance.cpp
  test_rtc_epoch/test_epoch.cpp
  hardware_checks/rtc_checks.md
```

Pure-logic `.cpp` files (`advance.cpp`, `epoch.cpp`) get added to
`[env:native]` `build_src_filter` in `platformio.ini`. `rtc.cpp` is
guarded with `#ifdef ARDUINO` for the same reason the sibling
adapters are.

### Namespace

`wc::rtc` for public API (consistent with `wc::display`,
`wc::buttons`, `wc::wifi_provision`). Pure-logic wrap helpers live in
the same namespace — not under `detail`. The `DateTime` struct is
shared between the pure layer and the adapter; no internal-only
types.

## Public API

### Header-only `DateTime`

```cpp
// firmware/lib/rtc/include/rtc/date_time.h
#pragma once

#include <cstdint>

namespace wc::rtc {

// Local wall-clock fields (output of now(), input to advance wrap
// math). UTC is an internal detail of the adapter and never escapes
// the rtc module's public API.
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

} // namespace wc::rtc
```

### Pure-logic wrap math

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

```cpp
// firmware/lib/rtc/include/rtc/epoch.h
#pragma once

#include <cstdint>
#include "rtc/date_time.h"

namespace wc::rtc {

// Caller guarantees valid input ranges (same as advance.h). Input
// fields MUST represent a UTC date-time; no TZ conversion happens
// here.
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

### ESP32-only public API

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
// returns UTC fields (libc default TZ is UTC).
//
// If the DS3231 is not present on the bus, logs an error and
// returns. now() will still return whatever the chip yields
// (typically the last-written value or a 2000-01-01 default on a
// dead battery) so the rest of the firmware keeps running.
void begin();

// Read current LOCAL wall-clock fields from the DS3231.
// Internally: read UTC fields from chip, convert to UTC epoch,
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
// See advance.h for wrap semantics.
void advance_hour();

// Button callback. Same round-trip as advance_hour, but with
// advance_minute_fields() in the middle.
void advance_minute();

} // namespace wc::rtc
```

## Adapter (`rtc.cpp`) — pseudocode

Shape for review; actual code written during implementation.

```cpp
#ifdef ARDUINO

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
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

Two TZ-adjacent operations across the adapter:

- **Read path (`now()`)** uses the pure-arithmetic `utc_epoch_from_fields` for the fields→epoch step, then `localtime_r` for the projection. `localtime_r` reads-but-doesn't-mutate the TZ state, so no race.
- **Write path (`advance_*`)** uses `mktime` with `tm_isdst = -1`. `mktime` also reads-but-doesn't-mutate TZ. The only TZ *mutation* in the whole firmware is `wifi_provision::start_sta()`'s `setenv/tzset` pair, which runs only on main-loop state transitions — not concurrently with anything rtc does.

`set_from_epoch` writes UTC directly via RTClib's epoch constructor with no conversion.

## Integration

### main.cpp — replaces today's hardcoded 14:23 scaffold

Current `firmware/src/main.cpp` paints a hardcoded `14:23 on May 15
2027` frame whenever `wifi_provision::state() == Online`. Once rtc
lands, this becomes:

```cpp
#include "rtc.h"
// ...

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();   // runs setenv/tzset on warm boot
    wc::rtc::begin();              // AFTER wifi_provision — load-bearing
    wc::display::begin();

    wc::buttons::begin([](wc::buttons::Event e) {
        using BE = wc::buttons::Event;
        using WS = wc::wifi_provision::State;
        switch (e) {
            case BE::HourTick:    wc::rtc::advance_hour();   break;
            case BE::MinuteTick:  wc::rtc::advance_minute(); break;
            case BE::AudioPressed:
                if (wc::wifi_provision::state() == WS::AwaitingConfirmation)
                    wc::wifi_provision::confirm_audio();
                // else audio module (not yet wired)
                break;
            case BE::ResetCombo:
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
        wc::display::RenderInput in{
            .year   = dt.year,
            .month  = dt.month,
            .day    = dt.day,
            .hour   = dt.hour,
            .minute = dt.minute,
            .now_ms = millis(),
            .seconds_since_sync = wc::wifi_provision::seconds_since_last_sync(),
            .birthday = {CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                         CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE},
        };
        wc::display::show(wc::display::render(in));
    } else {
        wc::display::show(wc::display::Frame{});
    }
    delay(1);
}
```

### ntp module contract (future)

Not implemented here. The `ntp` module — scheduled next in TODO.md
— will:

1. On transition to `Online` and every 24 hours (±30 min jitter)
   thereafter, fetch UTC
   from an NTP server via `NTPClient` (already in
   `[esp32-base] lib_deps` as `arduino-libraries/NTPClient@^3.2.1`).
2. Range-validate the result (reject epoch 0, reject epoch < the
   compile-time build timestamp or some plausible floor like
   2023-01-01 = 1672531200).
3. Call `wc::rtc::set_from_epoch(utc_seconds)`.
4. Call `wc::wifi_provision::nvs_store::touch_last_sync(utc_seconds)`.

rtc exposes no other contract to ntp. rtc doesn't know last-sync
exists.

### Dependency on wifi_provision

- **Sequencing:** `rtc::begin()` runs after `wifi_provision::begin()`
  to ensure `tzset()` has been invoked on warm boot. main.cpp
  enforces this; rtc doesn't defend against misordering at runtime.
- **No runtime calls.** rtc doesn't invoke anything in
  `wifi_provision`. TZ is communicated via the global libc `TZ` env
  var, which `wifi_provision` owns the writes to.

### pinmap.h

`PIN_I2C_SDA = 21` and `PIN_I2C_SCL = 22` are already defined in
`firmware/configs/pinmap.h` (shipped with the buttons module). No
additions needed.

### platformio.ini

RTClib is already declared in `[esp32-base] lib_deps`
(`adafruit/RTClib@^2.1.4`). Two edits:

- Add `-I lib/rtc/include` to `[env:native] build_flags`.
- Add `+<../lib/rtc/src/advance.cpp>` and
  `+<../lib/rtc/src/epoch.cpp>` to `[env:native] build_src_filter`.

`rtc.cpp` is NOT added to the native filter — `#ifdef ARDUINO`
short-circuits it on that target.

## Testing

### Pure-logic native tests (Unity)

**`test_rtc_advance`** — 11 tests.

advance_hour_fields (5):

- `test_advance_hour_increments_by_one`:
  `advance_hour_fields({2030, 5, 15, 14, 23, 45})` returns
  `{2030, 5, 15, 15, 23, 45}`. Confirms the non-hour fields are
  preserved exactly.
- `test_advance_hour_wraps_23_to_0`:
  `advance_hour_fields({2030, 5, 15, 23, 23, 45})` returns
  `{2030, 5, 15, 0, 23, 45}`. Day is still 15.
- `test_advance_hour_no_day_carry_on_new_years_eve`:
  `advance_hour_fields({2030, 12, 31, 23, 23, 45})` returns
  `{2030, 12, 31, 0, 23, 45}`. Day / month / year unchanged at the
  calendar boundary.
- `test_advance_hour_preserves_second_at_zero`:
  `advance_hour_fields({2030, 5, 15, 14, 23, 0})` returns
  `{2030, 5, 15, 15, 23, 0}`. Boundary of the second field.
- `test_advance_hour_preserves_second_at_59`:
  `advance_hour_fields({2030, 5, 15, 14, 23, 59})` returns
  `{2030, 5, 15, 15, 23, 59}`. Opposite boundary; confirms no
  accidental reset of second on hour advance (only minute advance
  zeros it per the behavioral contract).

advance_minute_fields (6):

- `test_advance_minute_adds_5_from_aligned`:
  `advance_minute_fields({2030, 5, 15, 14, 25, 45})` returns
  `{2030, 5, 15, 14, 30, 0}`. 25 → 30. Second zeroed.
- `test_advance_minute_adds_5_then_floors`:
  `advance_minute_fields({2030, 5, 15, 14, 23, 45})` returns
  `{2030, 5, 15, 14, 25, 0}`. 23 + 5 = 28 → floor to 25.
  Confirms add-then-floor, not raw add-5.
- `test_advance_minute_at_55_carries_to_hour`:
  `advance_minute_fields({2030, 5, 15, 14, 55, 45})` returns
  `{2030, 5, 15, 15, 0, 0}`. Hour carry, minute wrap, second zeroed.
- `test_advance_minute_at_23_55_wraps_hour_and_stays_on_same_day`:
  `advance_minute_fields({2030, 5, 15, 23, 55, 45})` returns
  `{2030, 5, 15, 0, 0, 0}`. Both wraps fire; day unchanged.
- `test_advance_minute_at_57_carries_past_60_and_floors`:
  `advance_minute_fields({2030, 5, 15, 14, 57, 45})` returns
  `{2030, 5, 15, 15, 0, 0}`. 57 + 5 = 62, minus 60 = 2, floor to 0.
  Edge-of-carry boundary check.
- `test_advance_minute_preserves_year_month_day_at_calendar_edge`:
  `advance_minute_fields({2030, 12, 31, 14, 25, 45})` returns
  `{2030, 12, 31, 14, 30, 0}`. Calendar fields untouched.

**`test_rtc_epoch`** — 5 tests.

- `test_utc_epoch_from_fields_at_unix_zero`:
  `utc_epoch_from_fields({1970, 1, 1, 0, 0, 0})` returns `0u`.
- `test_utc_epoch_from_fields_at_2000_01_01`:
  `utc_epoch_from_fields({2000, 1, 1, 0, 0, 0})` returns
  `946'684'800u`. (Independently verifiable via `date -u -d
  '2000-01-01 00:00:00' +%s`.) Pins the algorithm against a known
  reference point RTClib also uses internally.
- `test_utc_epoch_from_fields_across_leap_day`:
  `utc_epoch_from_fields({2028, 3, 1, 0, 0, 0}) -
   utc_epoch_from_fields({2028, 2, 29, 0, 0, 0})` returns
  `86'400u`. 2028 is a leap year; confirms February 29 is accounted
  for.
- `test_utc_epoch_from_fields_at_2030_10_06_18_10_00`:
  Emory's birth minute (local — treated as UTC here for the math
  test). `utc_epoch_from_fields({2030, 10, 6, 18, 10, 0})` returns
  `1'917'720'600u`. Confirms hour/minute/second contribute
  3600/60/1 seconds respectively.
- `test_utc_epoch_from_fields_round_trips_through_gmtime`:
  Pick 10 arbitrary UTC DateTimes from 2026-2100. For each, assert
  `utc_epoch_from_fields(dt)` returns the same value as the host's
  libc `timegm(&tm)` where `tm` is the struct-tm form of `dt`.
  Cross-check: native test runner has real `timegm`; this catches
  any arithmetic bug against libc's own implementation.

Total rtc pure-logic tests: **16** (11 advance + 5 epoch).

Target native-suite count after this module: 124 (post-display) +
16 (rtc) = **140 native tests**.

### ESP32 adapter — NOT native-testable

Three things the native suite can't exercise:

- RTClib I²C round-trip (requires real DS3231).
- `localtime_r` / `mktime` interaction with the ESP32 newlib's
  handling of the POSIX TZ + DST rule (works on host libc too, but
  asserting specific behavior is brittle across host-vs-target libc
  implementations — the actual risk is an ESP32-toolchain quirk).
- DS3231 battery-backed retention across power cycles.

These are covered by the hardware manual-check document below.

### Hardware manual check: `firmware/test/hardware_checks/rtc_checks.md`

Executed during breadboard bring-up per
`docs/hardware/breadboard-bring-up-guide.md` §Step 6 (DS3231 on the
I²C bus). Not automatable.

1. **Charging-resistor removal (pre-flight).** Before inserting a
   CR2032 into the ZS-042 module, confirm the 200Ω + 1N4148
   trickle-charge circuit has been removed per
   `docs/hardware/pinout.md` §Critical issues #1. A non-rechargeable
   CR2032 on a charging circuit can leak, vent, or explode.
   Visual-inspect the spot near the battery holder; verify no 200Ω
   resistor remains in series with battery+.
2. **I²C probe.** With DS3231 wired (SDA=21, SCL=22, VCC=3.3V, GND),
   flash `pio run -e emory -t upload`. Serial at 115200. Boot log
   should NOT show `[rtc] ERROR: DS3231 not found on I²C bus`.
   Confirms RTClib `begin()` found address `0x68`.
3. **Round-trip read/write.** Temporarily patch `setup()` to call
   `wc::rtc::set_from_epoch(<a known UTC epoch>)` once, then log
   `wc::rtc::now()` every second. Verify:
   - First log line matches the written fields projected through
     the current TZ (e.g., wrote 2026-04-17 22:00:00 UTC, TZ=Pacific
     PDT, expect `{2026, 4, 17, 15, 0, 0}`).
   - Subsequent log lines increment `second` by 1 per tick.
   - After 60 s: `minute` increments, `second` rolls to 0.
4. **Button advance — hour.** Remove the sync patch. Set a known
   time via a temporary `set_from_epoch` call, then press the Hour
   button once. Confirm serial shows `now()` hour incremented by 1;
   minute + date preserved. Press 24 times total; confirm hour
   returned to its initial value and date is unchanged.
5. **Button advance — minute.** Set time to 14:25:00. Press Minute
   once. Confirm `now()` reports 14:30:00 (second zeroed). Advance to
   14:55 (via repeated Minute presses). Press Minute. Confirm
   15:00:00 — hour carried, second zeroed, date unchanged.
6. **Battery-backed retention.** Set a known time. Unplug USB. Wait
   **5 minutes** (short-duration outages can be carried by the
   DS3231's onboard decoupling capacitance alone — the extended
   wait forces the CR2032 to actually source current). Plug USB.
   Confirm `now()` resumes from approximately the unplug time + 5
   minutes (NOT 1970-01-01 or 2000-01-01). Confirms CR2032 is
   providing backup and the oscillator survived the outage.
7. **NTP handoff (after ntp module ships).** Boot the clock. After
   the ntp module runs and calls `rtc::set_from_epoch(...)`, confirm
   `now()` matches wall-clock time in the user's current timezone.
   Confirms libc TZ application + adapter UTC↔local path end-to-end.
8. **DST transition (opportunistic).** Hard to test in real time.
   Temporarily set the chip's UTC to a time ~5 minutes before the
   next US DST transition (using #7's `set_from_epoch` patch).
   Observe the local fields report correctly on either side of the
   transition without any further writes. Skip if not near a
   transition date during bring-up.

## Open issues

One implementation-time verification — not a design open item:

- **newlib `mktime` + `tzset` DST behavior on ESP32 (write path
  only).** `local_fields_to_utc_epoch` relies on `mktime(&t)`
  respecting the POSIX DST rule in the `TZ` env var when
  `tm_isdst = -1`. Standard POSIX behavior; newlib claims to
  implement it. Verification is a 10-line bring-up sketch that
  `setenv("TZ","PST8PDT,M3.2.0,M11.1.0"); tzset();` and compares
  `mktime` output against a hand-computed UTC epoch across a DST
  transition. If it misbehaves, fallback is hand-rolled offset
  math keyed off the POSIX TZ string — still workable, more code.
  The read path (`now()`) is independent of this check because it
  uses the pure `utc_epoch_from_fields`.

All 5 brainstorming decisions are locked.

## Verified numbers

| Item | Value | Source |
|---|---|---|
| DS3231 I²C address | `0x68` | DS3231 datasheet (fixed, not configurable) |
| I²C SDA pin | GPIO 21 | `docs/hardware/pinout.md` §Signal assignments + `firmware/configs/pinmap.h` |
| I²C SCL pin | GPIO 22 | `docs/hardware/pinout.md` §Signal assignments + `firmware/configs/pinmap.h` |
| I²C pullup | 4.7 kΩ on DS3231 module (per-unit verify) | `docs/hardware/pinout.md` §Power (stacking caveat) |
| DS3231 drift rating | ±2 ppm, ~seconds per week | DS3231 datasheet + parent spec §Time sync |
| RTClib version | 2.1.4 | `firmware/platformio.ini` `[esp32-base] lib_deps` (`adafruit/RTClib@^2.1.4`) |
| ZS-042 charging resistor | 200 Ω + 1N4148, must be removed before CR2032 insertion | `docs/hardware/pinout.md` §Critical issues #1 |
| Hour-advance wrap | `(hour + 1) % 24`, no date carry | Mega reference `clockAdvanceHour` + QLOCKTWO convention |
| Minute-advance behavior | Add 5, floor to 5-block, carry to hour at 60, no date carry, second → 0 | Mega reference `clockAdvanceMinute5` |
| POSIX TZ string source | 8-entry table in `wifi_provision` | `docs/superpowers/specs/2026-04-16-captive-portal-design.md` §Timezone options |
| TZ application path | `wifi_provision::start_sta()` runs `setenv("TZ", posix); tzset()` → libc `localtime_r` in `rtc::now()` | `firmware/lib/wifi_provision/src/wifi_provision.cpp:140` |
| DST rule | Encoded in POSIX TZ string; libc applies via `tzset` + `localtime_r` | POSIX TZ format spec |
| NTP sync cadence | On boot + every 24 hours (±30 min jitter) | `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §Time sync |
| `time_to_words` minute resolution | 5-min blocks (internal floor) | `firmware/lib/core/include/time_to_words.h:16` |
| NTPClient version | 3.2.1 | `firmware/platformio.ini` `[esp32-base] lib_deps` |

## References

- Parent design: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
  (§Time sync, §Firmware Behaviors)
- Hardware pinout: `docs/hardware/pinout.md`
  (§Signal assignments, §Critical issues #1, §Pre-power smoke test
  steps 3 + 6)
- Sibling modules (template pattern + hexagonal-split examples):
  - `docs/superpowers/specs/2026-04-17-display-design.md`
    (consumes `rtc::now()` fields)
  - `docs/superpowers/specs/2026-04-16-buttons-design.md`
    (fires `HourTick` / `MinuteTick` → `advance_hour/minute`)
  - `docs/superpowers/specs/2026-04-16-captive-portal-design.md`
    (owns POSIX TZ string + `setenv/tzset`)
  - `firmware/lib/display/`, `firmware/lib/buttons/`,
    `firmware/lib/wifi_provision/`
- Reference implementation: `~/dev/personal/word-clock/word-clock/clock.{h,cpp}`
  (Arduino Mega word-clock; wrap semantics + flat-adapter pattern)
- RTClib 2.1.4: https://github.com/adafruit/RTClib
- Breadboard bring-up (manual hardware-check prerequisite):
  `docs/hardware/breadboard-bring-up-guide.md` §Step 6
  (DS3231 I²C probe)
