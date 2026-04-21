# Laser-Cut Face + Side Frames — Design Spec

Date: 2026-04-15
Status: Design locked, ready for implementation planning (`writing-plans`)

## Overview

This spec covers the laser-cut wooden components of both clocks: the 13×13 letter face and the four side frames. Light channel, diffuser, back panel, PCB standoffs, speaker mount, button housings, and 3D-printed internals are OUT of scope — each gets its own spec later.

Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`. Parent dependency graph: `docs/archive/specs/2026-04-15-activity-blocking-graph.md` (archived — activities **E**, **F**, **G**).

Target vendor: **Ponoko**. File format: SVG (mm units). Material kerf and minimum-feature values verified against Ponoko material pages (2026-04-15).

## Scope

**In:**
- Emory face (activity E) — Maple Hardwood, 3.20mm
- Nora face (activity F) — Walnut Hardwood, 3.20mm
- Side frame strips (activity G) — 4 per clock, Maple or Walnut Hardwood to match face, 6.40mm

**Out:**
- Diffuser material and assembly (Track Q — parts-blocked)
- Light-channel piece (Track 5 — PCB-integration-blocked)
- Back panel (blocked on PCB USB-C port location + button-access decision)
- 3D-printed internals (separate printer-learning phase)
- Assembly / finishing / sealing (future assembly spec)

## Dimensions

The face is **larger than the letter grid** so it can also serve as the top cap of the box, glued flush to the top of the frame strips. The 13×13 letter grid (177.8 × 177.8 mm — sized to match PCB LED positions exactly) is **centered** in the face with a 7.1 mm plain-wood border around it.

| | Value |
|---|---|
| Face outer | **192.0 × 192.0 mm (~7.56" × 7.56")** |
| Letter grid (centered in face) | 177.8 × 177.8 mm (7" × 7") |
| Border around grid | 7.1 mm on each side |
| Face thickness | 3.20 mm |
| Frame outer | **192.0 × 192.0 mm** (matches face) |
| Frame inner opening | 179.2 mm (face−2×wall = 192−2×6.4) — fits 177.8 mm PCB with 0.7 mm clearance per side |
| Frame strip thickness (wall) | 6.40 mm (Ponoko hardwood max) |
| Frame strip width (depth, front-to-back) | **48 mm (~1.89")** |
| Back panel | 192.0 × 192.0 × 3.20 mm (separate spec; same outer dim) |
| Total clock outer | **192.0 × 192.0 × 54.4 mm (~7.56" × 7.56" × 2.14")** |

Frame depth (48 mm) was chosen as a multiple of 6 mm so the box joints land on a clean 8-fingers-per-corner pattern with each finger equal to the 6 mm material thickness — the traditional box-joint ratio.

**Why no shadow-box recess:** producing a recess for the face to sit behind the frame's front edge requires a rabbet in the frame strip's interior — not feasible in a single-pass laser cut. Stacked-layer or glued-lip workarounds were considered and rejected: stacked layers complicate the file count and assembly; glued lip strips conflict with the PCB-sized face (no room for inward-extending support). **Decision: face glues flush to the top of the frame.** Box joints remain visible at the four corners from the side viewing angle, which is the carpentry detail the build is meant to teach.

### Layer stack inside the 48 mm frame cavity (face−back internal space)

| Layer | mm | Notes |
|---|---|---|
| Diffuser stack (adhesive retainer + opal acrylic) | 3 | Separate spec (Q); target ~3 mm total |
| Light channel | 15–17 | LED-to-diffuser distance; separate spec (Track 5) |
| PCB | 1.6 | Verified from KiCad Edge.Cuts: 177.8 × 177.8 mm |
| Bottom-side components | 16 | Dominated by C2 1000 µF / 10V electrolytic, verified from KiCad footprint |
| Buffer | 3 | Tolerance + thermal |
| **Sum used** | **~40.6 mm** | leaves ~7 mm spare inside the 48 mm cavity |

## Materials

Both clocks use **solid hardwood** throughout. Ponoko confirmed (2026-04-15) that both Maple and Walnut Hardwood are kiln-dried solid lumber (not plywood, not veneer).

| Clock | Face | Frame |
|---|---|---|
| Emory | Maple Hardwood, 3.20 mm | Maple Hardwood, 6.40 mm |
| Nora | Walnut Hardwood, 3.20 mm | Walnut Hardwood, 6.40 mm |

**Why solid hardwood (not plywood, not veneer MDF):** this is a 40-year heirloom. Ponoko's Birch Plywood specifically "laser cuts with a dark edge" (charring visible on every letter aperture) and can have knots, pin marks, and mineral streaks. Walnut Premium Veneer MDF would create an asymmetry where Emory's clock looks and feels materially cheaper than Nora's. Both clocks solid hardwood keeps the tier equal and the aesthetic right.

**Why Maple for Emory (not Birch):** Ponoko has no birch hardwood — only birch plywood. Maple hardwood is a solid-wood substitute with pale cream coloring that reads similarly to birch from normal viewing distance. Same tier as walnut, same thicknesses, same laser behavior.

**Known natural variation:** both materials may include knots, color variation, and mild grain variation. Ponoko explicitly says it cannot guarantee complete flatness; mild warping is possible. Acceptable for the aesthetic.

## Letter Grid & Typography

### Grid

- 13 × 13 cell letter grid, **177.8 × 177.8 mm**, centered in the 192 × 192 mm face
- Per-cell dimension: **13.68 mm square** (177.8 / 13)
- Cells aligned to PCB LED grid (PCB is same 177.8 × 177.8 mm footprint, LEDs on PCB top side, on 13.68 mm pitch — verified from CPL file)
- 7.1 mm plain-wood border around the grid (the difference between 192 and 177.8, halved)
- Letters in each cell drawn from `firmware/lib/core/src/grid.cpp` — grids are locked in firmware, identical layout per kid except row 8 and acrostic filler cells

### Letter height

- **Letter cap height: 10 mm** (~73% of 13.68 mm cell)
- Padding per side: 1.84 mm vertical + horizontal (letter centered in cell)
- Inter-letter wood: 3.68 mm (≥ Ponoko's 3 mm minimum feature on solid hardwood)
- Inner counter clearance: closed letters (B, P, R, 6, 8, 9) at 10 mm cap height have counters ~3.3 mm across — above minimum feature
- Going larger than 10 mm (e.g., 11 mm) would drop inter-letter wood below the 3 mm minimum feature. 10 mm is a hard sweet spot.

### Fonts

| Clock | Font | License | Source |
|---|---|---|---|
| Emory | **Jost Bold** | SIL OFL | fonts.google.com/specimen/Jost |
| Nora | **Fraunces Medium** | SIL OFL | fonts.google.com/specimen/Fraunces |

**Why Jost (not Futura):** Futura is proprietary and requires a commercial license. Jost was explicitly designed as an open-source Futura alternative — same geometric letterforms, same single-storey 'a', same confident warm bold weight. Similarity rated 90% vs. Futura. Free under SIL OFL for any use.

**Personality intent** (from brainstorming):
- Emory — Jost Bold: confident, warm, geometric. Reflects "loud, silly, wrecking-ball-with-internal-structure" energy.
- Nora — Fraunces Medium: modern serif with character. Reflects "quiet watcher with depth, rewards close attention."
- Both warm, both readable, both solid enough visually to not get lost in a 13×13 grid.

### Closed-letter handling (inner-island retention)

Closed-counter letters (A, B, D, O, P, Q, R) and digits (0, 6, 8, 9) are cut **without bridges**. Inner counter islands will be retained by an **adhesive-backed diffusion film** applied to the back of the face. This matches the approach used in Chelsea's 2015 clock.

**Assembly sequence (documented here for forward reference; executed in future assembly spec):**
1. Face cut at Ponoko, shipped with inner islands loose.
2. Lay face letter-side up, apply adhesive-backed diffusion film to the back.
3. Any island still sitting in its aperture sticks automatically; use tweezers to place any strays before flipping.
4. Face + diffusion film becomes one sub-assembly.
5. Frame box already assembled (4 strips with box joints, glued square).
6. Glue face sub-assembly to the top edge of the frame box, edge-aligned. Clamp until cured.
7. Internal stack (light channel + PCB + diffuser-second-layer) installs from the back via the open back of the frame box, before the back panel is attached (back panel attachment in separate spec).

**Constraint flowing OUT to diffuser spec (Q):** the diffuser (or at least the layer touching the back of the face) MUST be adhesive-backed to retain inner islands. Rules out loose paper diffusers.

## Face Construction

Each face is a single SVG cut file containing:

1. **Outer rectangle** — 192.0 × 192.0 mm rectangle, cut path.
2. **Letter apertures** — 169 letter shapes, cut paths. Letters rendered from the chosen font at 10 mm cap height, positioned at cell centers within the centered grid, converted to outline paths.
3. **No bridges.** Inner counters of closed letters are separate paths (will cut as islands; retained by adhesive diffusion film during assembly).

**Letter positioning math** (for both clocks):
- Grid origin (top-left of letter grid): (7.1 mm, 7.1 mm) from face top-left corner — accounts for the 7.1 mm border.
- Cell (row, col) center: x = 7.1 + (col + 0.5) × 13.68 mm, y = 7.1 + (row + 0.5) × 13.68 mm.
- Letter glyph centered at cell center with 10 mm cap height.

Letters come from `firmware/lib/core/src/grid.cpp`:
- Emory: `EMORY` on row 8, filler letters spell `E M O R Y 1 0 6 2 0 2 3` in reading order.
- Nora: `NORA` on row 8, filler letters spell `N O R A M A R 1 9 2 0 2 5` in reading order.

## Frame Construction

Each clock has 4 identical frame strips that form a square box with box-joint (finger-joint) corners.

### Strip dimensions (before joint cuts)

- Length: 192.0 mm (full outer dimension — joints cut on short edges)
- Width: 48 mm (frame depth)
- Thickness: 6.40 mm (material)

### Corner joinery — box joint

- **8 fingers × 6 mm pitch** per corner mating edge
- Finger pattern: corner edge alternates finger-slot-finger-slot. Two mating strips have **inverse** finger patterns so they mesh.
- Finger length = material thickness = 6.4 mm (the finger sticks through the mating strip's slot to be flush on the outside).
- **Kerf compensation:** finger cuts slightly undersized (−0.1 mm on slot, +0.1 mm on finger) to produce a press-fit joint accounting for Ponoko's 0.20 mm kerf. Exact values refined after cardboard test (see Validation).

### Face-to-frame attachment

Face glues directly to the **top edge** of the assembled frame box. Because face outer (192 × 192 mm) matches frame outer (192 × 192 mm), the face caps the box flush — no overhang, no recess. The 4 box-joint corners are visible from the side viewing angle (the carpentry detail) but covered from the top by the face.

Glue: standard wood glue (Titebond III or equivalent) along the top 6.4 mm edge of all 4 frame strips. Clamp until cured.

### Back panel attachment (defined in future spec, dimensioned here for reference)

Back panel matches face: **192 × 192 × 3.2 mm**, glued or screwed to the bottom edge of the frame box (mirror of the face attachment). For service access (USB-C reflashing, SD card swap), the back panel may be screwed rather than glued — TBD in the back-panel spec.

## Ponoko file format

- **Format:** SVG, unit: mm
- **Stroke:** 0.01 mm hairline
- **Stroke color:** blue `#0000FF` for cut paths (Ponoko convention)
- **Fills:** none; paths only
- **One file per piece** — no nested groups of unrelated pieces in a single SVG
- **No text elements** — all text (letters) converted to outline paths
- **Nesting** — Ponoko bills per sheet area, so pieces from the same material/thickness can be nested into one SVG to minimize cost (face on its own Maple 3.20mm sheet; 4 frame strips into a single Maple 6.40mm sheet; similarly for Walnut).

