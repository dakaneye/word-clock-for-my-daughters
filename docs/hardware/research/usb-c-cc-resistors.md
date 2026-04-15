# USB-C CC Resistor Configuration — Without Power Delivery

## Why this matters

Our USB-C breakout (Cermant 16P) has passive 5.1kΩ CC resistors only — no PD controller chip. This means we can't negotiate for high current; we rely on the USB-C Type-C spec's default current levels.

Our worst-case power budget (`docs/hardware/pinout.md`) is ~2.6A peak when WS2812B LEDs are full white + audio playing + ESP32 on WiFi. This doc confirms what our USB-C configuration will actually deliver.

## Verified facts

### What 5.1kΩ on CC1 and CC2 signals

From Adafruit (confirmed):

> "The two 5.1kΩ resistors on the CC1 pins indicate to the upstream port to provide **5V and up to 1.5A** (whether the upstream can supply that much current depends on what you're connecting to)."

This is the **"default USB-C sink"** configuration. It identifies the device as a USB 2.0-style consumer. Per Type-C spec, the source is **required** to provide only 500mA (USB 2.0 default).

More modern Type-C sources (USB-C laptop chargers) may advertise 1.5A or 3.0A as available via their Rp resistors, but:
- **Without PD negotiation**, the source decides what it's willing to deliver, up to the advertised limit.
- **Hard-spec compliant USB-C sources** without PD are limited to 1.5A.
- **Most consumer 5V/2A "USB-C" chargers** ignore strict Type-C spec and deliver full rated current — but this is not guaranteed.

### What 3.0A requires

USB Power Delivery (PD) negotiation. Requires a PD controller chip (e.g., CH221K, FUSB302) on the sink side that negotiates with the source. Not present on our Cermant breakout.

## Implications for our design

**Worst case (2.6A) exceeds the 1.5A Type-C default guarantee.**

Three mitigation strategies, in order of preference:

1. **Cap LED brightness in firmware** to keep worst-case draw below 1.5A.
   - 25 LEDs × 60mA = 1.5A full white. Capping brightness to ~50% = 750mA for LEDs, leaving 750mA budget for ESP32 + audio.
   - Implemented via FastLED's `FastLED.setBrightness()` or by not using full (255) RGB values.
   - Simple, no hardware changes, aligns with the spec's "warm white default" which naturally isn't full white anyway.

2. **Use a 5V/2A USB-C "dumb" charger** (most consumer phone/tablet chargers). These ignore strict Type-C spec and deliver the full 2A. User has TCKN 2-pack on the parts list already. Acceptable for a personal project; relies on the charger's non-compliant generosity.

3. **Add USB PD support on v2 PCB**: include a FUSB302 or CH221K controller to negotiate 5V/3A with a modern USB-PD charger. Adds $1-2 BOM and design complexity. Appropriate if we find the rail sags with brightness capping.

## Recommendation for v1 PCB

Use mitigation **(1) brightness cap** as the primary strategy. No hardware change from the current pinout. Document the firmware-level cap in Phase 2's display module.

Mitigation (2) is the fallback if a user connects a strict-spec USB-C source. Acceptable risk for a personal clock — the user will use the provided TCKN 5V/2A charger, which is a USB-A (despite the Type-C cable) and not subject to Type-C spec anyway.

Re-evaluate (3) if Phase 5 burn-in reveals rail sag under realistic use.

## Source

Adafruit USB-C Breakout Board product page:
https://www.adafruit.com/product/4090

Fetched 2026-04-15.

## Related reading (not fetched)

- USB Type-C Cable and Connector Specification (www.usb.org) — the authoritative source
- "USB-C: Uncertain Connection" by Allan Yogasingham — good intro to CC resistor behavior without PD
