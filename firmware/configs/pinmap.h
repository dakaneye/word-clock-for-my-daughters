// firmware/configs/pinmap.h
//
// Shared GPIO pin assignments for every Phase 2 firmware module.
// Source of truth: docs/hardware/pinout.md §Firmware constants.
//
// The `configs/` directory is on the ESP32 envs' -I include path via
// `[esp32-base] build_flags = -I configs` in platformio.ini, so any
// Arduino-env source can `#include "pinmap.h"`. Native/pure-logic code
// never includes this header — it doesn't touch hardware.
#pragma once

// WS2812B LED strip
#define PIN_LED_DATA     13

// DS3231 RTC (I²C default pins)
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22

// microSD card (VSPI)
#define PIN_SD_CS         5
#define PIN_SD_MOSI      23
#define PIN_SD_MISO      19
#define PIN_SD_CLK       18

// MAX98357A I²S amplifier
#define PIN_I2S_BCLK     26
#define PIN_I2S_LRC      25
#define PIN_I2S_DIN      27

// Tact switches (INPUT_PULLUP, other leg to GND)
#define PIN_BUTTON_HOUR   32
#define PIN_BUTTON_MINUTE 33
#define PIN_BUTTON_AUDIO  14