## Files to be produced (implementation phase)

| File | Content | Material |
|---|---|---|
| `enclosure/emory-face.svg` | 192 × 192 mm face with 13×13 letter grid | Maple Hardwood 3.20 mm |
| `enclosure/emory-frame.svg` | 4 frame strips (192 × 48 mm) with box-joint cuts | Maple Hardwood 6.40 mm |
| `enclosure/nora-face.svg` | 192 × 192 mm face with 13×13 letter grid | Walnut Hardwood 3.20 mm |
| `enclosure/nora-frame.svg` | 4 frame strips (192 × 48 mm) with box-joint cuts | Walnut Hardwood 6.40 mm |

Generation approach deferred to `writing-plans`. Likely Python with `fontTools` (glyph path extraction) + `svgwrite` or direct SVG templating, plus a parametric box-joint generator (existing open-source options: boxes.py, `makercase`).

## Verified numbers (from Ponoko + KiCad, 2026-04-15)

| | Value | Source |
|---|---|---|
| PCB outer dim | 177.8 × 177.8 mm | `hardware/word-clock.kicad_pcb` Edge.Cuts |
| Bulk cap C2 height | 16 mm | KiCad footprint `CP_Radial_D10.0mm_P5.00mm` |
| Walnut Hardwood kerf | 0.20 mm | ponoko.com/materials/walnut-hardwood |
| Walnut Hardwood min feature | 3.00 mm | ponoko.com/materials/walnut-hardwood |
| Maple Hardwood kerf | 0.20 mm | ponoko.com/materials/maple-hardwood |
| Maple Hardwood min feature | 3.00 mm | ponoko.com/materials/maple-hardwood |
| Walnut Hardwood max sheet | 599 × 295 mm | ponoko.com/materials/walnut-hardwood |
| Maple Hardwood max sheet | 599 × 295 mm | ponoko.com/materials/maple-hardwood |
| Light channel reference depth (≥15 mm) | 16 mm (2× 8 mm MDF) | madengineer.ch word-clock build, 2016 |
| Jost font license | SIL OFL | fonts.google.com/specimen/Jost |
| Fraunces font license | SIL OFL | fonts.google.com/specimen/Fraunces |

