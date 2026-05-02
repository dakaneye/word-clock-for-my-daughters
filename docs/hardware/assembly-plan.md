# Assembly Plan

End-to-end glue-up + fasten-up sequence for each clock. Single source of
truth once all parts are in hand. Emory is the pilot build; Nora follows with
fixes baked in.

Prereqs: PCB validated on breadboard (Phase 2 firmware bring-up complete),
final PCB ordered and received from JLCPCB, 3D-printed internals printed,
wood arrived from Ponoko, supplies on the `TODO.md` order list in hand.

---

## Fastener + adhesive BOM (per clock)

| Item | Qty | Purpose |
|---|---|---|
| M3 × 1/2" or 5/8" wood screws (pan-head, brass or steel) | 4 | Back-panel-to-frame-wall closure (thread directly into the 6.4 mm frame wood at the existing 4 mm corner inset; pre-drilled pilot hole) |
| M3 × 10 mm brass hex spacer, F-F, 5 mm AF | 2 | Speaker mount risers epoxied to back panel interior, 37 mm apart behind the vent |
| M3 × 6-8 mm machine screws | 2 | Speaker integral flanges → speaker hex spacers |
| Rubber grommet for ~16 mm panel hole + cable P-clip | 1 | USB cable strain relief; back-panel hole gets drilled out 6 → 16 mm at assembly so the connector overmold fits through |
| Self-adhesive black foam (1 mm), ~30 cm² | 1 | Light-channel top-edge seal against the diffusion film on the face back (alternative: putty at assembly) |
| M3 washers (optional) | 4 | Under screw heads on the back panel to protect the wood |

**Adhesives**

