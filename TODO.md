# Word Clock — Remaining Work

Living working list of what's left. Supersedes the archived phase-by-phase
roadmap (`docs/archive/specs/2026-04-15-activity-blocking-graph.md`).

## Status update (2026-07-12) — EMORY ASSEMBLED AND RUNNING

Board #1 is inside the enclosure, closed up (carrier plate dry-fit, no
glue), on the wall-of-Dad's-desk running bench firmware: WiFi + NTP + RTC
+ display + audio all verified on the assembled clock. Physical Audio
button proven by ear; Hour/Min sim-verified (identical circuit; physical
press declined to avoid skewing time — accepted). Night dim raised
0.10 → 0.25 after viewing through the real face (2026-07-12).

**Emory's remaining punch list:**
- [ ] Captive-portal persist bug (below) — fix, then strip TEMP bench code
      from `main.cpp`, flash production, re-provision via the real portal
      (doubles as the Hour+Audio 10 s combo's first physical test)
- [ ] Decide wall-hang vs shelf placement — no hanging provision exists,
      and the clock can't hang flush (button caps sit 2.5 mm proud of the
      back panel; cable exits the back). Shelf needs nothing; wall needs
      a French cleat or equivalent standoff
- [ ] Record real audio (parallel track below), then load it with
      `firmware/tools/sd_load.py` — no screws; needs the cable + home WiFi (spec:
      docs/superpowers/specs/2026-07-13-sd-loader-design.md). HARD
      DEADLINE: before Oct 6 (first live birthday fire). Verify by ear:
      Audio button for lullabies, bench 'b' for birth.wav.
- [ ] 30-day burn-in, then start Nora
- [ ] G2 note: first-light bleed at the U1/U2/SW2 bridge keyholes not yet
      formally assessed — eyeball at night and tape on the PCB if needed

**Nora's build punch list (start after burn-in):**
- [ ] Hand-solder THT on board #2 + pre-power smoke test (pinout.md)
- [ ] Abbreviated bring-up: flash `nora` env with bench backdoor, run the
      full-system check (WiFi → NTP → RTC → words → audio)
- [ ] Print: carrier plate + 3 caps + 2 spares (STLs current; snap size
      7.9 validated on this printer 2026-07-12), verify her light channel
      print + fit (STL final; confirm it was printed)
- [ ] Assemble per `docs/hardware/assembly-plan.md` (now carrier-era
      current) — watch daughtercard seating at closeout (Emory's RTC
      unseated during assembly)
- [ ] Her audio WAVs (9:17 AM birth minute), her burn-in

## Status (2026-06-13)

| Workstream | State |
|---|---|
| **Firmware** | **Done.** 183/183 native Unity tests green; both `emory`/`nora` ESP32 envs build. Drives the **63-LED board** via per-word `LedSpan{start,count}` mapping (long words light 2–3 adjacent LEDs), audio is a **two-track lullaby playlist** (`lullaby1.wav` → `lullaby2.wav`, auto-advancing) with the **birthday message interrupting** an in-progress lullaby, LED power capped at **1700 mA** (FastLED, sized for a 3 A USB-C supply). |
| **PCB** | **63-LED board ordered from JLCPCB 2026-06-07** — 5 boards, full PCBA (top-side, Economic). Only the 63 SMD LEDs are machine-placed; **all THT is hand-soldered on arrival.** LED substituted to **WS2812B-V6 (LCSC C52917433)** — see `hardware/fab/README.md`. Canonical KiCad source + as-ordered fab package now committed. **Awaiting delivery.** |
| **April 35-LED board** | Fabbed but **obsolete** — physical boards exist, superseded by the 63-LED respin. Not used. |
| **Enclosure** | **Built.** Frames glued (both clocks), faces glued to frames, light channel printed. Remaining enclosure work is integrating the PCB into it (see finish line). |
| **Audio assets** | **Not yet recorded.** Need `lullaby1.wav` + `lullaby2.wav` + `birth.wav` per kid, 16-bit PCM 44.1 kHz mono LE, at SD root. Re-recordable up to delivery (2030/2032) — non-blocking. |
| **Supplies** | A handful of assembly consumables still to buy — see "Still to buy". |

## The finish line (board-gated)

Everything below waits on the 63-LED boards arriving. Once they do, in order:

1. **Hand-solder THT** on each board: ESP32 module, RTC / SD / amp right-angle
   headers, U2 (74HCT245), R1, C2 + the 100 nF decoupling caps, buttons.
2. **Pre-power smoke test** per `docs/hardware/pinout.md` §Pre-power smoke test:
   multimeter checks, inspect the AITRIP module for a VBUS polyfuse (bridge if
   present — see `docs/archive/hardware/2026-04-17-usb-c-breakout-removal.md`),
   DS3231 trickle-charge resistor stays in place (locked 2026-04-20, see
   `docs/hardware/pinout.md` §1 — inert at 3.3V, mitigated by the 5-year
   battery-swap cadence), MAX98357A pin sanity.
