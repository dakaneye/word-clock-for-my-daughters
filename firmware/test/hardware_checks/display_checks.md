<!-- firmware/test/hardware_checks/display_checks.md -->
# display — manual hardware verification

Run these after the full breadboard bring-up reaches
`docs/hardware/breadboard-bring-up-guide.md` §Step 9. Full WS2812B
chain (35 LEDs) on GPIO 13 through the 74HCT245 level shifter +
300 Ω series resistor + 1000 µF bulk cap.

Flash: `pio run -e emory -t upload`
Serial: `pio device monitor -e emory`

## 1. Boot + captive portal + blank face

- [ ] Serial shows `word-clock booting for target: EMORY`.
- [ ] Phone shows `WordClock-Setup-XXXX` AP. Face is all dark (no
      LEDs glowing) while in AP mode. Confirms main's
      `if state == Online` guard is in effect.
- [ ] Connect phone, submit valid WiFi creds via the form. State
      advances to Online per serial logs.

## 2. First light — warm-white clock face

- [ ] After Online transition, face shows a lit clock readout in
      warm white (per the hardcoded 14:23: `IT IS ... PAST TWO IN
      THE AFTERNOON`; the exact word set is whatever
      `time_to_words(14, 23)` returns).
- [ ] Each lit word reads uniformly — no per-letter flicker, no
      dimmer halos between word pockets. Confirms the light
      channel + diffuser stack are working AND the LED chain is
      wired correctly.
- [ ] No decor words (HAPPY/BIRTH/DAY/NAME) glow — it's not a
      birthday in the hardcoded scenario.

## 3. Color order verification

Temporarily patch `firmware/src/main.cpp` to paint one LED red:
replace the `wc::display::show(wc::display::render(in))` call with
a single-red-LED frame (e.g., `Frame f{}; f[0] = {255, 0, 0};
wc::display::show(f);`). Reflash.

- [ ] D1 (LED_IT, top-left corner per the face) glows RED.
      If it glows GREEN, the FastLED addLeds color order is wrong
      (should be `GRB` for WS2812B) — revert the spec's GRB
      declaration investigation rather than rewriting the renderer.
- [ ] Revert the patch; reflash.

## 4. Level-shifter sanity (optional but recommended)

- [ ] Disconnect the 74HCT245 output wire from the WS2812B DIN.
      Reflash and power-cycle: LEDs stay dark or show random
      colors (nothing downstream is receiving a stable signal).
      Confirms the level shifter is load-bearing — the bare ESP32
      3.3 V drive at 5 V Vdd would "sometimes work" (per
      `docs/hardware/pinout.md` §Critical issues #2).
- [ ] Reconnect and reflash; face returns to normal.

## 5. Holiday palette on real hardware

Temporarily edit `firmware/src/main.cpp` to set `in.month = 10;
in.day = 31;`. Reflash.

- [ ] Lit time words show alternating ORANGE and PURPLE per the
      Halloween palette. The pattern is deterministic by WordId
      enum idx — if HAPPY were lit (it isn't on a non-birthday
      Halloween), it'd be purple because enum idx 31 % 2 == 1.
- [ ] Revert to `in.month = 5; in.day = 15;`.

## 6. Birthday rainbow

Temporarily edit `firmware/src/main.cpp` to set `in.month = 10;
in.day = 6;`. Reflash.

- [ ] Time words show WARM WHITE (Oct 6 isn't a holiday).
- [ ] HAPPY, BIRTH, DAY, and NAME words cycle through the hue
      wheel smoothly. Full 360° cycle visibly completes in ≈ 60 s.
- [ ] The four decor words are at different hues at any instant
      (90° phase offset). Visually reads as a "chase" around the
      decor set.
- [ ] Revert to the non-birthday config.

## 7. Dim window

Temporarily edit `firmware/src/main.cpp` to set `in.hour = 20;
in.minute = 0;`. Reflash.

- [ ] Face brightness drops visibly to ~10% — readable in dim
      light, not blinding in a dark room.
- [ ] Face still shows a complete time readout (no words drop out
      at 10% brightness — the 300 Ω series resistor + level
      shifter path works at low current).
- [ ] Revert to `in.hour = 14;`.

## 8. Amber stale-sync

Temporarily edit `firmware/lib/display/include/display/renderer.h`
to `STALE_SYNC_THRESHOLD_S = 60` (one minute, for the test).
Reflash, let the clock come online, then disconnect WiFi by
powering off your router or moving out of range.

- [ ] After 60 s of no WiFi, `wifi_provision::seconds_since_last_sync()`
      crosses 60. Face shifts from warm white to AMBER
      (`{255, 120, 30}`).
- [ ] Reconnect WiFi; after the next NTP sync the face returns to
      warm white within a few seconds.
- [ ] Revert the threshold to 86'400. Reflash.

## 9. Power-budget sanity (belt-and-suspenders check)

- [ ] Measure USB-C input current during full-bright warm-white
      display (no birthday). Expected: ~500-700 mA at typical
      lit-word count (~7 words).
- [ ] Measure during a synthetic palette that hits the 700-sum
      per-entry cap on all 35 LEDs (e.g., temporarily patch
      warm_white() to return `{255, 230, 215}` and reflash).
      Expected: current stays under ~1.8 A because
      `FastLED.setMaxPowerInVoltsAndMilliamps` is enforcing.
- [ ] Revert the palette patch.

## 10. Stress / burn-in

- [ ] Let the clock run for 1 h on the warm-white readout. No
      hang, no flash corruption, no serial reset messages.
- [ ] Watch for any color drift across the 35 LEDs — all lit
      words should read visually identical. A single off-color
      LED suggests a bad pixel or a misaligned word pocket.
