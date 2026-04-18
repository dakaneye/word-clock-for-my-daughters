# NTP Firmware Module — Design Spec

Date: 2026-04-17

## Overview

The `ntp` module owns the WiFi-side time-sync path. On a 24-hour
nominal cadence (with ±30 min jitter), it queries `time.google.com`
via the Arduino `NTPClient` library, validates the result, and
writes UTC seconds to the DS3231 via `wc::rtc::set_from_epoch()`. It
also notifies `wifi_provision` of the success via `touch_last_sync()`,
which the display module reads to drive its amber stale-sync tint at
>24 h.

`ntp` is an observer of `wc::wifi_provision::state()` — when the
state is not `Online`, the module sleeps. It does not register
callbacks, does not own WiFi state, and does not own the DS3231; it
is a thin scheduler that translates "WiFi is up" into "DS3231 is
correct."

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
(ntp listed as a Phase 2 firmware module in TODO.md; parent §Time sync).

Sibling pattern: `wc::rtc` (`docs/superpowers/specs/2026-04-17-rtc-design.md`).
The pure-logic split + `#ifdef ARDUINO` adapter shape mirrors rtc /
buttons / display / wifi_provision.

## Scope

**In:**

- `wc::ntp::begin()` — open the NTPClient's UDP socket once, register
  hostname `time.google.com`. No network I/O at boot.
- `wc::ntp::loop()` — single function called from `main.cpp`'s
  `loop()`. Polls `wifi_provision::state()`; when `Online`, runs the
  pure-logic scheduler; on a sync trigger, calls
  `NTPClient::forceUpdate()` (bypassing the library's internal gate),
  validates, and on success calls
  `wc::rtc::set_from_epoch(epoch)` + `wc::wifi_provision::touch_last_sync(epoch)`.
- Pure-logic `is_plausible_epoch(unix_epoch)` — range floor only
  (≥ 2026-01-01). No upper bound (Y2106 wrap is past device
  lifetime; see Behavioral Contract §NTP era handling).
  Native-testable.
- Pure-logic `next_backoff_ms(consecutive_failures)` — capped
  exponential curve: 30 s → 1 m → 2 m → 4 m → 8 m → cap at 30 m.
  Native-testable.
- Pure-logic `next_deadline_after_success(success_ms, jitter_sample)`
  — returns `success_ms + 24 h + uniform(-30 m, +30 m)`.
  Native-testable.
- `#ifdef ARDUINO` guard on `ntp.cpp` adapter — same pattern as rtc /
  buttons / display.

**Out:**

- **WiFi state management.** `wifi_provision` owns it; ntp observes.
- **POSIX TZ string + `tzset()`.** `wifi_provision` owns it.
- **Last-sync NVS write.** `wifi_provision::touch_last_sync()` does
  the NVS work (`firmware/lib/wifi_provision/src/nvs_store.cpp:96`).
  ntp just calls it. (Public-header exposure called out in
  Integration below.)
- **Last-sync read / display tint.** `display` already reads
  `wifi_provision::seconds_since_last_sync()`. ntp does not duplicate.
- **Multi-server failover / fallback NTP host.** Single server
  (`time.google.com`). On chronic failure, the display's amber tint
  surfaces it; that's the right escalation. (Researched + dropped:
  see "Decisions deliberately deferred" below.)
- **Circuit breaker as a separate concept.** The 30-min backoff cap
  *is* the circuit breaker — at the cap we attempt twice per hour,
  comfortably inside NTP Pool's 30-min ToS floor (which doesn't apply
  to Google NTP, but is a useful lower bound).
- **Sub-second accuracy / NTP filtering algorithms.** Display
  resolution is 5 minutes; we accept whatever single-shot UDP
  round-trip gives us.
- **NTP era 2 (year 2172).** Past device lifetime by ~70 years.
- **Alternative transport (NTS over TCP/4460).** ISP UDP/123
  filtering on US residential is rare; if it happens, the right
  mitigation is a router-level fix, not a client-side library swap.
- **First-boot bootstrap delay.** Cold-boot with no time set wants a
  fast first sync, not a randomized delay.

