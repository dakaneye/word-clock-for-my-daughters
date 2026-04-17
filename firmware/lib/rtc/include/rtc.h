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
