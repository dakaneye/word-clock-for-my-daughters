<!-- firmware/test/hardware_checks/buttons_checks.md -->
# buttons — manual hardware verification

Run these with the breadboard wired per
`docs/hardware/breadboard-bring-up-guide.md` §Step 6. Three tact
switches: Hour → GPIO 32, Minute → GPIO 33, Audio → GPIO 14. Each
switch has one leg on its GPIO and the diagonal-opposite leg on GND.
No external resistors — the ESP32's INPUT_PULLUP provides the pullup.

## 1. Each button produces its expected serial log

- [ ] Flash: `pio run -e emory -t upload`
- [ ] Open serial monitor: `pio device monitor -e emory`
- [ ] Boot completes; `word-clock booting for target: Emory` appears.
- [ ] Press Hour → serial shows `[buttons] HourTick (rtc module not yet wired)`.
- [ ] Press Minute → serial shows `[buttons] MinuteTick (rtc module not yet wired)`.
- [ ] Press Audio (outside captive portal) → serial shows `[buttons] AudioPressed (audio module not yet wired)`.

## 2. Debounce behaves

- [ ] Rapid-tap any one button 5 times — serial shows exactly 5 events, no spurious extras or swallows.
- [ ] Press and hold any one button for 3 s — serial shows exactly 1 event (fires on press-down, not repeatedly while held).
- [ ] Release the held button — no event fires on release.

## 3. Reset combo fires at 10 s

- [ ] With the clock running normally (post-wifi), hold Hour + Audio simultaneously.
- [ ] After 10 s, serial shows `[buttons] ResetCombo — resetting to captive portal`.
- [ ] The clock re-enters AP mode: `WordClock-Setup-XXXX` reappears on your phone's WiFi list.
- [ ] Stored credentials are cleared (you'll need to re-submit the form to reconnect).

## 4. Reset combo cancels on early release

- [ ] Hold Hour + Audio for ~5 s (less than 10 s), then release Hour while keeping Audio held.
- [ ] No ResetCombo event fires.
- [ ] Release Audio. No delayed event. No AudioPressed event fires (suppressed during the combo hold + no press-down edge on release).
- [ ] Re-press Hour + Audio and hold a fresh 10 s — combo fires normally (timer reset).

## 5. Audio during AwaitingConfirmation confirms credentials

- [ ] Trigger reset combo (step 3) to get back to captive portal.
- [ ] Connect phone, submit valid WiFi credentials via the form.
- [ ] Phone page shows "Press the Audio button on the clock".
- [ ] Press Audio within 60 s → clock transitions to Validating → Online.
- [ ] Serial does NOT show `[buttons] AudioPressed (audio module not yet wired)` — confirming the state-routing switched to `confirm_audio()` instead of the default log.

## 6. Minute during combo is unaffected

- [ ] Hold Hour + Audio + Minute simultaneously for 10 s.
- [ ] ResetCombo fires (as in check 3).
- [ ] During the 10 s hold, pressing Minute registers as normal MinuteTicks — combo suppression is Hour/Audio only.