## Decisions deliberately deferred

These were considered and dropped during brainstorming. Recorded so
future-us doesn't relitigate without context:

- **Fallback server `time.cloudflare.com`.** Adds list iteration +
  second hostname resolution + leap-smear-mismatch risk if mixed
  with non-smeared servers. Single server is simpler; chronic
  failure already surfaces via display's amber tint at >24 h.
- **0–60 s random boot delay before first sync.** Two units in one
  house is not a herd. The ±30 min jitter on subsequent syncs
  de-correlates effectively. Cold-boot with bad RTC time wants the
  *opposite* — sync ASAP.
- **Library swap to ESP-IDF SNTP.** Defensible for a multi-server
  failover story, but we don't need failover (single-server +
  display amber tint covers chronic failure). NTPClient is already
  in lib_deps and its uint32 modular arithmetic naturally bridges
  the Y2036 era boundary (see Behavioral Contract §NTP era
  handling).
- **Pure-logic Y2036 era disambiguation against the DS3231.**
  Initially specced as a wrapper around NTPClient with a
  `disambiguate_era(raw, reference) → unix_epoch` function and a
  `COMPILE_TIME_EPOCH` build-time constant fallback. Self-review
  caught that it's unnecessary: `NTPClient`'s `unsigned long
  secsSince1900 - SEVENZYYEARS` is a uint32 modular subtraction
  that returns the correct unix epoch for any date in 1970-2106
  via natural arithmetic wrap. Verified against
  `firmware/.pio/libdeps/nora/NTPClient/NTPClient.cpp:115`.
- **Circuit-breaker state machine.** Backoff cap ≡ open circuit.
  Adding a second state machine on top duplicates the same behavior.

## Behavioral Contract

### Cadence

- **Nominal:** every 24 hours after a successful sync, ± uniform(-30
  min, +30 min) jitter.
- **First-ever sync (cold boot, no successful sync since flash):**
  attempted as soon as `wifi_provision::state() == Online`. No
  jitter, no boot delay. Rationale: DS3231 might be wildly wrong on
  fresh hardware (no battery yet, or dead battery, or never set);
  the user should see correct words within seconds of WiFi
  connecting.
- **First sync after warm boot (NVS shows a prior successful
  sync):** scheduled per the nominal 24h-+jitter rule, computed
  from `seconds_since_last_sync()`. If we're already overdue (which
  is common after a long power-off), the sync fires immediately on
  the next `Online` tick.

### Retry on failure

- **Backoff curve:** 30 s → 1 m → 2 m → 4 m → 8 m → cap 30 m.
- **Reset to nominal on success:** `consecutive_failures = 0`,
  schedule next attempt 24h ± 30min from success.
- **WiFi drop during backoff:** scheduler keeps `consecutive_failures`
  state across `Online` → `not Online` → `Online` transitions.
  Rationale: a WiFi drop is itself a failure mode; restarting from
  the 30 s floor on every reconnect would amount to a faster retry
  curve than intended.
- **No upper limit on `consecutive_failures` count.** Counter is
  `uint32_t`; if we somehow accumulate 2^32 failures (~136 years at
  30-min cap), we have bigger problems. Cap behavior holds
  indefinitely.

### NTP era handling (Y2036 → Y2106)

NTP packets carry a 32-bit "seconds since 1900" field. It rolls over
at NTP timestamp 2^32 = Feb 7 2036 06:28:16 UTC ("era 0" → "era 1").
This is inside the daughters' clock lifetime — the girls would be
about 6 and 4 years old. So this matters.

The good news: `NTPClient@3.2.1` is naturally Y2036-safe through
2106 because of how it arithmetics. From
`firmware/.pio/libdeps/nora/NTPClient/NTPClient.cpp:113-115`:

```cpp
unsigned long secsSince1900 = highWord << 16 | lowWord;
this->_currentEpoc = secsSince1900 - SEVENZYYEARS;
```

`unsigned long` on ESP32 is uint32. The subtraction is uint32
modular arithmetic, which gives correct unix epochs across the
era 0 → era 1 boundary:

