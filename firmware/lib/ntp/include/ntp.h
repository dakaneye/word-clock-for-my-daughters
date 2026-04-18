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