| Joint | Adhesive | Why |
|---|---|---|
| Frame box joints (4 corners) | Titebond III preferred; Gorilla 5-min epoxy works as substitute | Strongest wood-to-wood; epoxy-on-finger-joints relies on joint geometry for strength but needs ≥30 min clamp time, not the 5 the label implies (Nora's frame is being assembled with epoxy 2026-04-25) |
| Face top edge → frame top | Titebond III (do NOT substitute 5-min epoxy here) | Large flat-on-flat perimeter joint; 5-min epoxy's ~3 min open time is too fast for face alignment, and squeeze-out is much harder to clean off a visible exterior joint than wood glue is |
| Speaker hex spacers → back panel interior | 2-part epoxy (Gorilla 5-min, JB Weld, or E6000) | Metal-to-wood; spacer flange contact is small, structural epoxy required |
| PCB standoff posts → back panel interior | E6000 | PLA-to-wood; slight flex tolerates thermal cycling |
| Light-channel top edge → diffusion film (on face back) | Self-adhesive foam (1 mm) compressed by back-panel screws, OR putty at assembly | Seals lateral light bleed at the channel/diffuser interface. (Diffuser stack is film-only as of 2026-04-26 — opal acrylic was removed from the production stack after bench testing showed it was the source of lateral bleed.) |
| Inner letter islands → face back | self-adhesive diffusion film | Ships pre-glued; the same film also retains loose letter islands |

**Tools**

- 4 × corner clamps (Bessey right-angle or equivalent)
- 2-4 × bar clamps (face-to-frame glue-up)
- Framing square (12"+)
- Tape measure / caliper
- Sanding block, P220 grit
- Needle-nose tweezers (letter islands)
- Small Phillips screwdriver (M3)
- Scissors (diffusion film cutting; X-Acto + steel ruler unnecessary at this precision level)
- 5/8" step drill bit (cable port drill-out, Phase F2)
- ~2 mm drill bit (frame wall pilot holes for back-panel wood screws, Phase G9)
- Masking tape (clamping substitute + alignment)

---

## Phase A — Pre-assembly prep

Do all of this before opening any glue. Most of these are independent and
can run in parallel.

- [ ] **Inspect Ponoko wood pieces.** Confirm face dimensions 192 × 192 ×
      3.2 mm, frame strips 192 × 48 × 6.4 mm × 4 strips, back panel
      192 × 192 × 3.2 mm. Sand any scorched edges with P220.
- [ ] **Measure frame interior + PCB outline (for light channel + fit
      verification).** Once each frame is fully cured, caliper the inner
      dimension on all 4 sides at top, middle, and bottom of the wall
      height (12 measurements per frame). Record the smallest. When
      JLCPCB boards arrive, caliper the PCB outline both axes. Decision
      tree: total PCB-to-frame clearance ≥ 1.4 mm → fits cleanly;
      0.4–1.4 mm → fits but tight, design light channel ~3 mm under
      smaller frame inner; < 0.4 mm → sand frame interior with P220
      block until clearance opens up. Light channel envelope is locked
      from these measurements, not from the nominal 179.2 / 177.8 spec.
- [ ] **Assemble + fully test the final PCB** on the workbench, not inside
      the enclosure. Flash firmware, verify every peripheral works, then
      power off.
- [ ] **Print 3D internals.**
  - PCB standoff posts × 4
  - Button actuator caps × 3
  - Light-channel honeycomb × 1 — *deferred until frame interior + PCB
    outline are measured (see measurement task above); design is
    parametric in those numbers*
- [ ] **Face sub-assembly (D below) can happen now** — doesn't block on anything else.
- [ ] **Cut captive USB cable to internal length.** Measure from back-panel
      cable hole position to ESP32's micro-USB port with ~50 mm slack.
      Typical: 100-150 mm of cable inside, the rest trails outside as the
      user-facing USB-C plug.

---

## Phase B — Frame box glue-up

- [ ] **B1: Dry-fit all 4 strips.** Press-fit the box joints — they should
      seat with hand pressure. If too tight, light sanding on the fingers;
      if too loose, add more glue later.
- [ ] **B2: Diagonal-check for squareness.** With the dry-fit box on a flat
      surface, measure both corner-to-corner diagonals. They must be equal
      within 0.5 mm. Rack the box into square if needed.
- [ ] **B3: Glue up.** Disassemble, apply Titebond III to each finger and
      slot on all 4 corners. Reassemble, press together. Wipe squeeze-out
      with a damp rag immediately.
- [ ] **B4: Clamp square.** 4 corner clamps, verify diagonals still equal
      before glue tacks.
- [ ] **B5: Cure 24 h** undisturbed on a flat surface.

---

## Phase C — REMOVED (2026-04-25)

The original plan had M3 hex spacers epoxied into the frame's interior
corners with the back-panel screws threading up into them. Abandoned
after `enclosure/scripts/render_back_panel.py` was found to specify a
4 mm corner inset that places the screw centers *inside* the 6.4 mm
frame wall — not in the interior corner — making the spacer-in-corner
geometry impossible. Replaced by **M3 wood screws threading directly
into the frame wall** (folded into Phase G); the 4 mm inset becomes
correct for the new strategy because it puts the screw center right in
the wall material. See `enclosure/scripts/render_back_panel.py` fastener
comment block for the locked rationale.

---

## Phase D — Face sub-assembly

Can run in parallel with B/C.

**Diffuser stack as of 2026-04-26: film only — no opal acrylic.** Bench
test with the printed 18 mm test pocket
(`enclosure/3d/light_channel_test_pocket.py`) over a single lit WS2812B
showed the film alone provides adequate hot-spot suppression at this
LED-to-film distance, while adding the acrylic introduced ~5 mm of
lateral light bleed. Acrylic sheets shelved in reserve as backup if
hot spots emerge during full-clock first-light testing — the fix
would be per-pocket acrylic cuts + light-channel walls extending
through the acrylic level (re-print of the channel at ~21 mm walls).
See `TODO.md` "Decisions locked 2026-04-26" for the rationale.

- [ ] **D1: Cut the diffusion film to ~178 × 178 mm.** Sharp scissors
      are fine for this — film is 0.16 mm and the cut edges sit hidden
      inside the assembled clock. Tolerance is generous: 177-179 mm
      cleanly satisfies "covers the 177.8 × 177.8 letter grid" and
      "stays clear of the 6.4 mm Titebond glue ring at the perimeter."
      Even off by ±2-3 mm degrades gradually rather than failing
      binary. Cut backing-side up, mark with pencil first, single
      pass. One Selens sheet (~200 × 300 mm) yields both kids' pieces
      plus practice off-cuts.
- [ ] **D2: Place face on clean bench, letter side down.** Wipe the
      back of the face with the alcohol wipe from the Selens kit (or
      IPA on a lint-free cloth). Remove dust, oils, and any kerf char
      residue. Dust under the film = visible specks when LEDs light up.
- [ ] **D3: Apply the film, adhesive-side down, centered on the inner
      letter-grid area.** Mark the 178 × 178 mm target with a faint
      pencil dot at each of the 4 corners during dry-fit before
      peeling. Peel a 2-3 cm strip of backing from one edge; align
      the partially-peeled edge with one of the marked corners; press
      down; progressively peel more backing while squeegeeing
      (included in Selens kit) outward from that edge. Stay strictly
      inside the 7 mm bare-wood perimeter — film overlapping the glue
      ring acts as a release layer and the Titebond won't bond
      wood-to-wood there.
- [ ] **D4: Letter-island retention.** As the film presses down over
      the apertures, the loose inner-letter islands (A/B/D/O/P/Q/R/0/6/8/9
      cuttings that fell out during laser cutting) bond to the film
      automatically. If you spot any loose islands in the apertures
      *before* the film passes over them, place them with tweezers.
      You can lift the film slightly to reposition islands while the
      adhesive is still fresh; once fully pressed, they're locked.

---

## Phase E — Face-to-frame glue-up

- [ ] **E1: Dry-fit the face on the frame.** Face 192 × 192 mm sits flush
      with the 192 × 192 mm frame top. All edges align. Verify the
      diffusion film (applied in Phase D) does NOT extend into the
      6.4 mm perimeter that contacts the frame top. If it does, either
      trim the film back or accept reduced glue area on that edge.
- [ ] **E2: Apply wood glue** in a continuous bead along all 4 top
      edges of the frame (the 6.4 mm wide top surface of each strip).
      **Titebond III preferred** (waterproof, ~20-30 min open time);
      **Gorilla Wood Glue acceptable** (water-resistant, ~10-15 min
      open time — same PVA family). DO NOT substitute Gorilla 5-min
      epoxy here — the open time is too short for a 192 mm perimeter
      alignment, and squeeze-out is much harder to clean off a visible
      exterior joint than wood glue is. Spread with a finger or
      spreader to a thin even layer.
- [ ] **E3: Place face letter-side up** on the frame top, align flush
      at all edges. Slide gently to true if needed (wood glue's
      open time gives you some leeway).
- [ ] **E4: Clamp with bar clamps** across the face, ideally 4 (one per
      side). Cauls (scrap wood between clamp and face) protect the wood
      from clamp marks.
- [ ] **E5: Wipe squeeze-out** at the joint line with a damp rag
      immediately. Wood glue cleans up easily while wet; cured drips
      have to be chiseled or sanded off later.
- [ ] **E6: Cure 24 h** undisturbed before unclamping.

---

## Phase F — Back panel internals

Do F1–F9 with the back panel on the bench, still separate from the frame.

- [ ] **F1: Verify back-panel cutouts.** 4 × M3 corner clearance holes
      (3.3 mm at 4 mm inset), 3 × button actuator holes (6.5 mm), 1 ×
      cable hole as cut (6 mm — gets drilled out in F2), 1 × speaker vent
      pattern. Clean up any char with P220.
- [ ] **F2: Drill cable hole 6 → 16 mm** with a 5/8" step bit (or
      Forstner) on a backing board. Step bit walks up the existing 6 mm
      hole cleanly; feed slowly, clear flutes between holes. The
      enlarged hole has to be big enough for the USB connector overmold
      (~12-15 mm) to pass through during cable threading — strain relief
      moves to an internal P-clip in F4 instead of being provided by the
      grommet itself.
- [ ] **F3: Install rubber grommet** in the now-16 mm cable hole.
      Cosmetic + dust-blocking only at this size; doesn't grip the cable.
- [ ] **F4: Thread USB cable through the grommet** from outside (USB-C
      plug trails outside) to inside (micro-USB plug lies loose inside).
      Screw a small cable P-clip to the back-panel interior 2-3 cm
      inside the cable hole, clamp the cable in the P-clip — this is
      the actual strain relief that keeps tugs on the outside cable from
      reaching the ESP32 micro-USB solder joints.
- [ ] **F5: Epoxy PCB standoff posts × 4** to the back panel interior at
      the PCB corner positions. Use E6000. Hold 60 s. Diagonals: measure
      corner-to-corner to confirm the 4 standoffs form a square matching
      the PCB footprint (verify against the actual PCB outline measured
      in Phase A, not the 177.8 mm nominal).
- [ ] **F6: Epoxy 2 × M3 hex spacers** to the back-panel interior 37 mm
      apart, centered behind the speaker vent. Threaded openings face
      outward (away from the back panel). The speaker's integral
      mounting flanges land on the spacer tops; the spacers carry the
      speaker, the vent passes the air. (No 3D-printed cradle needed —
      removed from the design 2026-04-21 after measuring that the
      speaker has 37 mm-spaced integral M3-compatible flanges.)
- [ ] **F7: Mount the speaker** with 2 × M3 × 6-8 mm machine screws
      through the speaker flanges into the hex spacers. Connect the
      JST-PH 2-pin lead to the speaker but leave the PCB-side end loose
      for now.
- [ ] **F8: Install button actuator caps × 3** by dropping each one
      into a button hole from the panel **interior side** (panel laying
      flat, engraved dedication face-down). The 6.2 mm neck slides
      through the 6.5 mm hole and protrudes 1 mm out the exterior; the
      8 mm flange catches on the panel interior and can't pass through.
      No force, no cutting. Caps will want to fall back out toward the
      case interior when you flip the panel for closeout — either tape
      over the holes during the in-between, or proceed promptly to
      Phase G. (Original design used a "push-from-outside friction-fit"
      cap with an outer head; revised 2026-04-25 to drop-in-from-
      interior because the head + flange both being wider than the hole
      made the original geometrically uninstallable. See `enclosure/3d/
      button_cap.py` history block for the rationale.)
- [ ] **F9: Cure all epoxies 24 h** before Phase G.

---

## Phase G — Final closeout

The compression-sandwich moment. Everything has been prepped to slot
together; now it does.

- [ ] **G1: Inspect the frame + face** (cured from E) interior. Remove any
      glue drips with a chisel. Dry-fit the light channel into the frame,
      letter-side aligned. It should slide in and rest against the back
      of the face — channel walls press against the diffusion film
      (which is bonded to the face back). If foam is in use as the
      top-edge seal, verify it's already applied to the channel's wall
      tops.
- [ ] **G2: First-light bleed assessment** *(post-2026-04-26 design;
      diffuser is film-only).* Before fastening anything, do a rough
      preview: hold the back-panel-plus-PCB assembly behind the
      frame+face, power on briefly via USB, light a few words
      (firmware in normal operation). Look at the face from typical
      viewing distance (~2 ft). If you see distinct LED hot spots
      bleeding through the cutouts, OR significant light bleed into
      filler letters, the acrylic-shelved-as-backup decision activates
      — disassemble, cut acrylic per-pocket, reprint the light channel
      at ~21 mm walls extending through the acrylic level. If face
      looks clean (uniform glow on lit words, dark on filler letters),
      proceed.
- [ ] **G3: Plug the micro-USB end** of the captive cable into the ESP32
      module on the PCB.
- [ ] **G4: Connect the speaker JST** to the PCB's speaker header.
- [ ] **G5: Place the PCB on the standoff posts** in the back panel. The
      PCB's 4 corners should rest on the 4 standoff tops with LEDs
      facing UP (toward the light channel / face). The standoff stubs
      locate the PCB without holding it down.
- [ ] **G6: Verify button alignment.** The PCB's 3 tact switches
      (SW1/SW2/SW3) must line up with the 3 button actuator caps in
      the back panel. If misaligned, the buttons won't press through —
      STOP and fix before closing.
- [ ] **G7: Lower the frame + face assembly** onto the back-panel-plus-PCB.
      PCB slides up into the frame interior, the light channel's top
      edge presses against the diffusion film on the face back. Back
      panel edges sit flush with frame bottom edges.
- [ ] **G8: Align back-panel corner clearance holes** with the frame
      walls below them. The 4 mm-inset holes fall over the 6.4 mm walls
      (not the interior corner) — that's intentional with the
      wood-screw approach.
- [ ] **G9: Pre-drill the frame wall** at each of the 4 corner positions
      with a ~2 mm pilot bit, going down through the back-panel
      clearance hole into the frame wall ~10-12 mm. Hardwood splits
      without a pilot, especially this close to a finger joint.
- [ ] **G10: Drive 4 × M3 × 1/2"–5/8" wood screws** through the back
      panel corner holes into the pilot holes in the frame wall. Hand-
      tight first, all 4, before any torquing.
- [ ] **G11: Torque in cross-pattern** (diagonal, not around the
      perimeter) to snug-not-gorilla-tight. The compression sandwich is
      now engaged: face → diffusion film → opal acrylic → light channel
      top edge (foam-sealed) → channel walls → PCB → standoffs → back
      panel. Wood screws hold all of it in tension against the frame
      walls.
- [ ] **G12: Power on via USB-C.** Flashing and daily operation use this
      port. If anything behaves wrong, loosen all 4 screws, lift back
      panel, debug; the clock is fully serviceable.

---

## Jig notes — keeping the frame square

The dominant risk in Phase B is a parallelogrammed frame box (all 4
angles off square but still closing). Three defenses:

1. **Dry-fit + diagonal measurement BEFORE glue touches the joints.**
   Opposite-corner distances must be equal. If not, the dry-fit itself
   is out of square; revisit kerf compensation before gluing.
2. **Corner clamps during cure**, not strap clamps — corner clamps pull
   90° explicitly; strap clamps only pull the perimeter and can tolerate
   a parallelogram.
3. **Re-check diagonals 5 min into cure.** Titebond III has ~20 min open
   time; if the box has racked under clamp pressure, there's still a
   window to correct.

A simple backup jig: 4 scrap wood blocks cut to exactly 90° (verified with
a framing square), placed in the 4 inside corners during cure. The blocks
physically enforce the right angle even if the clamps slip.

---

## Post-assembly validation

Before declaring the build done, walk through the Phase 2 hardware checks
(`firmware/test/hardware_checks/wifi_provision_checks.md` plus any other
module checklists as they're written). Assembly can introduce issues that
breadboard bring-up didn't catch: loose standoff post, speaker wire
pinched by the light channel, button actuator with too much friction.
Burn-in: 30 days of running on Dad's desk before declaring Emory done and
starting Nora.

---

## Parallel-track artifacts

Run these while epoxy cures — they're not blocked on the main sequence:

- Re-record voice memos if any earlier pass is stale
- Confirm SD card is FAT32 with the two audio files at root: `lullaby.wav`
  and `birth.wav`, 16-bit PCM 44.1 kHz mono little-endian (per `TODO.md`
  Phase 2 decisions; design pivoted from MP3 to uncompressed WAV during
  the audio-module spec pass to drop the MP3 decoder dependency)

---

## Long-term maintenance

### DS3231 CR2032 replacement — every 5 years

The DS3231 RTC module (ZS-042) keeps time during power outages on a CR2032 coin cell. In a mostly-powered clock the battery drain is minimal (~2 μA only during outages), so a typical 220 mAh cell has >10 year practical life. We replace every 5 years conservatively — this also keeps the cell voltage comfortably above the 2.6V threshold where the onboard trickle-charge circuit would start engaging (see `docs/hardware/pinout.md` §Critical Issues #1 for the full physics).

**Back panel is removable** via 4 × M3 wood screws threaded into the
frame wall — unscrew, swap cell, screw back. Takes 2 minutes per clock.
Wood screws into hardwood handle ~10-15 open/close cycles before threads
loosen meaningfully; for a 5-year battery cadence over 40 years (8
cycles), well within tolerance. If a thread does eventually strip, the
fix is to drill the pilot hole 0.5 mm deeper and step up to a slightly
longer screw.

**Replacement schedule:**

| Clock | First CR2032 install | Next replacement | Then every |
|---|---|---|---|
| Emory | TBD (~2026 breadboard → ~2030 delivery) | +5 years from install | 5 years |
| Nora | TBD (~2032 build) | +5 years from install | 5 years |

**Set calendar reminders at install time.** The clock won't warn you — the battery just goes flat, and on the next power outage the RTC loses the time and reverts to 2000-01-01 until NTP re-syncs. No catastrophic failure, but a 5-minute avoidable inconvenience.

**Why not remove the trickle-charge resistor instead?** Considered and rejected 2026-04-20. The trickle circuit is inert on 3.3V with a fresh cell (diode reverse-biased). The actual risk is end-of-life batteries, mitigated cheaper by the replacement schedule than by a desolder step that could damage the board.
