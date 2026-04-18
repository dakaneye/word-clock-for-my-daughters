# Word Clock — Remaining Work

Live working list of what's left. The original phase-by-phase roadmap
lives in `docs/archive/specs/2026-04-15-activity-blocking-graph.md` for
historical reference; this file supersedes it.

## Status (2026-04-17)

| Workstream | State |
|---|---|
| Firmware Phase 1 (pure logic) | **Done** — tagged `phase-1-complete` |
| Firmware Phase 2 — `wifi_provision` + `buttons` | **Done native-side** — 89 native tests + 6 Python tests green, code-reviewed to A-grade (137/150). Awaiting hardware-flash verification. |
| Bench / printer prep | Bambu Studio installed, build123d venv ready (`enclosure/3d/`), PCB standoff STL generated. See `docs/hardware/3d-printing-setup.md`. |
| Face SVGs | **Ordered from Ponoko** — Emory Maple 3.2 mm, Nora Walnut 3.2 mm |
| Frame SVGs | **Ordered from Ponoko** — Emory Maple 6.4 mm, Nora Walnut 6.4 mm (bare shells, no cutouts) |
| Back-panel SVGs | **Ordered from Ponoko** — Emory Maple 3.2 mm, Nora Walnut 3.2 mm |
| PCB layout | Cermant USB-C removed. Mounting holes verified. **Awaiting final review + JLCPCB submit** (gated on breadboard bring-up). |
| Parts (ESP32, LEDs, MAX98357A, DS3231, speaker, USB breakout) | **Arriving today (2026-04-17).** |
| Bambu Lab A1 3D printer | **Arriving today.** Standoff STL pre-generated, waiting for printer + filament. |
| 3D internals (button caps, speaker cradle, light blocker) | Not started — blocked on physical component measurements. |

## Architecture (decisions locked this session)

- **Frame is a bare shell.** All exterior features live on the back panel.
- **Buttons pressed through the back panel** (PCB tact switches are bottom-side; "Hour / Min / Audio" labels engraved next to each hole).
- **Captive portal — custom implementation, not WiFiManager.** Raw
  `DNSServer` + `WebServer` + NVS-backed credentials. Avoids supply-chain
  dependency and gives full control over the per-kid palette. Shipped
  in `firmware/lib/wifi_provision/`.
- **USB path: captive cable.** A 3-6 ft Micro-USB-to-USB-C cable lives permanently inside the clock, plugged into the ESP32 module's micro-USB port, exiting through a 6 mm grommeted hole at the bottom-right of the back panel. No adapter, no pigtail, no panel-mount hardware. See `docs/hardware/usb-c-breakout-removal-guide.md`.
- **Back panel removable** via 4 × M3 brass corner screws threading into hex spacers epoxied into the frame corners. Unlimited removal cycles; serves the 40-year CR2032 replacement cadence.
- **Daughterboard orientation:** DS3231 + HW-125 install with battery holder / SD slot facing the back panel (silkscreen markers on the PCB will enforce this).

---

## This week — blocked on parts arrival (2026-04-17)

- [ ] **Inspect AITRIP module for a polyfuse on the micro-USB VBUS input.** Magnifier-level inspection near the micro-USB connector, looking for a small yellow/green 0603/0805 component in series with VBUS. If a 500 mA polyfuse is present, bridge it with solder or bypass it before commit — the full clock draw (~700 mA typical, up to 1.5-2 A peak) will trip it otherwise. Detail in `docs/hardware/usb-c-breakout-removal-guide.md` step 3.3.
- [ ] **Caliper-measure tallest bottom-side components.** Spot-check the pinout.md table's [MED]-confidence daughterboard heights (MAX98357A, DS3231 coin-cell holder, microSD slot). Update the standoff + light-channel depth budget if any value is off by > 2 mm.
- [ ] **Pre-power smoke test protocol.** Run through `docs/hardware/pinout.md` §Pre-power smoke test: multimeter check of the USB-C breakout, DS3231 battery-charging resistor removal, MAX98357A pin sanity, then ESP32-alone flash/run, add peripherals one at a time. ~15 minutes total.
- [ ] **Re-verify `wifi_provision_checks.md` + `buttons_checks.md`** against
      today's C1 fix (confirm_audio now actually triggers WiFi). Behavior
      externally identical, but worth eyeballing before running them on
      real hardware.
