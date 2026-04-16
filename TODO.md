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
- USB cable is a single **Micro-USB-to-USB-C cable, 3-6 ft long**, plugged into the ESP32 module's native Micro USB port and routed out through a grommeted hole in the back panel. The USB-C end dangles outside the clock as the user-facing connector — plug it into any USB-C charger or laptop. No panel-mount adapter, no internal pigtail, no screw-aligned hardware. Cable replaces by removing the 4 corner screws + pulling the old cable + threading the new one. The Cermant USB-C breakout on the main PCB is **removed** (see `docs/hardware/usb-c-breakout-removal-guide.md`).

---

## Blocking path to "can order PCB"

Do these before submitting to JLCPCB — any of them can force a respin.

- [x] **Remove Cermant USB-C breakout from schematic + PCB** — done (commit `c576061`). ESP32 module's native micro-USB now carries both power and data via a panel-mount USB-C-female to micro-USB-male pigtail. See `docs/hardware/usb-c-breakout-removal-guide.md`.
- [ ] **Verify AITRIP module's USB VBUS input can carry system current** — blocked on parts arrival (2026-04-17 to 2026-04-20). Visually inspect the micro-USB area for a polyfuse. If a 500 mA polyfuse is present, either bridge it, replace it, or add a separate power-only 5V input to the PCB before fab. See step 3.3 in the removal guide.
- [x] **Generate + verify PCB ↔ face alignment** — done. `enclosure/scripts/render_overlay.py` reads LED + mounting-hole positions directly from `hardware/word-clock.kicad_pcb`, cross-references with the letter grid from `grid.cpp`, and 5 regression tests assert the alignment invariants. Every LED is inside the grid bounds with the documented (−1.5, −0.5) mm offset exactly matching the spec. Physical print skipped — computational verification with encoded test claims supersedes it.
- [x] **Measure bottom-side component heights** — done from footprint + datasheet analysis. Tallest feature is the MAX98357A screw-terminal breakout at ~19 mm (if using that variant) or ~15 mm for the DS3231 coin-cell holder. Recommendation: 20 mm standoffs, 18 mm light channel, inside the 48 mm frame. Table + stack-up math in `docs/hardware/pinout.md` under Mechanical section. Confidence [MED] for daughterboard stacks — re-measure with calipers when parts arrive and tighten the budget if helpful.

## Back panel design

Blocked on: nothing structurally, but better to confirm the alignment check above doesn't demand a PCB respin first.

- [ ] **Add daughterboard-orientation silkscreen to the PCB** — `J_RTC1` and `J_SD1` both sit on the bottom side with the breakout hanging into the air gap toward the back panel. Correct orientation puts the CR2032 holder and SD card slot *facing the back panel* (accessible). Reversed orientation buries them against the main PCB (trapped). Prevent mis-assembly by silkscreening a "BATTERY →" / "SD →" marker on the main PCB next to each header. Single KiCad edit, no fab consequence beyond the next gerber export.
- [x] **microSD slot cutout** — included in the generated back-panel SVG (14 × 3 mm rectangle near the J_SD1 header X). Confidence [LOW] on exact Y position — depends on which direction the HW-125 breakout orients when plugged in. Verify + tune during Emory first-fit.
- [x] **Back-panel attachment method** — decided: **M3 brass hex spacers epoxied into each interior corner of the frame**. Spacers are 5 mm AF × 10 mm long female-female. Structural epoxy (JB Weld / E6000) bonds each spacer to both adjacent frame strips at the corner, giving a multi-surface bond stronger than any single-surface fix. The spacer provides the machine thread independent of the frame's 6.4 mm wall thickness, so threading into thin wood is never a concern. 4 corner screw holes in the back panel (currently at 4 mm inset), M3 × 12 mm countersunk brass machine screws. Unlimited removal cycles, covers the 40-year CR2032 replacement cadence without any wood-thread wear. Shopping list: one 10-pack of M3 × 10 mm brass hex spacers (~$8), M3 × 12 mm flat-head brass screws (~$5), 2-part structural epoxy.
- [x] **Design back panel SVG** — v1 generated by `enclosure/scripts/render_back_panel.py`. Per-kid files (`emory-back-panel.svg`, `nora-back-panel.svg`) emit:
  - Outer 192×192 mm cut
  - 4 × M3 clearance holes (one per edge, centered, 3.2 mm inset) for brass-threaded-insert machine screws
  - 3 button access holes aligned to SW1/SW2/SW3 PCB positions
  - USB-C panel-mount cutout (9 × 13 mm + 2 × M2.5 mount screws) — dimensions from Adafruit 4218-class pigtail
  - microSD access slot (tentative position — see note above)
  - 5×5 speaker vent grid (placeholder — adjust once speaker driver is picked)
  - Dedication raster engraving rendered in the matching face font (Jost for Emory, Fraunces for Nora)
  - 9 pytest regression tests guard structural invariants (`test_render_back_panel.py`)
- [ ] **Verify back panel SVG in Ponoko design checker** — same process as frame/face. Engraving uses `fill="#000000"` per Ponoko's raster-engrave convention; cuts use blue hairline per the existing `svg_utils` helpers.

## 3D-printed internals

Blocked on: parts arrival (for diffuser test + LED physical spec), PCB final (for exact LED positions).

- [ ] **Diffuser material test** — compare paper vs 0.5mm frosted PETG on a single LED lit through each. Determines light-channel depth: thicker diffuser = shallower channels needed for uniform pocket illumination.
- [ ] **Light channel / word-blocker model** — walls separating each word's LED pocket, sized from actual LED positions (not documented positions — see LED offset anomaly). Height tuned to (face thickness + diffuser + channel wall) fitting within frame interior above PCB. Smallest pocket is 27.36mm wide.
- [ ] **PCB standoffs** — hold PCB off back panel with clearance for tallest bottom-side component (C2 cap, 16mm). Could integrate with light channel as a single part, or separate for easier printing.
- [ ] **Speaker mount** — holds driver against back-panel vent, accommodates JST connector to PCB.
- [ ] **Button actuator caps** (conditional) — only needed if 6mm tact switches don't sit flush against the back-panel inner surface. Each cap is a short cylinder that translates back-panel press to switch actuation.
- [ ] **Source a Micro-USB-to-USB-C cable (3-6 ft) + matching rubber grommet** — grommet panel hole currently sized at 6 mm; tune to match the specific cable's OD (most slim cables are 3.5-4 mm, which wants a 5 mm grommet; thicker cables need 6-7 mm).

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