| NTP raw (`secsSince1900`) | `raw - SEVENZYYEARS` mod 2^32 | Real date — given device lifetime 2026-2070ish |
|---:|---:|---|
| ~3'970'000'000 (era 0, 2026) | ~1'761'000'000 | 2026 — straightforward era 0 |
| 2^32 - 1 (era 0 max) | 2'085'978'495 | 2036-02-07 06:28:15 UTC — last second of era 0 |
| 0 (era 1 start) | 2'085'978'496 | 2036-02-07 06:28:16 UTC — first second of era 1 |
| ~1'070'000'000 (era 1, 2070) | ~3'156'000'000 | 2070 — uint32 modular add gives correct unix epoch |
| 2'208'988'800 (era 1, 2106) | 0 | 2106-02-07 06:28:16 UTC — uint32 unix epoch wraps to 0; rejected by `is_plausible_epoch` (< MIN_PLAUSIBLE_EPOCH) |

The uint32 modular subtraction is monotonically correct from
1970-01-01 through 2106-02-07 06:28:15 UTC. At the Y2106 instant,
uint32 unix wraps to 0 and gets rejected by validation as
implausible — which surfaces as a sync failure (treated like any
other transient failure: backoff, retry). Daughters would be 76/74;
device may or may not still be running. Out of scope.