- [ ] **Optional: day-1 runbook** — single chronological doc threading
      smoke test → Bambu A1 calibration → first standoff print → ESP32
      flash → step-by-step breadboard bring-up. Currently spread across
      3 docs (`pinout.md`, `3d-printing-setup.md`, `breadboard-bring-up-guide.md`).
      Skippable — the 3 docs work fine side-by-side.

## Breadboard bring-up (after smoke test passes)

Per-peripheral isolation tests first, then the Phase 2 firmware modules that
wire everything together. Full bench procedure with wiring + test sketches:
`docs/hardware/breadboard-bring-up-guide.md`. Pre-power smoke-test protocol
lives in `docs/hardware/pinout.md`. See
`docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md` §Phase 2.

- [ ] **ESP32 alone** — blink sketch. Confirms power + flashing path work on the real module.
- [ ] **I²C + DS3231** — scanner sketch finds address `0x68`.
- [ ] **MicroSD** — SPI file-listing sketch.
- [ ] **MAX98357A + speaker** — 440 Hz tone test.
- [ ] **WS2812B chain (1-3 LEDs via 74HCT245 level shifter)** — FastLED rainbow. Tests the 3.3 V → 5 V level-shift path that the PCB uses.
- [ ] **Full integration — all peripherals on one breadboard.** All Phase 2 modules (below) running together. Real de-risk before submitting the PCB.

## Phase 2 firmware modules (built during breadboard bring-up)

Phase 1 produced pure-logic modules that don't touch hardware (`time_to_words`,
`dim_schedule`, holiday/birthday triggers, grid rendering). Phase 2 is
everything that actually drives the hardware and WiFi. Each remaining
module below needs a short spec + plan (same shape as
`docs/superpowers/specs/2026-04-16-buttons-design.md` +
`docs/archive/plans/2026-04-16-buttons-implementation.md`) before coding.
The master plan at `docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md`
is intentionally general from Phase 2 onward — each module gets its own
spec+plan pair when its turn comes up. Modules to write:

- [x] **`display`** — FastLED driver + Phase 1 renderer. Shipped:
      `firmware/lib/display/` with pure-logic renderer (priority
      chain birthday-rainbow > holiday > amber-stale-sync > warm-white,
      dim multiplier last), overflow-safe 60 s rainbow on decor words,
      PALETTE_MAX_RGB_SUM=700 invariant, FastLED adapter with 1.8 A
      runtime power cap. 35 native tests; full suite 124/124. Spec:
      `docs/superpowers/specs/2026-04-17-display-design.md`. Hardware
      checklist: `firmware/test/hardware_checks/display_checks.md`.
- [x] **`rtc`** — DS3231 read/write via RTClib. Shipped:
      `firmware/lib/rtc/` with pure-logic wrap math
      (`advance_hour_fields`, `advance_minute_fields`) and pure-logic
      UTC-fields→epoch (`utc_epoch_from_fields`, Howard Hinnant
      days_from_civil), plus an ESP32 adapter. 16 native tests (5
      epoch + 11 advance); full suite 140/140. UTC stored on chip;
      libc `localtime_r` applies POSIX TZ set by wifi_provision.
      Spec: `docs/superpowers/specs/2026-04-17-rtc-design.md`.
      Plan: `docs/superpowers/plans/2026-04-17-rtc-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/rtc_checks.md`.
- [x] **`ntp`** — NTPClient on boot + every 24 hours (±30 min jitter);
      uses RTC continuously between syncs. Shipped: `firmware/lib/ntp/`
      with pure-logic validation (epoch floor 2026-01-01) + scheduler
      (capped exponential backoff 30s→30m, jittered 24h cadence) and
      ESP32 adapter wrapping NTPClient + WiFi.hostByName. 10 native
      tests; full suite 150/150. Server: `time.google.com`. Spec:
      `docs/superpowers/specs/2026-04-17-ntp-design.md`. Plan:
      `docs/superpowers/plans/2026-04-17-ntp-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/ntp_checks.md`.
