// firmware/src/main.cpp
#include <Arduino.h>
#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "ntp.h"
#include "rtc.h"
#include "wifi_provision.h"

#if defined(BENCH_BACKDOOR_SSID) || defined(BENCH_SIM)
#include <SD.h>                       // bench 'l' command: list SD root
#endif
#ifdef BENCH_BACKDOOR_SSID
// TEMP bench-only block — remove before final assembly. Injects WiFi
// credentials directly into NVS (same atomic write the captive portal
// uses) so bench iteration doesn't need the phone flow, and adds a
// serial command channel ('h'/'m'/'a') that mirrors the button events.
// Credentials arrive via -D build flags (pulled from the macOS keychain
// at build time); nothing secret lives in this file or in git.
// Build with -D BENCH_SIM=1 alone to get the serial sim + frame
// telemetry without the credential inject (board must already be
// provisioned).
#include "wifi_provision/form_parser.h"
namespace wc::wifi_provision::nvs_store {
    bool has_credentials();
    bool write(const FormBody& body);
}
#endif

static void handle_button_event(wc::buttons::Event e);

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    Serial.flush();  // TEMP boot breadcrumbs — remove after bench debug

#ifdef BENCH_BACKDOOR_SSID
    if (!wc::wifi_provision::nvs_store::has_credentials()) {
        wc::wifi_provision::FormBody creds;
        creds.ssid = BENCH_BACKDOOR_SSID;
        creds.pw   = BENCH_BACKDOOR_PASS;
        creds.tz   = "PST8PDT,M3.2.0,M11.1.0";
        bool ok = wc::wifi_provision::nvs_store::write(creds);
        Serial.printf("[bench] backdoor credential inject (%s): %s\n",
                      BENCH_BACKDOOR_SSID, ok ? "OK" : "FAILED");
        Serial.flush();
    } else {
        Serial.println("[bench] NVS already has credentials; backdoor idle");
        Serial.flush();
    }
#endif

    wc::wifi_provision::begin();   // runs setenv/tzset on warm boot
    Serial.println("[boot] wifi_provision::begin done"); Serial.flush();
    wc::rtc::begin();              // AFTER wifi_provision — load-bearing
                                   // so TZ is set before first now()
    Serial.println("[boot] rtc::begin done"); Serial.flush();
    wc::ntp::begin();              // AFTER wifi_provision; warm-boot
                                   // resume reads NVS-stored last-sync
    Serial.println("[boot] ntp::begin done"); Serial.flush();
    wc::display::begin();
    Serial.println("[boot] display::begin done"); Serial.flush();

    wc::buttons::begin(handle_button_event);
    Serial.println("[boot] buttons::begin done"); Serial.flush();
    wc::audio::begin({CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                      CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE});
    Serial.println("[boot] audio::begin done — setup complete"); Serial.flush();
}

static void handle_button_event(wc::buttons::Event e) {
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
                wc::audio::play();
            }
            break;
        case BE::ResetCombo:
            Serial.println("[buttons] ResetCombo — resetting to captive portal");
            wc::wifi_provision::reset_to_captive();
            break;
    }
}