Downstream consumers must keep the value as `uint32_t`. We pass to
`wc::rtc::set_from_epoch(uint32_t)` which delegates to
`RTClib::DateTime(uint32_t)` — both uint32-clean. The risk we DO
inherit is Y2038 from libc `time_t` if it's `int32_t` on our ESP32
toolchain — but that lives in `wc::rtc::now()`'s `localtime_r` call,
not here. (Captured in rtc spec; not ntp's problem.)

Net: ntp does no era handling. It accepts whatever
`NTPClient::getEpochTime()` returns and validates it.

### Range validation

`is_plausible_epoch(unix_epoch)` returns `true` iff
`unix_epoch >= 1767225600` (2026-01-01 00:00:00 UTC).

- **Lower bound 2026-01-01:** the daughters' clocks were not built
  before this date; any sync result earlier is a packet error or
  uint32 wrap (Y2106 boundary).
- **No upper bound:** uint32 unix epoch maxes at 2106-02-07; any
  value in the uint32 range is plausible by date.

### NTPClient internal gate bypass

`NTPClient::update()` skips the actual network call if
`millis() - _lastUpdate < _updateInterval` (default 60 s, but we
don't care because we're calling `forceUpdate()` instead).
`forceUpdate()` always fires the UDP exchange and waits up to ~1 s.

We own the cadence in our pure-logic scheduler. Calling `update()`
would either (a) be redundant with our scheduler (when it doesn't
gate), or (b) silently no-op when our scheduler said it's time
(when it does gate). Both are wrong. We always call `forceUpdate()`.

### Dead-NTPClient pitfalls (worked around)

`NTPClient@3.2.1` has known issues (per arduino-libraries/NTPClient
GitHub issues #73, #164, #172):

- **DNS failure crash (#73).** Adapter checks
  `WiFi.status() == WL_CONNECTED` and `WiFi.hostByName("time.google.com",
  IPAddress&) == 1` before calling `forceUpdate()`. If hostname
  resolution fails, we treat it as a sync failure (increment
  consecutive_failures, schedule next backoff) without calling into
  the library.
- **Stale UDP buffer.** `forceUpdate()` flushes the buffer at the
  top, so this is not an issue on the path we take. We do NOT call
  `update()`, which has the bug.
- **Socket churn.** Adapter calls `NTPClient::begin()` exactly once
  in `wc::ntp::begin()`. Never calls `end()` / `begin()` mid-session.
- **Blocking `forceUpdate()`.** Up to ~1 s synchronous busy-wait.
  Acceptable: sync runs at most once per ~24 h on the happy path,
  and even at the 30-min cap, a 1 s blip every 30 minutes is
  invisible to the user (display continues to render the previous
  frame for 1 s — the words still match the time).

### Failure modes

| Scenario | Behavior |
|---|---|
| `wifi_provision::state() != Online` | `loop()` returns early. No timers, no allocations, no network I/O. |
| `WiFi.hostByName("time.google.com")` returns 0 | Treated as a sync failure. Increment `consecutive_failures`, schedule next attempt per backoff curve. Logged as `[ntp] DNS resolution failed`. |
| `NTPClient::forceUpdate()` returns false (timeout / packet loss) | Treated as a sync failure. Same handling as DNS. Logged as `[ntp] forceUpdate() failed`. |
| Sync returns implausible epoch (< 2026-01-01) | Discarded. Treated as a sync failure (the source is lying, the packet is corrupt, or we've reached the Y2106 wrap). Logged as `[ntp] implausible epoch <value>`. |
| Era 0 → era 1 boundary fires while clock is running | No-op. NTPClient's uint32 modular arithmetic naturally returns the post-boundary unix epoch on the next sync. DS3231 is unaffected (ticks BCD fields, not seconds-since-1900). |
| `wifi_provision::touch_last_sync()` fails (NVS write error) — or power dies between `set_from_epoch` and `touch_last_sync` | Logged. RTC has already been updated. Display tint will incorrectly show "stale" until the next successful sync writes NVS. Same failure class either way. Acceptable: visible nuisance, not a correctness bug. |

## Architecture

Hexagonal split, matching `display` / `buttons` / `wifi_provision` /
`rtc`. Pure-logic scheduler + era math is native-testable; adapter is
thin NTPClient + WiFi glue.

```
firmware/lib/ntp/
  include/
    ntp.h                    # ESP32-only public API (begin, loop)
    ntp/
      validation.h           # is_plausible_epoch + MIN_PLAUSIBLE_EPOCH
      schedule.h             # next_backoff_ms,
                             #   next_deadline_after_success
  src/
    ntp.cpp                  # ADAPTER — NTPClient + WiFi.hostByName
                             #   (#ifdef ARDUINO)
    validation.cpp           # pure-logic range validation
    schedule.cpp             # pure-logic cadence + backoff math

firmware/test/
  test_ntp_validation/test_validation.cpp
  test_ntp_schedule/test_schedule.cpp
```

Pure-logic `.cpp` files (`validation.cpp`, `schedule.cpp`) get added
to `[env:native]` `build_src_filter` in `platformio.ini`. `ntp.cpp`
is guarded with `#ifdef ARDUINO` for the LDF deep+ reason already
documented in sibling specs.

### Namespace

`wc::ntp` for public + pure helpers, consistent with sibling modules.

## Public API

### Pure-logic validation

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

### Pure-logic schedule

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

### ESP32-only public API

```cpp
// firmware/lib/ntp/include/ntp.h
//
// ESP32-only public API. Depends on Arduino.h, NTPClient, WiFi.
// Include ONLY from translation units that compile under the
// Arduino toolchain (the emory / nora PlatformIO envs). Native
// tests include the pure-logic headers under ntp/ — never this one.
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
// In main.cpp it's placed after rtc::begin() by convention, mirroring
// the rest of the setup() chain.
void begin();

// Drives the scheduler. Call from main.cpp's loop(). Polls
// wifi_provision::state(); when Online, checks the deadline; on
// hit, runs forceUpdate, validates, and on
// success calls rtc::set_from_epoch + wifi_provision::touch_last_sync.
//
// At most ~1 s blocking via NTPClient::forceUpdate() on sync
// attempts (~once per 24h on the happy path). All other ticks
// return in microseconds.
void loop();

} // namespace wc::ntp
```

## Adapter (`ntp.cpp`) — pseudocode

Shape for review; actual code written during implementation.

```cpp
#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "ntp.h"
#include "ntp/validation.h"
#include "ntp/schedule.h"
#include "rtc.h"
#include "wifi_provision.h"

namespace wc::ntp {

static WiFiUDP   udp;
static NTPClient client(udp, "time.google.com", 0 /*offset*/, 0 /*update_interval; we bypass*/);

static bool      started               = false;
static bool      ever_synced           = false;
static uint32_t  consecutive_failures  = 0;
static uint64_t  next_deadline_ms      = 0;     // 0 = "fire immediately when Online"
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
        constexpr uint32_t NOMINAL_SECS = 86'400;  // 24h
        uint32_t remaining_secs = (sync_age < NOMINAL_SECS)
                                  ? (NOMINAL_SECS - sync_age) : 0;
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

## Integration

### main.cpp — wire ntp into setup() + loop()

Current (`firmware/src/main.cpp`, abbreviated for diff clarity — the
real file has the full buttons callback, all display-render fields,
and longer comments around the rtc/wifi_provision sequencing):

```cpp
void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    wc::wifi_provision::begin();
    wc::rtc::begin();              // AFTER wifi_provision
    wc::display::begin();
    wc::buttons::begin([](wc::buttons::Event e) { /* ... */ });
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();
    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        // render
    } else {
        wc::display::show(wc::display::Frame{});
    }
    delay(1);
}
```

Diff:

```cpp
#include "ntp.h"   // new include

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    wc::wifi_provision::begin();
    wc::rtc::begin();              // AFTER wifi_provision
    wc::ntp::begin();              // AFTER rtc — for era reference
    wc::display::begin();
    wc::buttons::begin([](wc::buttons::Event e) { /* ... */ });
}

void loop() {
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // sync scheduler; no-op when not Online
    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        // render — unchanged
    } else {
        wc::display::show(wc::display::Frame{});
    }
    delay(1);
}
```

### Public-API addition: `wifi_provision::touch_last_sync`

Currently `nvs_store::touch_last_sync(uint64_t unix_seconds)` is
defined in `firmware/lib/wifi_provision/src/nvs_store.cpp:96` and
called only from inside the wifi_provision module. ntp needs to
call it.

Add to `firmware/lib/wifi_provision/include/wifi_provision.h`:

```cpp
// Stamp NVS with a successful NTP sync at unix_seconds (UTC).
// Called by the ntp module on a successful sync. Display reads
// the corresponding seconds_since_last_sync() to drive the amber
// stale-sync tint.
void touch_last_sync(uint64_t unix_seconds);
```

Add a thin wrapper in `firmware/lib/wifi_provision/src/wifi_provision.cpp`
that delegates to the existing `nvs_store::touch_last_sync`. No
behavior change, no NVS schema change.

**Avoiding the duplicate-API trap.** With the public wrapper added,
both `wc::wifi_provision::touch_last_sync(uint64_t)` and the
internal `wc::wifi_provision::nvs_store::touch_last_sync(uint64_t)`
become reachable. The public wrapper is the only sanctioned caller
from outside the module. As part of this PR, add a one-line comment
above the `nvs_store` declaration noting it is internal — use the
public wrapper. No header-shape change beyond that.

### Parent-spec edits (same commit per "no conflicting guides" rule)

Two edits land in the spec-add commit:

- `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
  §Time sync: "NTP sync on boot and every 6 hours" →
  "NTP sync on boot and every 24 hours (±30 min jitter)".