## Constraints flowing OUT to other specs

1. **Diffuser (Q):** the layer touching the back of the face MUST be adhesive-backed to retain inner islands. Recommended stack: ~0.2 mm adhesive-backed diffusion film + 2–3 mm opal cast acrylic behind it for primary hot-spot elimination.
2. **Light channel (Track 5):** must maintain ≥ 15 mm LED-to-diffuser distance for acceptable hot-spot suppression. 17 mm allocated in the depth budget.
3. **PCB:** mounts to frame or back panel via standoffs within the 16 mm bottom-side component envelope. **Flow-out note for future PCB rev:** swapping the 16 mm through-hole 1000 µF electrolytic for a low-profile SMD polymer cap would enable a slimmer (~35 mm) frame depth in a v2 clock.
4. **Back panel:** 192 × 192 × 3.2 mm outer (mirror of face), glues or screws to the bottom edge of the frame box. Needs cutouts for USB-C (PCB top-left corner, bottom-side J_USB1 at (33.5, −7.46)) and possibly button access. Screwed (not glued) is preferred for service access — TBD in back-panel spec.
5. **Button access:** PCB SW1/SW2/SW3 are through-hole tact switches on bottom side near the right edge (X=168.5 mm), pointing toward the back panel. The parent design spec says "right-side buttons." This is a conflict to resolve in the enclosure-assembly spec: (a) accept bottom-rear access via back-panel cutouts, (b) build a mechanical side-actuator linkage, or (c) plan a PCB revision. No frame modification in this spec commits to any of these paths.