void loop() {
#ifdef BENCH_BACKDOOR_SSID
#define BENCH_SIM 1
#endif
#ifdef BENCH_SIM
    // TEMP: serial button simulation — 'h' hour, 'm' minute, 'a' audio,
    // 'X' reset-to-captive (wipes NVS + restarts — bench only).
    // Same dispatch as the physical switches; ResetCombo deliberately
    // not exposed (it wipes NVS).
    while (Serial.available()) {
        char c = (char)Serial.read();
        using BE = wc::buttons::Event;
        if      (c == 'h') { Serial.println("[bench] sim HourTick");     handle_button_event(BE::HourTick); }
        else if (c == 'm') { Serial.println("[bench] sim MinuteTick");   handle_button_event(BE::MinuteTick); }
        else if (c == 'a') { Serial.println("[bench] sim AudioPressed"); handle_button_event(BE::AudioPressed); }
        else if (c == 'r') {
            // Restore DS3231 from the NTP-true system clock (button ticks
            // above only skew the DS3231, never the system clock).
            wc::rtc::set_from_epoch((uint32_t)time(nullptr));
            Serial.println("[bench] DS3231 restored from system clock");
        }
        else if (c == 'X') {
            Serial.println("[bench] reset_to_captive (NVS wipe + restart)");
            wc::wifi_provision::reset_to_captive();
        }
        else if (c == 'l') {
            // List SD root: name + size. Audio owns the mount; SD is the
            // Arduino global, safe to read between playback pumps.
            File root = SD.open("/");
            if (!root) {
                Serial.println("[bench] SD root open failed");
            } else {
                Serial.println("[bench] SD root listing:");
                for (File f = root.openNextFile(); f;
                     f = root.openNextFile()) {
                    Serial.printf("[bench]   %-16s %8u bytes%s\n",
                                  f.name(), (unsigned)f.size(),
                                  f.isDirectory() ? " <dir>" : "");
                    f.close();
                }
                root.close();
                Serial.println("[bench] end of listing");
            }
        }
        else if (c == 'b') {
            // Play birth.wav on demand (does not stamp the NVS birthday
            // guard) — audible verification after an SD reload.
            Serial.println("[bench] play birth.wav");
            wc::audio::play_birthday_message();
        }
    }
#endif
    wc::wifi_provision::loop();
    wc::buttons::loop();
    wc::ntp::loop();               // sync scheduler; no-op when not Online
    wc::audio::loop();             // pump I²S when Playing; auto-fire check when Idle

    // Throttle display rendering to ~30 Hz. At full loop speed (~1 kHz,
    // given the delay(1) below) the display was re-rendering 1000× per
    // second — pure waste, plus saturating the ESP32 enough that the
    // UART serial output became garbled during bench-tests. 30 Hz is
    // plenty for the only animated element (birthday rainbow, 60 s
    // period) and comfortable for button/audio CPU budget.
    static uint32_t last_render_ms = 0;
    const uint32_t now = millis();
    if (now - last_render_ms >= 33) {
        last_render_ms = now;

        // Render the clock face whenever we have trustworthy time — i.e.,
        // at least one successful NTP sync has happened on this device
        // (seconds_since_last_sync() != UINT32_MAX). That signal also
        // implies wifi_provision::begin() ran setenv/tzset, so
        // localtime_r() inside rtc::now() returns fields in the user's
        // zone. We deliberately DO NOT gate on state() == Online:
        // the parent spec §Time sync promises the clock free-runs on the
        // DS3231 during WiFi drops / reconnects / captive-portal re-entry.
        //
        // Blank falls through only when there is no trustworthy time:
        // never provisioned (NVS last_sync == 0 — first-ever boot or a
        // post-reset-to-captive wipe, where TZ is unset too), or the
        // DS3231 lost power so rtc::begin() declined to seed the system
        // clock. A warm boot with a good coin cell seeds time(nullptr)
        // from the DS3231 in rtc::begin(), so the face shows (stale,
        // amber-tinted) DS3231 time immediately — even with WiFi down —
        // instead of going dark.
        uint32_t sync_age = wc::wifi_provision::seconds_since_last_sync();
        if (sync_age != UINT32_MAX) {
            // rtc::now() is a blocking I²C read and the face only changes
            // per minute, so cache it and re-read at ~1 Hz rather than on
            // every 33 ms render tick. The rainbow animation uses now_ms
            // (millis) and still advances every frame.
            static wc::rtc::DateTime cached_dt{};
            static uint32_t last_rtc_ms = 0;
            if (last_rtc_ms == 0 || now - last_rtc_ms >= 1000) {
                last_rtc_ms = now;
                cached_dt = wc::rtc::now();
            }
            const wc::rtc::DateTime& dt = cached_dt;
            wc::display::RenderInput in{};
            in.year   = dt.year;
            in.month  = dt.month;
            in.day    = dt.day;
            in.hour   = dt.hour;
            in.minute = dt.minute;
            in.now_ms = now;
            in.seconds_since_sync = sync_age;
            in.birthday = {CLOCK_BIRTH_MONTH, CLOCK_BIRTH_DAY,
                           CLOCK_BIRTH_HOUR,  CLOCK_BIRTH_MINUTE};
            wc::display::Frame frame = wc::display::render(in);
#ifdef BENCH_SIM
            // TEMP: log the lit-LED set whenever it changes so the face
            // can be verified over serial without eyes on the board.
            {
                static uint64_t last_sig = 0;
                uint64_t sig = 0;
                int lit = 0;
                for (int i = 0; i < (int)frame.size(); i++) {
                    if (frame[i].r | frame[i].g | frame[i].b) {
                        sig ^= 0x9E3779B97F4A7C15ull * (uint64_t)(i + 1);
                        lit++;
                    }
                }
                sig ^= (uint64_t)lit << 56;
                if (sig != last_sig) {
                    last_sig = sig;
                    Serial.printf("[bench] frame %02u:%02u lit=%d idx=",
                                  in.hour, in.minute, lit);
                    for (int i = 0; i < (int)frame.size(); i++)
                        if (frame[i].r | frame[i].g | frame[i].b)
                            Serial.printf("%d,", i);
                    Serial.println();
                }
            }
#endif
            wc::display::show(frame);
        } else {
            // Pre-first-sync: blank. Captive portal is running here;
            // displaying UTC (or DS3231 lost-power garbage) would be
            // more confusing than a dark face while Dad provisions.
            wc::display::show(wc::display::Frame{});
        }
    }

    delay(1);  // yield to IDLE for watchdog + WiFi
}
