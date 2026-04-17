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
