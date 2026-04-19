# Audio Module — Manual Hardware Checks

These checks exercise the ESP32 paths that the native test suite
cannot reach: real I²S DMA + MAX98357A + speaker output, real SD
card open / read / close via VSPI, real `Preferences`/NVS round-trip
for `last_birth_year`, real `rtc::now()` for the auto-fire guard.
Run during breadboard bring-up after the MAX98357A + microSD paths
have been wired (`docs/hardware/breadboard-bring-up-guide.md`
§MAX98357A + Speaker and §MicroSD) and the ntp
(`ntp_checks.md`) and rtc (`rtc_checks.md`) checklists are green.

Spec: `docs/superpowers/specs/2026-04-18-audio-design.md`.

## Prerequisites

- ESP32 flashed with the current emory or nora env build.
- Captive-portal flow completed at least once (NVS holds valid
  WiFi creds + TZ; `seconds_since_last_sync()` is non-UINT32_MAX).
- MicroSD prepared with canonical WAV files at the card root:
  - `/lullaby.wav` — 44.1 kHz / 16-bit / mono / PCM, ~3 min.
  - `/birth.wav`   — same format, ~30 s.
  - Authoring recipe: Audacity > File > Export > Export as WAV;
    Encoding = "Signed 16-bit PCM", Sample Rate = 44100 Hz,
    Channels = Mono.
- MAX98357A hardware gain jumper set at the assembly default
  (9 dB unless bench measurement says otherwise).
- `CLOCK_AUDIO_GAIN_Q8` left at the default 256 for the first
  bring-up pass.
- Speaker connected to MAX98357A output terminals.
- Serial monitor open at 115200 baud.

## Checks

1. **Boot path observable.** Power on. Expect
   `[audio] begin: SD ok, I2S ok` in serial. If `SD FAILED`
   appears, re-seat the card and re-check VSPI wiring per
   `docs/hardware/pinout.md`.

2. **Lullaby play.** Once the clock is Online, press the audio
   button. Expect `[audio] play /lullaby.wav` in serial, followed
   by audible playback through the speaker. Wait for natural EOF;
   expect `[audio] finished (played <N> bytes)` within ~3 min.
   Confirm the clock display continues rendering the current time
   during playback (display cadence is independent of audio).

3. **Button-during-playback stops.** Start lullaby. After ~10 s,
   press the audio button again. Expect `[audio] stopped (played
   <N> bytes)` in serial and immediate silence from the speaker.
   Press once more; expect fresh playback from the start.

4. **Volume sanity.** Listen for distortion / clipping on the
   loudest passages. If present: either re-encode the WAV at
   lower peak level (authoring fix), or lower `CLOCK_AUDIO_GAIN_Q8`
   to 181 (−3 dB) / 128 (−6 dB) via `configs/config_<kid>.h` and
   rebuild. Hardware jumper change (lower-gain resistor on
   MAX98357A) is the alternative if software trim isn't enough.
   Document the setting that worked in `docs/hardware/assembly-plan.md`.

5. **SD card removed mid-playback.** Start lullaby. Pull the
   microSD card from the slot. Expect either
   `[audio] error: i2s_write err=...` OR
   `[audio] aborted (played <N> bytes)` in serial, followed by
   silence. Re-seat the card; press the button again → plays
   successfully (no reboot needed).

6. **Missing file graceful.** Rename `/lullaby.wav` →
   `/xxx.wav` on the card, re-insert. Press audio button. Expect
   `[audio] error: file /lullaby.wav not found`; no sound. Stay
   Idle.

7. **Invalid WAV graceful.** Place a stereo 48 kHz WAV (or a
   renamed .mp3) at `/lullaby.wav`. Press audio button. Expect
   `[audio] error: wav header invalid (code=<N>)`; no sound.

8. **Birth auto-fire (simulated).** Pre-set the DS3231 via a
   bring-up sketch to 6:09:50 PM on Oct 6 of the current year
   (adjust month/day/time for the active env's `CLOCK_BIRTH_*`).
   Reset `audio/last_birth_year` to 0 via a wipe sketch OR by
   flashing a one-shot debug build that calls `Preferences::clear()`
   on the `"audio"` namespace at boot. Wait through 6:10:00 PM.
   Expect `[audio] play birth.wav (year=<current year>)` within
   one `loop()` tick of the minute roll. Confirm audible playback;
   wait for `[audio] finished`.

9. **Birth auto-fire fires once per year.** After check 8 passes,
   stay powered and wait until the next minute (6:11 PM). Expect
   no additional `[audio] play` log. Power-cycle. Expect no
   `[audio] play birth.wav` on next boot (NVS `last_birth_year`
   now matches the current year).

10. **Unknown-time gate.** Wipe NVS (hold Hour + Audio 10 s →
    `reset_to_captive`). Captive portal comes up; do NOT submit
    credentials. Pre-set the DS3231 to 6:10 PM on Oct 6 via the
    bring-up sketch. Verify no auto-fire — `time_known` is false
    because `seconds_since_last_sync()` reads UINT32_MAX. No
    `[audio] play birth.wav` log should appear.

11. **ntp deferral during playback.** Start lullaby playback. In
    serial, confirm no `[ntp] sync ok` / `[ntp] forceUpdate()`
    log lines fire during the ~3 min playback window, even if a
    deadline would otherwise fire. After the lullaby finishes,
    confirm ntp resumes its normal cadence.

12. **Underrun sanity.** During playback, stress the main loop by
    adding a temporary `Serial.println(millis());` every loop tick.
    Playback should remain clean. Audible glitches here would
    point to DMA ring sizing being too tight — rare at
    4 × 1024 buffers (~93 ms of audio).

## Pass criteria

Checks 1–7 and 11 must pass before considering the module
hardware-verified. 8–10 require a controlled pre-set DS3231 and
may be deferred until a real birthday during burn-in. 12 is
diagnostic-only.
