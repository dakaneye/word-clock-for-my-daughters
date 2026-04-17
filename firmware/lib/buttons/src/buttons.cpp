// firmware/lib/buttons/src/buttons.cpp
// Guarded with #ifdef ARDUINO — same pattern as wifi_provision's
// adapters. PlatformIO LDF would otherwise compile this TU in the native
// test build where <Arduino.h> doesn't exist.
#ifdef ARDUINO

// Compile-time guard: targets ESP32 GPIO pinmap + Arduino-ESP32 digitalRead.
// If a future toolchain change defines ARDUINO but without the ESP32 arch,
// fail loud instead of silently compiling to nothing.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "buttons adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <Arduino.h>
#include "buttons.h"
#include "buttons/debouncer.h"
#include "buttons/combo_detector.h"
#include "buttons/event.h"
#include "pinmap.h"                 // PIN_BUTTON_{HOUR,MINUTE,AUDIO}

namespace wc::buttons {

static Debouncer     db_hour;
static Debouncer     db_min;
static Debouncer     db_audio;
static ComboDetector combo;
static Handler       on_event;

void begin(Handler h) {
    pinMode(PIN_BUTTON_HOUR,   INPUT_PULLUP);
    pinMode(PIN_BUTTON_MINUTE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_AUDIO,  INPUT_PULLUP);
    on_event = std::move(h);
}

void loop() {
    const uint32_t now = millis();
    // Raw reads: LOW = pressed for active-low INPUT_PULLUP buttons.
    const bool h_raw = (digitalRead(PIN_BUTTON_HOUR)   == LOW);
    const bool m_raw = (digitalRead(PIN_BUTTON_MINUTE) == LOW);
    const bool a_raw = (digitalRead(PIN_BUTTON_AUDIO)  == LOW);

    const bool h_edge = db_hour.step(h_raw, now);
    const bool m_edge = db_min.step(m_raw, now);
    const bool a_edge = db_audio.step(a_raw, now);

    const bool combo_fired =
        combo.step(db_hour.is_pressed(), db_audio.is_pressed(), now);

    // Priority order (see spec §Event ordering).
    if (combo_fired)                  on_event(Event::ResetCombo);
    if (h_edge && !combo.in_combo())  on_event(Event::HourTick);
    if (m_edge)                       on_event(Event::MinuteTick);
    if (a_edge && !combo.in_combo())  on_event(Event::AudioPressed);
}

} // namespace wc::buttons

#endif // ARDUINO