- [ ] **`audio`** — I²S + MAX98357A. MP3 decode from microSD. Play / stop
      behavior: press once to play lullaby, press during playback to stop.
      Volume is fixed in firmware and tuned during assembly. Needs a spec
      (ESP8266Audio vs. libhelix library choice, volume curve, play-state
      machine with buffer-underrun handling).
- [x] **`buttons`** — debounced tact-switch input on GPIO 14 / 32 / 33
      (hour / minute / audio). Shipped: `firmware/lib/buttons/` with
      `Debouncer` + `ComboDetector` pure-logic state machines (13 native
      Unity tests) and an ESP32 adapter. 10 s Hour+Audio combo calls
      `wifi_provision::reset_to_captive()`. Spec:
      `docs/superpowers/specs/2026-04-16-buttons-design.md`. Manual
      hardware checklist: `firmware/test/hardware_checks/buttons_checks.md`.
- [x] **`wifi_provision` (captive portal)** — first-boot WiFi setup.
      Decision: custom `DNSServer` + `WebServer` (not WiFiManager) for
      UI + supply-chain control. Shipped: `firmware/lib/wifi_provision/`
      with pure-logic state machine (13 transitions, native-tested), form
      parser, credential validator, 8-entry TZ table, plus ESP32 adapters
      for NVS, DNS hijack, web server, and lifecycle. Spec:
      `docs/superpowers/specs/2026-04-16-captive-portal-design.md`.
      Hardware checklist: `firmware/test/hardware_checks/wifi_provision_checks.md`.
- [x] **HTML/CSS for the captive portal form** — embedded as PROGMEM via
      build-time generator `firmware/assets/captive-portal/gen_form_html.py`
      (5 pytests). Per-kid palette; runtime-substituted CSRF token + error
      message. Browser preview: `python gen_form_html.py --kid emory --preview`.
      Uses Arduino core `WebServer`, not SPIFFS / LittleFS.
- [ ] **`main.cpp` state machine** — boot → captive portal if no creds → NTP
      sync → normal clock loop, with holiday / birthday / bedtime-dim modes
      and audio button handling interleaved. Integration spec — pins
      mode-priority order when multiple triggers align (birthday +
      holiday + bedtime-dim all hit at 8:30 PM on Halloween of Emory's
      birthday year, etc.).
- [x] **SD-card filesystem layout — flat root.** Cards are per-kid (one per
      clock), bench testing runs one clock at a time, so `emory/`/`nora/`
      subdirs buy nothing. Files: `lullaby.mp3` + `birth.mp3` at the root.
      Firmware reads by fixed path; zero path-handling branches.
- [x] **Audio file format — MP3, 128 kbps CBR, mono, 44.1 kHz.**
      CBR over VBR because ESP32 MP3 decoder libs (ESP8266Audio / libhelix)
      handle CBR more reliably — VBR can hiccup on sample-rate transitions.
      Mono because single speaker. 128 kbps is transparent for voice + a
      simple lullaby, ~1 MB/min, well under microSD capacity. 44.1 kHz
      is standard and within MAX98357A's 8–48 kHz @ 16-bit envelope.

## PCB finalization + order

- [ ] **Add silkscreen markers for daughterboard orientation.** Print "BATTERY →" next to `J_RTC1` and "SD →" next to `J_SD1` on the main PCB silkscreen. Prevents assembly-time orientation errors.
- [ ] **Optional: revise PCB if breadboard reveals pin conflicts or noise issues** (documented failure modes in pinout.md).
- [ ] **Re-export gerbers + drill + CPL + BOM.** Same process as `docs/hardware/kicad-rework-guide.md` step 2.11.
- [ ] **Submit to JLCPCB** for fab + assembly. 5-unit MOQ. Lead time 2-3 weeks realistic + shipping.

## 3D-printed internals (after printer calibrated + parts in hand)

- [x] **Install build123d** — venv at `enclosure/3d/.venv/`. See
      `enclosure/3d/README.md` for setup + per-part workflow.
