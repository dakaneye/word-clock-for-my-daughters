# Word Clock — Remaining Work

Live working list of what's left. The original phase-by-phase roadmap
lives in `docs/archive/specs/2026-04-15-activity-blocking-graph.md` for
historical reference; this file supersedes it.

## Status (2026-04-25)

| Workstream | State |
|---|---|
| Firmware Phase 1 (pure logic) | **Done** — tagged `phase-1-complete` |
| Firmware Phase 2 — all modules | **Done native-side** — `wifi_provision`, `buttons`, `display`, `rtc`, `ntp`, `audio` all shipped; `main.cpp` wires them together. 173/173 native Unity tests + 17/17 captive-portal pytest (Tier 1 structural + Tier 2 Playwright) green. Bench bring-up Steps 1-6 passed 2026-04-20; Step 7 (captive portal) shipped three scan fixes; awaiting on-device re-verification of the scan fixes. |
| Bench / printer prep | Bambu Studio installed, build123d venv ready (`enclosure/3d/`), PCB standoff STL generated. See `docs/hardware/3d-printing-setup.md`. |
| Face SVGs | **Delivered 2026-04-24** — Emory Maple 3.2 mm, Nora Walnut 3.2 mm. Nora's faceplate lost one O and part of a W during unboxing; super-glued back on — holds, imperfect but acceptable as character. |
| Frame SVGs | **Delivered 2026-04-24** — Emory Maple 6.4 mm, Nora Walnut 6.4 mm (bare shells, no cutouts). Nora's frame: corner 1 epoxied with Gorilla 5-min epoxy 2026-04-24 (initially failed at 5 min unclamp, re-glued with proper 30+ min clamp time); 3 corners + Emory frame still to go. |
| Back-panel SVGs | **Delivered 2026-04-24.** Emory Maple 3.2 mm, Nora Walnut 3.2 mm. **Two design bugs caught 2026-04-25 and resolved without re-cutting** — see "Decisions locked 2026-04-25". |
| PCB layout + PCBA | **Ordered from JLCPCB 2026-04-21** — 5 units, top-side PCBA for 35× WS2812B-V6 (LCSC C52917433), hand-soldering bottom. Photo Confirmation enabled. FedEx on customer account. In production. |
| Parts (ESP32, LEDs, MAX98357A, DS3231, speaker, USB breakout) | **Arrived 2026-04-17.** On hand, ready for bench work. |
| 1×6 right-angle female sockets for DS3231 / HW-125 | **Delivered 2026-04-24.** 10-pack, on hand. |
| Screws, standoffs, epoxy, grommets | **Delivered 2026-04-24.** M3 hex spacers, M3 machine screws, Gorilla 5-min epoxy (in use on Nora frame now), 6 mm rubber grommets — all on hand. **Note:** the M3 × 12 mm machine screws planned for back-panel attachment are now superseded by M3 wood screws; the existing screws can be repurposed. |
| Diffuser stack (Selens film only — acrylic held in reserve) | **Diffusion film validated 2026-04-26 as sole diffuser.** Bench test with single LED + 18 mm test pocket showed near-zero hot spots through film alone, but 5 mm lateral light bleed when opal acrylic was added (the acrylic's bead-scattered transmission spreads light laterally, defeating the channel walls). Decision: **drop the acrylic from the production stack**, channel walls go directly to film. Acrylic sheets ($37) shelved as backup if hot spots appear during full-clock assembly — fix would be per-pocket cuts + walls extending through the acrylic level. |
| Bambu Lab A1 3D printer | **Arriving 2026-04-24 to 2026-04-27.** Standoff STL pre-generated, waiting for printer + filament. |
| 3D internals — PCB standoffs + button caps | STL designs done; print when A1 lands. |
| 3D internals — light channel | **Deferred.** Design parametric in frame interior + PCB outline; need physical measurements (frame post-cure, PCB post-arrival) before locking the envelope. See "Decisions locked 2026-04-25" + Open questions. |

## Architecture (decisions locked this session)

- **Frame is a bare shell.** All exterior features live on the back panel.
- **Buttons pressed through the back panel** (PCB tact switches are bottom-side; "Hour / Min / Audio" labels engraved next to each hole).
- **Captive portal — custom implementation, not WiFiManager.** Raw
  `DNSServer` + `WebServer` + NVS-backed credentials. Avoids supply-chain
  dependency and gives full control over the per-kid palette. Shipped
  in `firmware/lib/wifi_provision/`.
- **USB path: captive cable.** A 3-6 ft Micro-USB-to-USB-C cable lives permanently inside the clock, plugged into the ESP32 module's micro-USB port, exiting through a back-panel hole at the bottom-right. No adapter, no pigtail, no panel-mount hardware. The hole gets drilled out from 6 mm to 16 mm at assembly so the connector overmold passes through; strain relief moves to an internal cable P-clip (the 16 mm hole is too loose for a grommet to grip). Historical rework record: `docs/archive/hardware/2026-04-17-usb-c-breakout-removal.md`. See "Decisions locked 2026-04-25" for the cable-port spec change rationale.
- **Back panel removable** via 4 × M3 × 1/2"–5/8" wood screws threaded directly into the 6.4 mm hardwood frame wall at the existing 4 mm-inset corner positions (pre-drilled ~2 mm pilots). The 4 mm panel inset places the screw centers in the wall thickness, so screws bite into wood. Wood-screws-into-hardwood handles ~10-15 open/close cycles before threads loosen — for a 5-year battery cadence over 40 years (8 cycles), well within tolerance. (Original plan — hex spacers epoxied into interior corners + machine screws — abandoned 2026-04-25; the 4 mm panel inset made spacer-in-corner placement impossible.)
- **Daughterboard orientation:** DS3231 + HW-125 plug in via right-angle 1×6 female sockets on B.Cu, so the daughterboard PCBs lie parallel to the main board extending away from the pad column (DS3231 body extends west, clear of ESP32; HW-125 body extends east into unused space). Right-angle geometry enforces orientation mechanically — no silkscreen marker needed. Swap was applied 2026-04-21; affected `J_RTC1` and `J_SD1` footprints in `hardware/word-clock.kicad_pcb` + `.kicad_sch`.

## Decisions locked 2026-04-25

- **Cable port: drill 6 → 16 mm at assembly + grommet + internal P-clip.** The original 6 mm grommet hole was too small for any USB connector overmold (~12-15 mm) to thread through. Splicing/desoldering rejected (failure point inside a 40-year build); split grommet rejected (sourcing). Resolution: drill the existing panels from 6 mm → 16 mm with a 5/8" step bit during Phase F2 assembly, install a loose-fit larger grommet for cosmetics, provide actual strain relief with a small cable P-clip screwed to the back-panel interior 2-3 cm in. Updated `enclosure/scripts/render_back_panel.py:USB_CABLE_EXIT_DIA_MM` to 16.0 for any future re-cuts. Updated assembly plan Phase F.
- **Back panel attachment: M3 wood screws into frame wall (no hex spacers, no Phase C).** Discovered `enclosure/scripts/render_back_panel.py:SCREW_CORNER_INSET_MM = 4.0` puts screw centers inside the 6.4 mm frame wall — incompatible with the original "hex spacer in interior corner" plan. Resolution: wood-screws-into-wall makes the 4 mm inset correct (screws bite into wood thanks to the wall placement). Pre-drill 2 mm pilots before driving 1/2"–5/8" M3 wood screws. Updated assembly plan: Phase C removed, Phase G rewritten. Updated `render_back_panel.py` fastener comment block.
- **Light channel: deferred until measurements.** A 2 mm error in the channel envelope causes light leak between word pockets; nominal frame interior (179.2) and PCB outline (177.8) numbers are paper specs subject to ±0.5 mm laser-cut tolerance + ±0.2 mm JLCPCB tolerance + wood expansion. Resolution: do not write `light_channel.py` until both numbers are measured with calipers. Channel design constraints: single piece (not segmented), envelope = (smaller frame inner) − 3 to 4 mm, passive PCB-edge locator tabs (not snap-fit, no fatigue), foam (1 mm) or putty seal at top edge against opal acrylic, 13×13 cell partition matching `firmware/lib/core/src/grid.cpp` word spans (per-kid NAME pocket variant), wall height TBD pending diffuser-stack validation.
- **Speaker mount: 2 × M3 hex spacers epoxied to back panel (no 3D-printed cradle).** Already settled 2026-04-21 — speaker has integral M3-compatible mounting flanges 37 mm apart. Captured here for completeness.
- **Frame-corner adhesive: Gorilla 5-min epoxy is acceptable substitute for Titebond III on box joints, but NOT for face-to-frame.** Box joints get most of their strength from finger-joint geometry; epoxy works given proper clamp time. Face-to-frame is a flat-on-flat perimeter joint where epoxy's 3 min open time fights alignment + makes squeeze-out harder to clean — Titebond III required for that step.

## Decisions locked 2026-04-26

- **Diffuser stack: film only, no opal acrylic.** Bench-tested 2026-04-26 with the printed 18 mm test pocket (`enclosure/3d/light_channel_test_pocket.py`) over a single lit WS2812B at full white brightness:
  - Pocket + opal acrylic + diffusion film → ~5 mm of lateral light bleed radiating outside the pocket perimeter (acrylic's bead-scattered transmission spreads light laterally inside the 3.2 mm sheet)
  - Pocket + film alone (no acrylic) → near-zero hot spots, diffusion looks fine
  - Conclusion: the acrylic was solving a problem (LED hot spots) that the film alone already handles at 18 mm of LED-to-film distance, AND the acrylic was the source of the lateral bleed that would have caused inter-letter light leakage. Drop it from the production stack.
  - Production light channel walls extend from PCB top directly to the diffusion film bottom (~17.84 mm tall, derived from frame depth 48 − back panel 3.2 − standoff 22 − PCB 1.6 − film 0.16). Foam strip at wall tops compensates for PLA flatness tolerance. Per-pocket acrylic + wall extension was an alternative considered; deferred as a backup mitigation if hot spots emerge during full-clock assembly.
  - Acrylic sheets ($37 sunk) shelved for future projects or backup. NOT discarded.
- **Light channel wall height locked at ~17.84 mm.** Was previously TBD pending diffuser-stack validation; now anchored by the no-acrylic decision and the geometric stack-up (PCB top to film bottom).

---

## Bench work — parts on hand (arrived 2026-04-17)

- [ ] **Inspect AITRIP module for a polyfuse on the micro-USB VBUS input.** Magnifier-level inspection near the micro-USB connector, looking for a small yellow/green 0603/0805 component in series with VBUS. If a 500 mA polyfuse is present, bridge it with solder or bypass it before commit — the full clock draw (~700 mA typical, up to 1.5-2 A peak) will trip it otherwise. Detail in `docs/archive/hardware/2026-04-17-usb-c-breakout-removal.md` step 3.3.
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

- [x] **ESP32 alone** — blink sketch. Confirms power + flashing path work on the real module. Verified 2026-04-20 via `firmware/test-sketches/01_blink`.
- [x] **I²C + DS3231** — scanner sketch finds address `0x68` (and `0x57` for the onboard AT24C32 EEPROM). Verified 2026-04-20 via `firmware/test-sketches/02_i2c_scan`.
- [x] **MicroSD** — SPI file-listing sketch lists `lullaby.wav` + `birth.wav` with correct byte counts. Verified 2026-04-20 via `firmware/test-sketches/03_sd_list`.
- [x] **MAX98357A + speaker** — 440 Hz tone audible. Verified 2026-04-20 via `firmware/test-sketches/04_tone`.
- [x] **WS2812B chain (1 LED via 74HCT245 level shifter)** — FastLED RGB cycle verified 2026-04-20 via `firmware/test-sketches/05_fastled`. Full 25-LED chain NOT tested — see §Bench verification gaps below.
- [x] **Full integration — all peripherals on one breadboard.** Complete end-to-end provisioning + display + audio + button flow verified 2026-04-20. Shipped 8 firmware fixes during bench-test (documented in `docs/superpowers/specs/2026-04-20-captive-portal-scan-fix-design.md` §Bench verification findings). See §Bench verification gaps below for what was NOT exercised.

## Bench verification gaps (2026-04-20)

Items intentionally not tested during the bench-bring-up session, for deferral or never-testable-on-breadboard reasons:

### Not testable without time-travel / seasonal conditions

- **Birthday auto-fire** — the audio module should auto-play `birth.wav`
  exactly at the birth minute (Emory: Oct 6 18:10) and exactly once per
  year (NVS-gated). Covered by unit tests in `test_audio_fire_guard` but
  not exercised on hardware.
- **Birthday rainbow + decor words** (HAPPY, BIRTH, DAY, NAME animated) — requires birthday or RTC fast-forward. Pure-logic renderer tested via
  `test_display_renderer_golden`.
- **Holiday palette** — date-specific accent colors on holidays (Halloween,
  Valentine's, etc.). Pure-logic tested; hardware verification awaits
  opportunistic date rollover or an RTC fast-forward drill.
- **Stale-sync amber tint** — >24 h since last NTP sync triggers amber
  overlay. Pure-logic tested; needs 24 h disconnected or hack the NVS
  last_sync value.

### Not testable without more hardware

- **Full 25-LED strip** — only 1 LED was physically wired for bench.
  Signal integrity, power delivery, color fidelity, and light-channel
  diffusion at scale are all unverified. **This is the single biggest
  coverage gap** — the answer to "does the clock actually look right"
  lives in this test. Ideally run after JLCPCB assembly lands.
- **PCB itself** — all bench work was breadboard. PCB is ready for
  submission but hasn't been fabbed.
- **Enclosure assembly** — face, frame, back panel, 3D-printed internals
  not yet integrated. See `docs/hardware/assembly-plan.md` for sequence.

### Skipped by deliberate choice (tested indirectly or low-risk)

- **WiFi drop + reconnect** — firmware logic exists
  (`wifi_provision::loop()` handles WL_CONNECTED → StaConnecting
  transition) but not exercised by unplugging the home router. Low
  priority; state-machine unit tests cover the logic.
- **SD card absent / corrupt** — `audio::begin()` logs an error but
  shouldn't crash. Not hardware-verified. Low-risk in production since
  the SD card is inside a sealed enclosure.
- **Validation failure UX** — the "Didn't connect. Check your password"
  inline error path. Hit accidentally once during bench and appeared to
  work; not verified with a deliberately-wrong-password test.
- **Multi-device captive AP** — `max_connection=4` firmware change is in
  place but never verified with 2+ devices simultaneously connected.
  Regression-safe since the prior default was 1 and we raised it.
- **AP 10-minute timeout** — AP shuts down after 10 min of no successful
  provisioning. Logic tested via state machine; never actually waited
  10 min on bench.
- **Confirmation 60-second timeout** — window to press Audio after
  submitting form. Logic tested; not hardware-verified.
- **30-day burn-in on Emory** — real-time, non-compressible. Gate on
  post-assembly, not bench.

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
- [x] **`audio`** — I²S + MAX98357A + microSD WAV playback.
      Shipped: `firmware/lib/audio/` with pure-logic WAV header
      validation (10 tests), Q8 gain scalar (4 tests), birthday
      auto-fire guard (8 tests), and an ESP32 adapter wrapping
      `driver/i2s.h` + `SD.h` + `Preferences` with a 2-state
      play-state machine. Button-press plays lullaby / stops
      playback; birth.wav auto-fires exactly once per year at
      the birth minute (NVS-gated). 22 native tests; full suite
      172/172. Both envs build clean. Spec:
      `docs/superpowers/specs/2026-04-18-audio-design.md`.
      Plan: `docs/superpowers/plans/2026-04-19-audio-implementation.md`.
      Hardware checklist: `firmware/test/hardware_checks/audio_checks.md`.
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
- [x] **Captive portal scan fix (2026-04-20)** — bench bring-up Step 7
      revealed three defects: `WIFI_AP` mode blocked `WiFi.scanNetworks`
      so the SSID dropdown stayed stuck on "Scanning for networks…",
      `max_connection=1` blocked multi-device provisioning, and the form
      JS fired `/scan` exactly once with no retry. Shipped three fixes
      (`WIFI_AP_STA`, `max_connection=4`, polling loop with 30 s timeout
      + Refresh button) under Tier 1 structural pytest (6) + Tier 2
      Playwright behavior (5) + 3 new hardware checks (§11-§13). Spec:
      `docs/superpowers/specs/2026-04-20-captive-portal-scan-fix-design.md`.
      Plan: `docs/superpowers/plans/2026-04-20-captive-portal-scan-fix-implementation.md`.
- [x] **HTML/CSS for the captive portal form** — embedded as PROGMEM via
      build-time generator `firmware/assets/captive-portal/gen_form_html.py`
      (5 pytests). Per-kid palette; runtime-substituted CSRF token + error
      message. Browser preview: `python gen_form_html.py --kid emory --preview`.
      Uses Arduino core `WebServer`, not SPIFFS / LittleFS.
- [x] **`main.cpp` state machine** — shipped as a side-effect of the
      Phase 2 module ships, no dedicated spec needed. `firmware/src/main.cpp`
      wires `wifi_provision::begin()` → `rtc::begin()` → `ntp::begin()` →
      `display::begin()` → `buttons::begin(...)` → `audio::begin(...)` in
      that order (tz set before first RTC read is load-bearing). `loop()`
      pumps each subsystem and renders only when
      `wifi_provision::seconds_since_last_sync() != UINT32_MAX` (free-runs
      on DS3231 during WiFi drops; blanks face before first-ever sync so
      pre-TZ UTC garbage is never shown). Button events route to
      rtc / audio / wifi_provision; the 10 s Hour+Audio combo triggers
      captive-portal reset. Mode-priority collisions (birthday + holiday
      + dim) were resolved inside `display::render` (priority chain in
      `docs/superpowers/specs/2026-04-17-display-design.md`), not here.
- [x] **SD-card filesystem layout — flat root.** Cards are per-kid (one per
      clock), bench testing runs one clock at a time, so `emory/`/`nora/`
      subdirs buy nothing. Files: `lullaby.wav` + `birth.wav` at the root.
      Firmware reads by fixed path; zero path-handling branches.
- [x] **Audio file format — WAV, 16-bit PCM, 44.1 kHz, mono, little-endian.**
      Design flipped from MP3 during audio-module spec pass
      (`docs/superpowers/specs/2026-04-18-audio-design.md`). Eliminates the
      MP3 decoder dependency (libhelix / ESP8266Audio / ESP32-audioI2S —
      ESP32-audioI2S required PSRAM we don't have; the others were decoder
      libraries we don't need once the file format is uncompressed).
      Collapses the audio adapter to an SD-read → I²S-write pump. File size
      is ~88 kB/s raw PCM (~18.5 MB total per card for a 3 min lullaby +
      30 s birth message); trivially inside any modern microSD's capacity.

## PCB finalization + order

- [x] **Daughterboard orientation solved via horizontal sockets (2026-04-21).** Swapped `J_RTC1` and `J_SD1` footprints from `PinHeader_1x06_P2.54mm_Vertical` to `Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Horizontal`. DS3231 uses `rot=0°` with un-mirrored coords (body extends west, clear of ESP32 at east); HW-125 uses `rot=180°` with Y-mirrored coords (body extends east). Pad global positions preserved in both — no routing changes needed. One cosmetic DRC warning ("does not match library copy") excluded, documented as KiCad GitLab issue #18399/#16501. Silkscreen markers no longer needed — geometry enforces orientation.
- [x] **Re-export gerbers + drill + CPL + BOM (2026-04-21).** Gerbers zipped as `hardware/word-clock-gerbers-2026-04-21.zip` (gitignored). PCBA files: `hardware/word-clock-cpl.csv` + `.xlsx`, `hardware/word-clock-pcba-bom.csv` + `.xlsx` (gitignored).
- [x] **Submit to JLCPCB (2026-04-21).** 5-unit order, top-side PCBA. LED part: WS2812B-V6 (LCSC C52917433, extended). Options: Tented vias (not epoxy), HASL lead-free, green solder mask, Remove Mark, Photo Confirmation YES, Depanel before delivery YES, Flying Probe Fully Test. Shipping: customer FedEx account. Total JLCPCB charge ~$50–70 + ~$30–80 FedEx freight + customs.

## 3D-printed internals (after printer calibrated + parts in hand)

- [x] **Install build123d** — venv at `enclosure/3d/.venv/`. See
      `enclosure/3d/README.md` for setup + per-part workflow.
- [x] **PCB standoff posts (×4)** — `enclosure/3d/pcb_standoff.py` generates
      the STL. 20 mm post + 2 mm glue flange + 2 mm locating stub (fits
      3.2 mm PCB hole). Run the script; slice in Bambu Studio when the
      printer lands.
- [x] **Button actuator caps (×3) — design shipped 2026-04-21** as `enclosure/3d/button_cap.py`. Plunger dia 3.52 mm + 1.5 mm extension measured on a physical switch; air-gap budget 15.5 mm derived from standoff (22 mm) - typical SW_PUSH_6mm body (5 mm) - plunger extension (1.5 mm). Stem deliberately 0.5 mm short of the air gap so the plunger floats free when released (better rattle than pre-activated). One un-measured assumption: switch body height above PCB (5 mm estimate). Verify on real hardware after PCB arrives and adjust `STEM_HEIGHT_MM` if needed. Slice + print when Bambu A1 lands.
- [x] **Speaker mount — no 3D-printed part after all.** Speaker has 2 integral M3-compatible mounting flanges 37 mm apart (measured 2026-04-21). Plan: glue 2× M3 × 10 mm hex spacers (from the corner-mount 10-pack already on order) to the back panel interior behind the vent, 37 mm apart. Speaker flange screws onto the spacer tops from the interior side. Brass-on-epoxy joint outlasts PLA-on-epoxy for the 40-year horizon, and the speaker stays removable for future replacement. Need to add 4× M3 × 6-8 mm short machine screws (2 per clock) to a future Amazon order.
- [ ] **Light channel honeycomb — design deferred until measurements.** Walls isolating each word's LED pocket. The design is substantially more nuanced than the original 1-line description here suggested — see "Decisions locked 2026-04-25" for the full constraint list. Summary: single piece, ~178 × 178 footprint sized off measured frame interior (not the nominal 179.2), height pending diffuser-stack validation, passive PCB-edge locator tabs, foam-or-putty top seal, 13×13 cell partition that exactly matches grid.cpp word spans (per-kid NAME pocket), every filler letter in its own walled cell with no LED. Script lives at `enclosure/3d/light_channel.py` once written. Pre-print measurement tasks documented in `docs/hardware/assembly-plan.md` Phase A.

## Diffuser stack (purchased, not printed)

Two-layer stack sitting between the face and the light channel. Neither
layer is 3D-printed — an earlier PETG-print iteration was rejected
because Chelsea's 2015 clock used PETG and still shows visible LED hot
spots.

- [x] **Design settled — adhesive film + opal cast acrylic, not single-layer.**
      Per `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md` §Diffuser:
      ~0.2 mm adhesive-backed diffusion film against the back of the
      face (also retains inner letter islands — A/B/D/O/P/Q/R/0/6/8/9)
      + 2-3 mm opal cast acrylic behind it for primary hot-spot
      elimination.
- [ ] **Diffuser stack validation** — during breadboard bring-up Step 5
      (single WS2812B lit), stack the film + opal acrylic at various
      distances (15 / 18 / 21 mm) in front of one lit LED and pick the
      channel depth that hides the die. Locks the light-channel
      honeycomb height before designing it. Also gives a last cheap
      A/B against scavenged tracing paper / vellum to confirm film is
      the right fix before the acrylic arrives.

## Supplies still to order

- [x] **M3 × 10 mm brass hex spacers — on hand 2026-04-24.** 10-pack;
      2 used per clock for speaker mount, 8 spare (4 originally allocated
      to back-panel attachment now unused — see Decisions locked 2026-04-25).
- [x] **M3 × 12 mm countersunk (or pan-head) brass machine screws — on hand 2026-04-24.** Originally for back-panel attachment; superseded by wood screws. Repurpose as needed or set aside.
- [x] **M3 × 6-8 mm machine screws — on hand 2026-04-24.** For mounting
      speakers to the epoxied hex standoffs on the back panel interior.
- [x] **2-part structural epoxy — on hand 2026-04-24.** Gorilla 5-min in use on Nora's frame.
- [ ] **M3 wood screws, 1/2" or 5/8" pan-head, 4-8 of each length** — for back-panel-into-frame-wall attachment. Pan-head simplest (sits proud); countersunk requires countersinking the 3.2 mm panel which is fiddly at 4 mm corner inset. Order both lengths to test bite depth.
- [ ] **5/8" step drill bit (HSS, 1/4" hex shank)** — for drilling cable port from 6 mm to 16 mm at assembly. Example: THINKWORK 3/16"-5/8" step bit on Amazon (~$10).
- [ ] **Cable P-clip, ~5 mm cable OD** — internal strain relief for the USB cable inside the back panel. Hardware store or Amazon (~$3 for a small bag).
- [x] **Self-adhesive black foam — skipped 2026-04-26.** Was planned as light-channel-to-diffuser-film seal. Bench test (single LED, no acrylic) showed near-zero hot spots without it; full-clock build will gamble on PLA-on-film contact being tight enough. Recovery if first-light shows edge bleed: add foam strips during one disassembly cycle (clock is serviceable).
- [ ] **Titebond III (or Type II/III wood glue)** — face-to-frame Phase E (do not substitute 5-min epoxy for this joint). Hardware store, ~$8 for an 8 oz bottle.
- [ ] Micro-USB-to-USB-C cable, 3-6 ft — 1 per clock (~$6 each).
- [x] **PLA filament — on hand 2026-04-21.** Black PLA for the light channel
      (required to block inter-pocket light bleed) + secondary color for
      internals. Ready for day-1 printing when the Bambu A1 arrives.
- [x] **Rubber grommets — on hand 2026-04-24.** 6 mm ID. With the cable port now spec'd at 16 mm and strain relief moved to a P-clip, these grommets are oversize for the original 6 mm hole but become loose-fit covers for the drilled-out 16 mm hole — usable but consider ordering ~16 mm grommets if cosmetics matter.
- [x] **1×6 right-angle female sockets (2.54 mm pitch) — ordered 2026-04-21.**
      10-pack from Amazon for both J_RTC1 (DS3231) and J_SD1 (HW-125).
      Arriving 2026-04-23. Extras cover spares for 40-year horizon.
- [x] **Adhesive-backed LED diffusion film — ordered.** Selens 3-pack, 7.9×11.82",
      0.16 mm thick, self-adhesive (Amazon ASIN B0D2D42XPG). One sheet per face
      + one spare for application practice. Thickness matches spec's 0.2 mm
      target within 0.04 mm. Ships with squeegee + cleaning wipes.
- [x] **Opal cast acrylic — ordered.** Acrylite Satinice White WD008 DF, 1/8"
      (3.2 mm), dual-sided matte with embedded light-diffusing beads (not
      surface frost — engineered specifically for LED hot-spot suppression).
      60% light transmission at this thickness per Evonik datasheet. Sourced
      from TAP Plastics, cut-to-size.

## Assembly + validation (Emory first, then Nora)

- [x] **Assembly plan document** — end-to-end glue-up + fasten-up sequence at `docs/hardware/assembly-plan.md`. Covers BOM, adhesives, 6-phase assembly (B/D/E/F/G — Phase C removed 2026-04-25), frame-squaring jig notes, post-cure measurement tasks. Major rewrite 2026-04-25 to reflect cable-port and back-panel-attachment decisions.
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

- **Frame interior + PCB outline measurements** — needed before the light
  channel script can be written. Frame interior measured with calipers
  on each cured frame, all 4 sides, top/middle/bottom of wall (12
  measurements per frame); PCB outline measured on each of the 5 JLCPCB
  boards when they arrive. Smallest frame inner determines channel
  envelope (= that minus 3-4 mm). If clearance < 0.4 mm, sand frame
  interior with P220.
- **Light channel wall height** — locked once diffuser-stack validation
  runs (single WS2812B + film + opal acrylic at varying heights:
  15 / 18 / 21 mm) per `TODO.md` Phase D bench task.
- **Light channel top-edge seal: foam vs putty.** Foam is cleaner (no
  mess at assembly) but requires applying a strip to every wall edge.
  Putty (the original prior-art solution) is messier but unforgiving
  of small gaps. Pick before assembling the first clock.
- **Light channel PCB-edge fastening: passive locator tabs vs snap-fit
  clips.** Passive recommended (no PLA fatigue over 40-year service
  cycles); snap-fit retains channel without compression but stresses
  cantilevers. Pick during channel design.
- **Light channel as one print vs print-and-glue assembly.** The full
  178 × 178 × 18 mm part is 6-8 hours on the A1 and a single failed
  print wastes that time. Tradeoff: one piece = perfect seal continuity
  but high failure cost. Pick after first test print of a sample
  region (NOT a quadrant — see Decisions locked 2026-04-25 on why
  quadrant testing doesn't validate the seal).
- **Back-panel screw-head style: pan-head vs countersunk wood screws.**
  Pan-head sits proud (simpler, less fiddly at the 4 mm inset close to
  the corner); countersunk requires countersinking the 3.2 mm panel.
  Pick during assembly; order both lengths to test.
- **16 mm grommet cosmetics.** Currently planning to use the on-hand
  6 mm grommets as loose-fit covers for the drilled-out 16 mm hole.
  If cosmetics matter, order properly-sized 16 mm grommets (~$3).
