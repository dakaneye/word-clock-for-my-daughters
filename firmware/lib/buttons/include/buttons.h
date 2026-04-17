// firmware/lib/buttons/include/buttons.h
//
// ESP32-only public API. Depends on Arduino.h. Include this ONLY in
// translation units that compile under the Arduino toolchain (i.e. the
// emory / nora PlatformIO envs). Native-env tests include the pure-logic
// headers `buttons/debouncer.h` and `buttons/combo_detector.h` instead —
// never this header.
#pragma once

#include <Arduino.h>
#include <functional>
#include "buttons/event.h"

namespace wc::buttons {

using Handler = std::function<void(Event)>;

// Register the pin modes + event callback. Call once from setup().
void begin(Handler on_event);

// Poll GPIOs, advance state machines, invoke callback for each event.
// Call once per main loop tick.
void loop();

} // namespace wc::buttons
