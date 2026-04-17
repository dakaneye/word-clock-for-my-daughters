// firmware/src/main.cpp
#include <Arduino.h>
#include "buttons.h"
#include "display.h"
#include "display/renderer.h"
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);

    wc::wifi_provision::begin();
    wc::display::begin();

    wc::buttons::begin([](wc::buttons::Event e) {
        using BE = wc::buttons::Event;
        using WS = wc::wifi_provision::State;
        switch (e) {
            case BE::HourTick:
                Serial.println("[buttons] HourTick (rtc module not yet wired)");
                break;
            case BE::MinuteTick:
                Serial.println("[buttons] MinuteTick (rtc module not yet wired)");
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

    // Bring-up stub: once provisioning is Online, push a hardcoded
    // 14:23 non-holiday non-birthday frame so the breadboard LED
    // check has something to verify. This is replaced by the
    // main.cpp integration spec with a real RTC-driven render.
    if (wc::wifi_provision::state() == wc::wifi_provision::State::Online) {
        wc::display::RenderInput in{};
        in.year   = 2027;   // non-holiday year window
        in.month  = 5;
        in.day    = 15;
        in.hour   = 14;     // "IT IS ... PAST TWO IN THE AFTERNOON"
        in.minute = 23;
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