3. **Bench checks on the real board** (details below).
4. **Flash `emory` firmware**, verify captive-portal provisioning, time-to-words,
   buttons (hour/min/audio + the 10 s Hour+Audio captive-reset combo), and audio.
5. **✅ DONE 2026-07-06 — light channel v4 printed (PETG) and fits board #1.**
   The 2026-06 print
   failed dry-fit on board #1 (2026-07-05): its pockets predate the 63-LED
   layout, and its flat underside rides up on THT pin stubs.
   `enclosure/3d/light_channel.py` (v4, first-principles rework) keeps the
   original pure-void honeycomb and adds essentially nothing: blind relief
   cavities (5 mm deep) at every pin, one merged slot per daughtercard
   header row (bent-flat socket tails spread laterally), and 25 low wall
   JOGS — 2 mm ribs only 6 mm tall that route walls around pins, straight
   wall untouched above. 9 small notches remain at the U1/U2 pin fences
   and SW2 (build prints WARNINGs; no thin wall can cross a 2.54 mm pin
   fence) — assess at G2, black tape on the PCB if visible. Pad/LED data:
   `hardware/word-clock-tht-pads.csv` + `word-clock-led-pos.csv`
   (regenerate via `enclosure/3d/export_tht_pads.py`). Orientation: the
   chamfered corner (tab on the coupon) goes to the D1 / IT-IS corner; do
   NOT flip the part in the slicer; supports OFF. Workflow: print the
   coupon first (`--coupon`), dry-fit, then the full part: black PETG,
   PCB-side down, brim, ~6-8 h. Old prints superseded. Note
   `hardware/word-clock-all-pos.csv` is stale (35-LED era) — regenerate or
   delete when convenient.
6. **Integrate the PCB into the (already-built) enclosure**: PCB standoffs, seat
   the light channel, apply diffusion film to the face back, drill the cable port
   6 → 16 mm, install the P-clip strain relief, fit the back panel (M3 wood
   screws into the frame wall). Sequence: `docs/hardware/assembly-plan.md`.
7. **Record + load the audio WAVs** (can happen any time — parallel track).
8. **Emory 30-day burn-in** — real-time; watch thermal, audio crackle, RTC drift,
   WiFi reconnection.
9. **Build Nora** with every fix learned from Emory baked in.

## On delivery — bench checks (the biggest coverage gap)

All firmware bench work so far was on a breadboard with a single LED. The real
board closes the "does it actually look right" question. Sketches live in
`firmware/test/hardware_checks/`.

- [x] **D1 glows RED / GRB color order** — `display_checks.md`. Confirms the
      level-shifter + first-LED data path and the GRB color order before trusting
      anything else. *Passed 2026-07-04 on board #1 via the 05_fastled sketch
      (63-LED rev): D1 red alone, full 63-LED chain walk clean, all-on dim red
      at 600 mA laptop cap. WS2812B-V6 substitution confirmed GRB.*
- [~] **Full 63-LED all-on brightness + power** — WAIVED 2026-07-12 with the
      clock assembled: production firmware's worst frame (birthday rainbow +
      decor) lights ~30 LEDs ≈ 40% of the 1700 mA cap, and the 2026-07-04
      600 mA all-on bench check plus assembled-clock operation showed no
      droop. Revisit only if brightness ever increases.
- [x] **Per-word LED walk** — confirm the firmware `kSpans` table matches the
      physical chain (each word lights its own letters, no neighbors). This is
      the on-hardware check behind the `test_spans_partition_the_chain` unit test.
      *Passed 2026-07-05 on board #1: live-rendered frames at 13:29/13:30 matched
      a host-compiled oracle (same pure-logic sources) index-for-index, and the
      2026-07-04 chain walk proved every physical LED lights at its index.
      Letter-alignment through the actual face still gets eyeballed at enclosure
      integration (assembly-plan G2).*
- [ ] **Captive-portal persist bug (OPEN, needs phone)** — two bench attempts on
      2026-07-05 (submit → "waiting for audio" → press Audio) ended with NVS still
      empty and the clock back in ApActive after reboot. Bench work continued via
      the TEMP `BENCH_BACKDOOR_SSID` credential inject in `main.cpp`, but the real
      portal flow MUST be debugged with a phone before Emory ships. Suspects: the
      per-AP submit rate limit (b988959) blocking re-submits, or the 60 s
      confirmation timeout racing the user. Repro: watch serial during a live
      phone provisioning session.
- [ ] **WiFi radio isolation** — the softap-only sketch (committed) if provisioning
      misbehaves, to separate radio bring-up from the rest.

## Still to buy

**Board THT passives — never purchased (gate the first bench check; order now).**
The April inventory predates the 35→63-LED respin and the on-PCB decoupling
work, so these were never bought. Quantities are for two clocks.

- [ ] **74HCT245, DIP-20 ×2** (buy 4–5 for spares) — the LED level shifter; D1
      won't light without it. HCT family specifically, not HC.