- `TODO.md` Phase 2 firmware modules section, the `ntp` bullet:
  "every 6 hours" → "every 24 hours".

### platformio.ini

NTPClient is already declared in `[esp32-base] lib_deps`
(`arduino-libraries/NTPClient@^3.2.1`). Three edits:

- Add `-I lib/ntp/include` to `[env:native] build_flags`.
- Add `+<../lib/ntp/src/validation.cpp>` to `[env:native] build_src_filter`.
- Add `+<../lib/ntp/src/schedule.cpp>` to `[env:native] build_src_filter`.

`ntp.cpp` is NOT added to the native filter — `#ifdef ARDUINO`
short-circuits it on that target.

## Testing

### Pure-logic native tests (Unity)

**`test_ntp_validation`** — 3 tests.

`is_plausible_epoch`:

- `test_plausible_zero_rejected`: `is_plausible_epoch(0) == false`.
- `test_plausible_just_below_floor_rejected`:
  `is_plausible_epoch(MIN_PLAUSIBLE_EPOCH - 1) == false`.
- `test_plausible_floor_accepted`:
  `is_plausible_epoch(MIN_PLAUSIBLE_EPOCH) == true`.

**`test_ntp_schedule`** — 7 tests.

`next_backoff_ms` (4):

- `test_backoff_n1_thirty_seconds`: `next_backoff_ms(1) == 30'000`.
- `test_backoff_n5_eight_minutes`: `next_backoff_ms(5) == 480'000`.
- `test_backoff_n6_at_cap`: `next_backoff_ms(6) == 1'800'000`.
- `test_backoff_n100_still_at_cap`: `next_backoff_ms(100) == 1'800'000`.