6. **LED placement offset (PCB layout anomaly):** The actual PCB has LEDs systematically offset by **(−1.5 mm, −0.5 mm)** from the documented word-center positions in `docs/archive/hardware/2026-04-15-kicad-rework.md`. Verified across all 35 LEDs. The offset is in the placement, not a footprint origin issue (footprint origin = chip body center, verified in `hardware/word-clock.kicad_pcb`). Root cause unknown (possibly intentional clearance for tact switches at X=168.5 mm right-edge, possibly placement error). **Track 5 (light channel) MUST size word pockets and wall positions referencing the actual LED coordinates in `hardware/word-clock-all-pos.csv`, NOT the documented positions in the rework guide.** With proper light-channel design + good diffusion, the offset is not visually significant on the lit face (analysis: smallest word pocket is 27.36 mm wide vs 1.5 mm offset = LED still fills pocket via 120° cone at 15 mm distance, diffuser homogenizes within pocket).

7. **Light channel material override:** The parent design spec calls for "white PLA" for all internal 3D-printed parts. The light-channel piece specifically must be **black PLA**, minimum 1.5 mm wall thickness, 100% infill near walls — to prevent inter-pocket light bleed. White PLA at typical wall thickness leaks visible light at WS2812B brightness, which would (a) glow filler cells that are supposed to stay dark, (b) halo word boundaries during dim mode. Other internal parts (PCB standoffs, speaker mount, button housings) can remain white per parent spec since they're not optical isolators.

8. **Filler cell isolation (consequence of 6 + 7):** All 12 (Emory) / 13 (Nora) filler cells must each have their own sealed dark pocket in the light channel — no LED, no light path from any neighboring word pocket. Otherwise neighboring word LEDs will illuminate the fillers and break the time-reading by glowing nonsense letters. Most fillers sit interleaved with word groups in the same row (e.g., filler `E` at (0,2) sits between word `IT` (cells 0-1) and word `IS` (cells 3-4)), so this is a real Track 5 design requirement, not an edge case.