- [ ] **100nF ceramic disc caps ×44** (buy a 100-pack) — 22 per board, one per
      IC Vcc. Footprint `C_Disc_D5.0mm_W2.5mm_P5.00mm`.
- [ ] **1000µF / 10V radial electrolytic ×2** (buy spares) — C2 bulk cap; skipping
      it risks frying LEDs on first power-up.
- [ ] **300 Ω axial resistor** — R1 LED-data series resistor; verify the salvaged
      assortment has it or buy a pack (470 Ω is an acceptable fallback).

**Enclosure consumables (shipping lead time — order now).**

- [ ] **M3 wood screws, 1/2" + 5/8" pan-head** — back-panel-into-frame-wall.
- [ ] **5/8" step drill bit (HSS, 1/4" hex)** — drill cable port 6 → 16 mm.
- [ ] **Cable P-clip, ~5 mm OD** — internal USB strain relief.
- [ ] **Titebond III** — face-to-frame joint (do NOT substitute 5-min epoxy here).
- [ ] **USB-C-to-USB-C cable, 3–6 ft** — one per clock (the captive cable;
      the delivered ESP32 modules turned out to have USB-C, not micro-USB —
      confirmed on the bench 2026-07-07).

## Parallel tracks (non-blocking, re-doable)

- [ ] **Voice-memo recording** — Dad's voice for the birthday message (Emory
      6:10 PM, Nora 9:17 AM). Re-recordable up to delivery.
- [ ] **Lullaby recording** — two tracks per kid (`lullaby1.wav`, `lullaby2.wav`).

## Architecture — locked decisions (reference)

- **Frame is a bare shell**; all exterior features live on the back panel.
- **Buttons press through the back panel** (PCB tact switches are bottom-side;
  Hour / Min / Audio labels engraved next to each hole).
- **Captive portal is a custom `DNSServer` + `WebServer` + NVS** implementation
  (not WiFiManager) — supply-chain control + per-kid palette. Shipped in
  `firmware/lib/wifi_provision/`.
- **USB: captive USB-C-to-USB-C cable** (module port is USB-C) permanently inside the clock, exiting
  a back-panel hole drilled 6 → 16 mm at assembly; strain relief via internal
  P-clip, not the grommet.
- **Back panel removable** via 4 × M3 wood screws into the 6.4 mm hardwood frame
  wall at the 4 mm-inset corners (pre-drill ~2 mm pilots). Handles the 40-year
  CR2032-replacement cadence (~8 open/close cycles).
- **Daughterboards** (DS3231, HW-125) plug in via right-angle 1×6 female sockets
  so they lie parallel to the main board; geometry enforces orientation.
- **Diffuser stack is film-only** (no opal acrylic) — bench-validated 2026-04-26;
  acrylic caused ~5 mm lateral light bleed. Acrylic shelved as a backup.
- **Speaker mount**: 2 × M3 hex spacers epoxied to the back panel (no printed
  cradle); the speaker's integral flanges screw onto the spacer tops.
- **Power/ground plane split**: B.Cu is a solid +5V plane; GND rides the F.Cu
  zone with short F.Cu jumpers (no B.Cu GND plane). See CLAUDE.md.

## Open decisions

- **Master BOM is stale + untracked.** CLAUDE.md calls `hardware/word-clock.csv`
  the source of truth, but it is gitignored AND stale — it lists the old 35-LED
  topology (D1–D35, includes a D7) with empty LCSC columns. The only current BOM
  is the LED-only `hardware/fab/word-clock-bom.csv` (63 designators, D7 removed).
  Regenerate a complete 63-LED master BOM from the KiCad source (with LCSC part
  numbers for a JLC reorder), then track it.
- ~~**Light-channel top-edge seal**~~ — DECIDED 2026-07-07: none. Channel
  walls press directly on the film; foam/putty held only as a G2 retrofit
  if bleed ever shows.
- **Back-panel screw head** — pan-head (sits proud, simple) vs countersunk
  (needs countersinking the 3.2 mm panel). Order both lengths, decide at assembly.
- **Wall-hang vs shelf** — see the Emory punch list above; undecided and
  physically consequential (proud button caps + rear cable exit).

## Coverage gaps carried forward

Not exercisable until hardware/dates allow:

- **Birthday auto-fire** at the birth minute (NVS-gated, once/year) — unit-tested
  (`test_audio_fire_guard`), not hardware-verified.
- **Birthday rainbow + decor words** (HAPPY/BIRTH/DAY/NAME) — needs the date or an
  RTC fast-forward; renderer is golden-tested.
- **Holiday palette** + **stale-sync amber tint** (>24 h since NTP) — pure-logic
  tested; need date rollover / 24 h disconnected.
- **WiFi drop + reconnect**, **SD absent/corrupt**, **validation-failure UX**,
  **multi-device captive AP**, **AP 10-min timeout**, **confirmation 60 s
  timeout** — state-machine tested; low priority, verify opportunistically.
- **30-day burn-in on Emory** — real-time, gated post-assembly.
