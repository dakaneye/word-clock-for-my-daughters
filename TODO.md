# Word Clock — Remaining Work

Source of truth for what's left on the project. See
`docs/superpowers/specs/2026-04-15-activity-blocking-graph.md` for the
original phase-by-phase roadmap; this file is the live working list.

## Status (2026-04-16)

- Firmware logic: done, 35 tests green, tagged `phase-1-complete`
- Face SVGs: done, ordered from Ponoko (Emory maple + Nora walnut, 3.2mm)
- Frame SVGs: done, ordered from Ponoko (maple + walnut, 6.4mm, bare shells — no cutouts)
- PCB layout: "pretty final" in KiCad, **not yet ordered** from JLCPCB
- Back panel, 3D internals, diffuser: **not started**
- Parts (ESP32, LEDs, MAX98357A, DS3231, speaker, USB-C breakout): en route, expected 2026-04-17 to 2026-04-20
- Bambu Lab A1 printer: en route, same window

## Architecture decisions landed this session

- Frame is a pure shell. All exterior features (buttons, USB, speaker vent, dedication) live on the **back panel**.
- Buttons are pressed through the back panel, not the side — forced by the PCB (tact switches are bottom-side, vertical through-hole, at X=168.5).
- USB-C port on the back panel is wired internally by a USB-C-female-to-micro-USB-male panel-mount pigtail that plugs into the ESP32 module's native micro-USB. The Cermant USB-C breakout on the main PCB is being **removed** (see `docs/hardware/usb-c-breakout-removal-guide.md`). The back-panel USB-C position is therefore free — wherever is convenient for the dedication engraving and the pigtail's reach.

---

## Blocking path to "can order PCB"

Do these before submitting to JLCPCB — any of them can force a respin.

- [x] **Remove Cermant USB-C breakout from schematic + PCB** — done (commit `c576061`). ESP32 module's native micro-USB now carries both power and data via a panel-mount USB-C-female to micro-USB-male pigtail. See `docs/hardware/usb-c-breakout-removal-guide.md`.
- [ ] **Verify AITRIP module's USB VBUS input can carry system current** — blocked on parts arrival (2026-04-17 to 2026-04-20). Visually inspect the micro-USB area for a polyfuse. If a 500 mA polyfuse is present, either bridge it, replace it, or add a separate power-only 5V input to the PCB before fab. See step 3.3 in the removal guide.
- [x] **Generate + verify PCB ↔ face alignment** — done. `enclosure/scripts/render_overlay.py` reads LED + mounting-hole positions directly from `hardware/word-clock.kicad_pcb`, cross-references with the letter grid from `grid.cpp`, and 5 regression tests assert the alignment invariants. Every LED is inside the grid bounds with the documented (−1.5, −0.5) mm offset exactly matching the spec. Physical print skipped — computational verification with encoded test claims supersedes it.
- [x] **Measure bottom-side component heights** — done from footprint + datasheet analysis. Tallest feature is the MAX98357A screw-terminal breakout at ~19 mm (if using that variant) or ~15 mm for the DS3231 coin-cell holder. Recommendation: 20 mm standoffs, 18 mm light channel, inside the 48 mm frame. Table + stack-up math in `docs/hardware/pinout.md` under Mechanical section. Confidence [MED] for daughterboard stacks — re-measure with calipers when parts arrive and tighten the budget if helpful.

## Back panel design

Blocked on: nothing structurally, but better to confirm the alignment check above doesn't demand a PCB respin first.

- [ ] **Pick back-panel attachment method** — *requirement: the back panel must be fully removable without damaging the wood, many times over the clock's life.* Forcing function: the DS3231 CR2032 coin cell (~5-10 year life × 40+ year clock = 4-8 battery replacements per unit, minimum). Glue and one-way fasteners are out. Contenders:
  - **Threaded brass inserts + M3 brass machine screws** (*recommended*): 4 inserts pressed into the inner bottom edge of each frame strip once during assembly, M3 × 12 mm countersunk brass screws through the back panel. Unlimited removal cycles. Brass blends into wood aesthetics. No failure modes over 40 years. Cost: ~$10 for inserts + screws.
  - **Recessed N52 magnets + steel plates**: 4× 5 mm discs recessed in panel, steel plates glued inside frame interior. Clean look (no visible fasteners) but has alignment tolerance during assembly, adhesive fatigue over decades, and a yanked panel releases suddenly.
  - **Direct wood screws (no insert)**: works but strips after ~20 removal cycles — marginal.
  - **Rabbet / glue**: ruled out (frames ordered as bare shells; glue is non-serviceable).