`next_deadline_after_success` (3):

- `test_deadline_jitter_zero_minus_30min`:
  `next_deadline_after_success(t0, 0) == t0 + 24h - 30m`.
- `test_deadline_jitter_mid_no_offset`:
  `next_deadline_after_success(t0, 1'800'000) == t0 + 24h`.
- `test_deadline_jitter_max_plus_30min_minus_1ms`:
  `next_deadline_after_success(t0, 3'599'999) == t0 + 24h + 30m - 1ms`.

Total ntp pure-logic tests: **10** (3 validation + 7 schedule).

Target native-suite count after this module: 140 (post-rtc) + 10
(ntp) = **150 native tests**.

### ESP32 adapter — NOT native-testable

Three things the native suite can't exercise:

- NTPClient UDP exchange with a real server.
- `WiFi.hostByName` resolution path.
- DS3231 round-trip after `set_from_epoch` (covered already by rtc's
  hardware checklist).

These are covered by the hardware manual-check document below.

### Hardware manual check: `firmware/test/hardware_checks/ntp_checks.md`

Created in this module's PR. Executed during breadboard bring-up
after WiFi + DS3231 are both up.

1. **First-ever sync.** Fresh-flashed device. Walk through captive
   portal, submit creds. After `Online` transition, watch serial
   for `[ntp] sync ok, epoch=<value>, next in ~24h` within a few
   seconds. Confirm `<value>` is a plausible 2026+ timestamp.
   Confirm `wc::rtc::now()` from a follow-up sketch reports
   wall-clock time correctly.
2. **Sync persists.** Power cycle. After re-`Online`, confirm
   `seconds_since_last_sync()` reflects the wait time (not
   `UINT32_MAX`), and the new sync runs per the 24h schedule (does
   NOT fire immediately unless overdue).
3. **WiFi drop mid-loop.** While running, kill the WiFi (turn off
   AP). Observe serial: ntp module quietly stops attempting (no
   spam). Restore WiFi. Confirm next attempt fires per the
   prevailing schedule (NOT immediately on reconnect unless
   overdue).
4. **DNS failure handling.** Block `time.google.com` at the router
   (DNS sinkhole). Observe `[ntp] DNS resolution failed` in serial
   on next attempt. Confirm consecutive_failures increments by
   watching cadence: 30 s → 1 m → 2 m → 4 m → 8 m → 30 m. Unblock;
   confirm next attempt succeeds and resets to nominal.
5. **Implausible-epoch rejection.** Hard to inject naturally;
   skipped unless a custom test NTP server is set up. Documented
   so a future me knows it's covered by native tests already.
6. **24-hour cadence sanity.** Power on. Note epoch + millis at
   first sync. Wait 24h ± 30m. Confirm second sync fires within
   that window. (Long-running test; may be deferred to burn-in
   phase.)
7. **Stale-sync tint.** Sync once successfully. Disconnect WiFi for
   >24h (use a dedicated test environment or block
   `time.google.com` for the duration). Confirm display amber tint
   activates per the display module's contract. (Cross-module
   integration check; the tint logic itself lives in display.)

## Open issues

One implementation-time note — not a design open item, but call it
out for the plan:

- **`extended_now_ms` precision under WiFi-task starvation.**
  Adapter computes the wraps-extended monotonic counter inside
  `loop()`. If `loop()` doesn't run for >49.7 days continuously
  (extremely unlikely on this firmware — the WiFi task can starve
  for seconds, never days), the wrap-detection arithmetic loses a
  full wrap. Acceptable: at the 24h cadence, even multi-second
  starvation is invisible.

