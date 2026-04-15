# ESP32-WROOM-32 GPIO Boot Behavior

## Why this matters

Some ESP32 GPIOs drive HIGH, LOW, or PWM during the boot process — BEFORE firmware runs. For an output pin connected to an LED data line, audio amp shutdown, or SPI CS, this can cause glitches (first LED flashes, amp pops, SD card confusion). For an input pin, it's usually fine because nothing is reading during boot.

This doc captures which pins are safe for which signals on our clock.

## Strapping pins — boot-time input requirements

These pins are sampled at reset to configure boot behavior. Must be driven to the correct state BEFORE power-up.

| Pin | Required at boot | Purpose | Our use |
|---|---|---|---|
| GPIO 0 | HIGH for run mode; LOW for download mode | Selects boot source | Unused. On-board pullup on dev board. On PCB: 10kΩ to 3.3V. |
| GPIO 2 | Floating or LOW | Internal pulldown; do not drive HIGH | Unused. Safe. |
| GPIO 5 | HIGH for normal boot | VSPI CS default | Used as SD CS. The HW-125 74LVC125A keeps CS HIGH when SD idle, so boot-time requirement satisfied. ✓ |
| GPIO 12 | LOW for 3.3V flash | VDD_SDIO voltage select | Unused. Internal pulldown keeps LOW. ✓ |
| GPIO 15 | HIGH | Suppresses boot debug output on UART0 | Unused. Internal pullup keeps HIGH. ✓ |

## Pins that output HIGH or PWM during boot

From Random Nerd Tutorials: **"GPIO 1, GPIO 3, GPIO 5, GPIO 6 to GPIO 11, GPIO 14, GPIO 15"** change state to HIGH or output PWM signals during boot.

Our project's use of these:

| Pin | Boot behavior | Our assignment | Risk | Status |
|---|---|---|---|---|
| 1 | UART0 TX — outputs boot debug text | Reserved (UART0 TX) | N/A | Safe (it's the debug console) |
| 3 | UART0 RX | Reserved (UART0 RX) | N/A | Safe |
| 5 | HIGH at boot | SD CS | SD card sees CS idle — normal | Safe |
| 6-11 | Flash memory | Reserved — NEVER USE | Breaks ESP32 | Safe (we don't use) |
| 14 | HIGH + PWM at boot | BUTTON_AUDIO (input) | Boot-time output on an input-pin line is harmless; the firmware sets pinMode INPUT_PULLUP after boot | Safe |
| 15 | HIGH at boot | Unused | N/A | Safe |

## Our LED_DATA pin: GPIO 13

GPIO 13 is **NOT in the "HIGH or PWM at boot" list.** It powers up as high-impedance input until firmware configures it. **No glitching risk for the first WS2812B LED during boot.** ✓

## Safe GPIOs for general digital output

Random Nerd Tutorials list: **GPIO 4, 13, 16-27, 32-33**.

Our assignments used: 13 (LED), 18/19/23/5 (SPI), 21/22 (I²C), 25/26/27 (I²S), 32/33/14 (buttons as inputs).

All our output assignments are safe. The only "risky" pin used is GPIO 5 (SD CS), but the HW-125's behavior at boot satisfies the HIGH-at-boot strapping requirement.

## Key takeaway for the pinout

No changes needed to the current pinout. All output pins are safe. The buttons on GPIO 14 (which has boot-time PWM) are inputs, so boot-time activity on that physical line doesn't matter — firmware configures the pin as INPUT_PULLUP after boot, and the tact switch is pulled to GND when pressed regardless.

## Source

Random Nerd Tutorials — ESP32 Pinout Reference:
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

Fetched 2026-04-15. Cross-reference with Espressif ESP32 datasheet (not fetched) before committing to PCB layout.