- [ ] **Design back panel SVG** — 192×192mm (matches face), per-kid file since dedication is engraved. Features:
  - USB-C cutout for the panel-mount pigtail (location chosen for cable reach and dedication layout — not PCB-constrained)
  - 3 button holes at X=168.5, Y=-19.5/-29.5/-39.5 (sized for button caps or direct tact access)
  - Speaker vent pattern (small circular holes — quantity/diameter TBD per driver datasheet)
  - Dedication engraving (per-kid text + year, shallow etch)
  - Magnet pockets (or screw holes, based on decision above)
  - Alignment features if needed (e.g., small pegs or marks to register panel to frame)
- [ ] **Verify back panel SVG in Ponoko design checker** — same process as frame/face. Engraving lines as separate color from cut lines.

## 3D-printed internals

Blocked on: parts arrival (for diffuser test + LED physical spec), PCB final (for exact LED positions).

- [ ] **Diffuser material test** — compare paper vs 0.5mm frosted PETG on a single LED lit through each. Determines light-channel depth: thicker diffuser = shallower channels needed for uniform pocket illumination.
- [ ] **Light channel / word-blocker model** — walls separating each word's LED pocket, sized from actual LED positions (not documented positions — see LED offset anomaly). Height tuned to (face thickness + diffuser + channel wall) fitting within frame interior above PCB. Smallest pocket is 27.36mm wide.
- [ ] **PCB standoffs** — hold PCB off back panel with clearance for tallest bottom-side component (C2 cap, 16mm). Could integrate with light channel as a single part, or separate for easier printing.
- [ ] **Speaker mount** — holds driver against back-panel vent, accommodates JST connector to PCB.
- [ ] **Button actuator caps** (conditional) — only needed if 6mm tact switches don't sit flush against the back-panel inner surface. Each cap is a short cylinder that translates back-panel press to switch actuation.

## PCB

- [ ] **Revise PCB** if the alignment check or USB check reveals issues. Document any changes in KiCad project.
- [ ] **Submit PCB to JLCPCB** for fab + assembly. 5-unit MOQ (spec budget assumes amortized ~$50 per clock). Lead time 2-3 weeks realistic with shipping slip buffer.

## Assembly + validation

- [ ] **Assembly plan document** — glue-up sequence, jig requirements (to keep frame square during box-joint assembly), fastener BOM, adhesive choice per joint (CA glue for frame box joints? Epoxy for face-to-frame? Wood glue for acrostic of magnet plates?). Write once, reference for both Emory and Nora builds.
- [ ] **Cardboard dry-run** — optional but recommended. Order a cardboard copy of frame + face + back panel (~$15 from Ponoko or local laser) to rehearse assembly before touching the real hardwood. Spec activity U; was skipped before ordering real wood, still worth doing before final gluing.
- [ ] **Emory unit assembly** — first-light test, then glue-up.
- [ ] **30-day burn-in** on Emory unit. Real-time calendar task, cannot be compressed. Watch for thermal issues, audio crackle, RTC drift, WiFi reconnection.
- [ ] **Nora unit assembly** — second unit, with any fixes from Emory baked in.

## Parallel tracks (independent, re-doable)

These don't block anything and can happen on low-brain evenings.

- [ ] **Captive portal HTML/CSS** — first-boot WiFi setup form. Test in a browser with mock backend before wiring to firmware.
- [ ] **Voice memo recordings** — Dad's voice at birth minute (6:10 PM Emory, 9:17 AM Nora). Re-recordable up to delivery in 2030/2032, so v1 now is fine.
- [ ] **Lullaby recordings** — one song per kid, Dad singing. Same re-record window.

## Open questions / research

- Back-panel attachment — magnet sizing + count, or screw sizing + countersink depth.
- Diffuser: paper vs frosted PETG — settled only after the test.
- Speaker driver: specific model dictates vent hole pattern on back panel.
- USB-C wiring approach — direct to ESP32 native USB vs USB-UART bridge chip. Depends on ESP32 variant on the current layout.
