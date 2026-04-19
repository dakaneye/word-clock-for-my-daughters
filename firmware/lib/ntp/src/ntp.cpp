// firmware/lib/ntp/src/ntp.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag NTPClient/WiFi into the native build.
// Same pattern as the wifi_provision/buttons/display/rtc adapters.
#ifdef ARDUINO

// Compile-time guard: this module targets the Arduino-ESP32 framework
// specifically (uses <WiFi.h>, <NTPClient.h>). If a future toolchain
// change defines ARDUINO but on a different arch, fail loud instead
// of silently compiling to nothing useful.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "ntp adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

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
    if (wc::audio::is_playing()) return;   // defer sync; ~1s forceUpdate
                                           // would underrun I²S DMA
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
