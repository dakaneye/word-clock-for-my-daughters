// firmware/lib/wifi_provision/include/wifi_provision.h
#pragma once

#include <Arduino.h>
#include <cstdint>
#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

// Module lifecycle. Call begin() once from setup(), loop() from loop().
void begin();
void loop();

// Current state for observers (e.g., the display module).
State state();

// Seconds since the last successful NTP sync. UINT32_MAX if never synced.
// Display module uses this to decide when to apply the amber stale-sync tint
// (>24 h).
uint32_t seconds_since_last_sync();

// Force a reset back into captive-portal mode. Clears stored credentials.
// Called by the buttons module when Hour + Audio is held for 10 s.
void reset_to_captive();

// Fire Event::AudioButtonConfirmed into the state machine.
// Caller: the buttons module when Audio is pressed during captive-portal
// AwaitingConfirmation state.
void confirm_audio();

} // namespace wc::wifi_provision
