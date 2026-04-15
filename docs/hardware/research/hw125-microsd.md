# HW-125 microSD SPI Breakout

## Part

- **Module:** Generic "microSD card module" based on the HW-125 reference design, widely cloned on Amazon and AliExpress.
- **Project-specific source:** Stemedu 2-pack from the Phase 2 wishlist.

## Verified facts

### Supply voltage
- **Input: 5 V** is the expected supply to VCC.
- Module includes an **on-board ultra-low-dropout 3.3 V regulator** that steps 5 V down to the 3.3 V the SD card requires.
- The microSD card itself needs 3.3 V; the module handles the regulation.

### Level shifting
- Module includes a **74LVC125A quad bus buffer** on the SPI lines (MOSI, MISO, CLK, CS).
- The 74LVC125A shifts between 5 V and 3.3 V logic, so **both 5 V microcontrollers (Arduino Uno) and 3.3 V microcontrollers (ESP32, ESP8266) work with this module without additional level shifting.**

### ESP32 wiring
- VCC: 5 V (not 3.3 V — the regulator and level shifter need 5 V to function correctly)
- GND: GND
- SPI lines: any VSPI-compatible pins. This project uses CS=5, MOSI=23, MISO=19, CLK=18 (see pinout.md).
- **CS pin:** SD modules hold CS HIGH when idle (via the 74LVC125A's pullup behavior). Safe for ESP32's GPIO 5 boot strap — no risk of pulling GPIO 5 LOW during boot.

### Known compatibility caveat
- The tutorial only explicitly shows Arduino Uno pinouts. ESP32 uses different SPI pins but the protocol is identical and standard `SD` library on Arduino-ESP32 handles this.
- FAT32 formatting required for most SD libraries. Allocation unit size should be default (32 KB or auto).

## Source

Last Minute Engineers — Arduino Micro SD Card Module Tutorial:
https://lastminuteengineers.com/arduino-micro-sd-card-module-tutorial/

Fetched 2026-04-15.