## Verified numbers

| Item | Value | Source |
|---|---|---|
| NTP server | `time.google.com` | [Google Public NTP](https://developers.google.com/time/) — anycast, leap-smeared, public-infrastructure commitment |
| Sync cadence (nominal) | 24 h ± uniform(-30 min, +30 min) | NTP Pool ToS minimum 30 min, leaning conservative for a 40-year keeper; DS3231 ±2 ppm drift = 17 s/month, well inside 5-min display granularity |
| Backoff curve | 30 s → 1 m → 2 m → 4 m → 8 m → cap 30 m | `~/.claude/patterns/reliability/retry-backoff.md` (capped exponential + jitter); cap chosen ≥ NTP Pool ToS floor for safety |
| NTP era 1 boundary | 2036-02-07 06:28:16 UTC | NTP timestamp 2^32 = 4'294'967'296 seconds since 1900 |
| Y2106 wrap | 2106-02-07 06:28:16 UTC | uint32 unix epoch overflow; out of expected device lifetime |
| MIN_PLAUSIBLE_EPOCH | 1'767'225'600 | 2026-01-01 00:00:00 UTC; verified via `python -c 'import datetime;print(int(datetime.datetime(2026,1,1,tzinfo=datetime.timezone.utc).timestamp()))'` |
| NTP epoch offset (`SEVENZYYEARS`) | 2'208'988'800 | seconds between 1900-01-01 and 1970-01-01 |
| NTPClient version | 3.2.1 | `firmware/platformio.ini` `[esp32-base] lib_deps` (`arduino-libraries/NTPClient@^3.2.1`) |
| NTPClient internal gate default | 60 s (we bypass; we call `forceUpdate()`) | NTPClient.cpp `_updateInterval` default |
| NTPClient blocking time | up to ~1 s busy-wait | `NTPClient::forceUpdate()` `delay(10)` retry loop |
| `forceUpdate` failure modes | DNS crash (#73), stale buffer in `update` (we don't call it), socket churn (we begin once) | arduino-libraries/NTPClient GitHub issues |
| DS3231 drift rating | ±2 ppm, ~17 s/month | DS3231 datasheet + parent spec §Time sync |
| Display stale-sync threshold | 86'400 s = 24 h | `firmware/lib/display/include/display/renderer.h` (amber tint contract) |
| Last-sync NVS API | `wc::wifi_provision::nvs_store::touch_last_sync(uint64_t)` (existing) | `firmware/lib/wifi_provision/src/nvs_store.cpp:96` |
| Last-sync read API (public) | `wc::wifi_provision::seconds_since_last_sync()` | `firmware/lib/wifi_provision/include/wifi_provision.h:20` |
| Online state observable | `wc::wifi_provision::state() == State::Online` | `firmware/lib/wifi_provision/include/wifi_provision.h:15` |

## References

- Parent design: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
  (§Time sync, §Hardware Platform)
- Sibling rtc spec (template + dependency): `docs/superpowers/specs/2026-04-17-rtc-design.md`
- Reliability patterns (retry-backoff, timeout, circuit-breaker,
  graceful-degradation): `~/.claude/patterns/reliability/`
- Hardware pinout: `docs/hardware/pinout.md` (no ntp-specific
  hardware; relies on existing WiFi + I²C paths)
- Sibling modules:
  - `firmware/lib/wifi_provision/` (state observer + last-sync API)
  - `firmware/lib/rtc/` (downstream consumer of `set_from_epoch`)
  - `firmware/lib/display/` (consumer of `seconds_since_last_sync`
    for amber stale-sync tint)
- NTPClient library: `arduino-libraries/NTPClient@3.2.1`
  (https://github.com/arduino-libraries/NTPClient)
- Y2036 NTP era rollover: https://www.ntp.org/reflib/y2k/
- NTP Pool Terms of Service: https://www.ntppool.org/tos.html
- Google Public NTP: https://developers.google.com/time/