## Open issues

These are known unknowns. None block this spec, but each has a follow-up:

1. **Adhesive diffusion film selection.** Generic products exist (KROPP, Armacost, generic strip-light diffusers). Specific product + thickness TBD in Q spec after testing on cardboard face.
2. **Opal acrylic source and thickness.** 2 mm vs 3 mm — depends on diffusion test results. TBD in Q spec.
3. **Wood finish.** Oil, wax, varnish, none? TBD in assembly spec. Ponoko ships sanded but unfinished.
4. **Letter path winding direction and island cut order.** For SVG generation, need to validate that Ponoko's laser respects winding rules such that inner islands are cut but not lost before face is shipped. Mitigated by cardboard test.

## Validation plan

Before committing to Maple/Walnut hardwood cuts (~$140 total in materials), **cut a cardboard proof** at Ponoko (activity **U** in the dep graph, ~$10–20):

1. Same SVG as Emory face, on ~3 mm corrugated cardboard.
2. Verify:
   - Letters cut cleanly without scorching the inter-letter wood
   - Inner counter islands survive shipping in a usable state (visually intact, identifiable per letter, retainable on adhesive backing)
   - Letter readability at intended viewing distance (3–6 ft)
   - Inter-letter wood doesn't tear or distort during cut or handling
   - Frame strip box joint with matching cardboard frame — verify kerf compensation produces a snug press-fit
3. If any fails: revise SVG (larger letters, finer kerf compensation, etc.) and re-cut cardboard before committing to real wood.
4. Once cardboard passes: commit to one Maple face + one Maple frame first (Emory), validate assembly, then order Nora in Walnut.

## Cost estimate

Ponoko pricing varies by piece area and material. Rough estimate for both clocks:

| Item | Material | Est. cost |
|---|---|---|
| Emory face (192×192 mm) | Maple 3.20 mm | ~$25–35 |
| Emory frame strips (4× 192×48 mm) | Maple 6.40 mm | ~$30–45 |
| Nora face (192×192 mm) | Walnut 3.20 mm | ~$30–40 |
| Nora frame strips (4× 192×48 mm) | Walnut 6.40 mm | ~$35–50 |
| Cardboard test cut (1 face + 4 frame strips) | Cardboard | ~$10–20 |
| Shipping (Ponoko USA → home, 1 order) | — | ~$15–25 |
| **Total (both clocks + test)** | | **~$145–215** |

Spec-level budget for laser-cut wood was ~$80 for both clocks (original spec); hardwood upgrade pushes this to $145–215. For a 40-year heirloom, this is the right line to spend on.

## Implementation status

- Implementation plan: `docs/archive/plans/2026-04-15-laser-cut-face-implementation.md` (archived — SVGs shipped)
- Generator: `enclosure/scripts/build.py` (run from repo root via `python enclosure/scripts/build.py`)
- Output SVGs: `enclosure/{emory,nora}-face.svg` + `enclosure/frame.svg` (frame is kid-agnostic — same file, different wood material per clock)
- Tests: `pytest enclosure/scripts/tests/`
- Cardboard test step was considered but skipped — it wouldn't catch the risks we actually care about (grain, hardwood kerf, scorching) since cardboard laser behavior differs from hardwood. Ponoko's design checker validates geometry; ordering cardboard first adds $20 + 3 week delay without de-risking the real failure modes.

## References

- Parent spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
- Dependency graph: `docs/archive/specs/2026-04-15-activity-blocking-graph.md`
- Firmware grids (source of truth for letter layout): `firmware/lib/core/src/grid.cpp`
- PCB (source of truth for board dimensions): `hardware/word-clock.kicad_pcb`
- Ponoko material pages: [maple](https://www.ponoko.com/materials/maple-hardwood), [walnut](https://www.ponoko.com/materials/walnut-hardwood)
- Ponoko manufacturing standards: https://help.ponoko.com/en/articles/9358110
- Font sources: [Jost](https://fonts.google.com/specimen/Jost), [Fraunces](https://fonts.google.com/specimen/Fraunces) (both SIL OFL)
- Box-joint rule-of-thumb: https://projects.raspberrypi.org/en/projects/lasercutjoints/2
- Word-clock light-channel depth precedent (16 mm): https://madengineer.ch/blog/2016/02/28/word-clock-on-steroids/
