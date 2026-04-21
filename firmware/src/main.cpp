// firmware/src/main.cpp
#include <Arduino.h>
#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "ntp.h"
#include "rtc.h"
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();   // runs setenv/tzset on warm boot
    wc::rtc::begin();              // AFTER wifi_provision — load-bearing
                                   // so TZ is set before first now()
    wc::ntp::begin();              // AFTER wifi_provision; warm-boot
                                   // resume reads NVS-stored last-sync
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
        // Blank falls through when the device has never synced — first-
        // ever boot or a post-reset-to-captive NVS wipe. In that window
        // the TZ isn't set either, so painting anything from rtc::now()
        // would show UTC (or DS3231 lost-power defaults), which is worse
        // than blank.
        uint32_t sync_age = wc::wifi_provision::seconds_since_last_sync();
        if (sync_age != UINT32_MAX) {
            auto dt = wc::rtc::now();
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
            wc::display::show(wc::display::render(in));
        } else {
            // Pre-first-sync: blank. Captive portal is running here;
            // displaying UTC (or DS3231 lost-power garbage) would be
            // more confusing than a dark face while Dad provisions.
            wc::display::show(wc::display::Frame{});
        }
    }

    delay(1);  // yield to IDLE for watchdog + WiFi
}
