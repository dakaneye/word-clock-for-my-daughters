// firmware/lib/audio/include/audio.h
//
// ESP32-only public API. Depends on Arduino.h, SD, Preferences,
// driver/i2s.h. Include ONLY from translation units that compile
// under the Arduino toolchain (the emory / nora PlatformIO envs).
// Native tests include the pure-logic headers under audio/ — never
// this one.
#pragma once

#include <Arduino.h>
#include "audio/fire_guard.h"

namespace wc::audio {

// Initialize the I²S driver, mount SD, open NVS namespace, seed
// last_birth_year. No audio output. Idempotent. Call once from
// setup() AFTER wifi_provision, rtc, ntp, display, and buttons
// have all begun.
void begin(const BirthConfig& birth);

// Drives the state machine. Call from main.cpp's loop().
// - When Playing: reads the next PCM chunk from SD, applies Q8
//   gain, writes to I²S with 0 timeout. On EOF / error: closes
//   file, transitions to Idle.
// - When Idle: runs should_auto_fire() against rtc::now();
//   transitions to Playing with birth.wav if the guard passes.
void loop();

// Open lullaby.wav, validate header, transition Idle → Playing.
// No-op if already Playing (caller should check is_playing()).
void play_lullaby();

// Close the open file, stop I²S TX, transition Playing → Idle.
// No-op if already Idle.
void stop();

// True iff the state machine is Playing. Public observer used by
// the main.cpp button dispatch and by ntp::loop() to defer syncs.
bool is_playing();

} // namespace wc::audio