- [x] **PCB standoff posts (×4)** — `enclosure/3d/pcb_standoff.py` generates
      the STL. 20 mm post + 2 mm glue flange + 2 mm locating stub (fits
      3.2 mm PCB hole). Run the script; slice in Bambu Studio when the
      printer lands.
- [ ] **Button actuator caps (×3)** — tiered cylinders that slide through the 6.5 mm panel holes, bridging the ~14 mm air gap to the tact switch plungers. ~30 min part.
- [ ] **Speaker cradle** — cup with 2 mount-tab screw holes matching the speaker (~37 mm tab spacing, measure exactly from the physical part), glued to the back panel interior behind the vent. Worth printing vs gluing the speaker directly because the speakers are cheap ($2.50 each, 4-pack) while a replacement back panel is ~$50 — cradle keeps the speaker swappable without wasting wood. ~1-2 hrs to design.
- [x] **Diffuser — PETG stack, not single-layer.** Settled: frosted PETG
      alone does not adequately diffuse (Chelsea's 2015 clock used PETG
      and still showed visible hot spots at the LED face). Go with the
      spec's recommended stack: adhesive-backed diffusion film against the
      back of the face (also retains inner letter islands) + 2-3 mm opal
      cast acrylic behind it for primary hot-spot elimination. The
      `diffuser material test` becomes a stack-validation test once the
      printer + materials are on hand.
- [ ] **Diffuser stack validation** — once printer + materials land, test
      the film + opal stack at various light-channel depths (15/18/21 mm)
      on a single LED to lock the channel height before printing the full
      honeycomb.
- [ ] **Light channel honeycomb** — walls isolating each word's LED pocket, 35 LED pockets, snap fits to PCB, height ~18 mm. Substantial part; budget several hours of iteration. May be easier in Fusion/Onshape than scripted.

## Supplies still to order

- [ ] M3 × 10 mm brass hex spacers, 5 mm AF, F-F — 10-pack (~$8)
- [ ] M3 × 12 mm countersunk (or pan-head) brass machine screws — (~$5)
- [ ] 2-part structural epoxy (JB Weld or E6000) — (~$8)
- [ ] Micro-USB-to-USB-C cable, 3-6 ft — 1 per clock (~$6 each)
- [ ] PLA filament — 1 kg black (required for light channel — see laser-cut
      spec §Light channel material override) + 1 kg any color for other
      internals. Bambu PLA Basic is the zero-thinking default. (~$40)
- [ ] Rubber grommet, 6 mm inner diameter (or sized to the cable OD) — (~$3)

## Assembly + validation (Emory first, then Nora)

- [x] **Assembly plan document** — end-to-end glue-up + fasten-up sequence at `docs/hardware/assembly-plan.md`. Covers BOM, adhesives, 7-phase assembly, frame-squaring jig notes.
- [x] **Cardboard dry-run** — skipped. Real hardwood already ordered from
      Ponoko (shipping); cardboard test would have validated kerf + box-
      joint fit but the per-sheet cost wasn't worth the 2-3 week delay.
- [ ] **Emory unit — first light, glue-up.**
- [ ] **Emory 30-day burn-in.** Real-time calendar task — cannot be compressed. Watch for thermal, audio crackle, RTC drift, WiFi reconnection.
- [ ] **Nora unit — build with fixes baked in from Emory.**

## Parallel tracks (low-brain, re-doable, non-blocking)

- [ ] **Voice-memo recording** — Dad's voice at birth minute (6:10 PM Emory, 9:17 AM Nora). Re-recordable up to delivery in 2030 / 2032, so v1 now is fine.
- [ ] **Lullaby recording** — one song per kid. Same re-record window.

## Open questions (to resolve when the situation demands)

- Light channel depth vs standoff height tradeoff — tune once real
  component heights are measured + diffuser-stack validation runs.
- Back-panel screw-head style: countersunk vs pan-head. Drives whether we
  countersink the 3.2 mm panel (countersunk) or let heads sit proud
  (pan-head, simpler). Pick during assembly.
