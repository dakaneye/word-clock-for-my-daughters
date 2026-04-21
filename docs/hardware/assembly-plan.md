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
| M3 × 10 mm brass hex spacer, F-F, 5 mm AF | 4 | Embedded in frame corners; back panel screws thread into these |
| M3 × 12 mm brass machine screws (countersunk or pan-head) | 4 | Back-panel-to-frame closure |
| M2 × 6 mm self-tapping screws | 2 | Speaker tabs into 3D-printed cradle |
| Rubber grommet, 6 mm panel hole, ID ~3 mm | 1 | USB cable strain relief at back panel exit |
| M3 washers (optional) | 4 | Under screw heads on the back panel to protect wood |

**Adhesives**

| Joint | Adhesive | Why |
|---|---|---|
| Frame box joints (4 corners) | Titebond III wood glue | Strongest wood-to-wood; 20 min open time is enough for a 4-corner glue-up |
| Face top edge → frame top | Titebond III | Same joint type |
| M3 hex spacers → frame corners (interior) | JB Weld 2-part epoxy | Metal-to-wood; must resist screw torque for 40 years |
| PCB standoff posts → back panel interior | E6000 | PLA-to-wood; slight flex tolerates thermal cycling |
| Speaker cradle → back panel interior | E6000 | Same |
| Light channel → PCB front | none (press-fit to PCB LEDs) | Snap-fit by design; removable for service |
| Inner letter islands → face back | self-adhesive diffusion film | Ships pre-glued |

**Tools**

- 4 × corner clamps (Bessey right-angle or equivalent)
- 2-4 × bar clamps (face-to-frame glue-up)
- Framing square (12"+)
- Tape measure / caliper
- Sanding block, P220 grit
- Needle-nose tweezers (letter islands)
- Small Phillips screwdriver (M3)
- Scissors or X-Acto (diffusion film, acrylic)
- Masking tape (clamping substitute + alignment)

---

## Phase A — Pre-assembly prep

Do all of this before opening any glue. Most of these are independent and
can run in parallel.

- [ ] **Inspect Ponoko wood pieces.** Confirm face dimensions 192 × 192 ×
      3.2 mm, frame strips 192 × 48 × 6.4 mm × 4 strips, back panel
      192 × 192 × 3.2 mm. Sand any scorched edges with P220.
- [ ] **Assemble + fully test the final PCB** on the workbench, not inside
      the enclosure. Flash firmware, verify every peripheral works, then
      power off.
- [ ] **Print 3D internals.**
  - PCB standoff posts × 4
  - Button actuator caps × 3
  - Speaker cradle × 1
  - Light-channel honeycomb × 1
- [ ] **Face sub-assembly (D below) can happen now** — doesn't block on anything else.
- [ ] **Cut captive USB cable to internal length.** Measure from back-panel
      grommet hole position to ESP32's micro-USB port with ~50 mm slack.
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

## Phase C — Hex spacers into frame corners

The M3 hex spacers live inside the frame's 4 bottom-interior corners. Back
panel screws thread up into them from outside.

- [ ] **C1: Locate screw positions on the back panel.** The 4 M3
      through-holes in the back panel determine where the spacers must sit
      on the frame. Typical pattern: ~10 mm inset from each corner.
- [ ] **C2: Mark spacer positions on the frame.** Lay the back panel on the
      frame's bottom edge, mark through each M3 hole with a pencil onto
      the frame's bottom-interior surface.
- [ ] **C3: Mix JB Weld.** Equal parts resin + hardener, ~1 min mix.
- [ ] **C4: Epoxy the 4 hex spacers** at the marked positions. The spacer's
      axis must be perpendicular to the back-panel plane (i.e. pointing
      down out of the bottom opening). A drop of epoxy on the spacer's
      bottom flange, press into position, hold 60 s.
- [ ] **C5: Test-fit back panel + screws** before epoxy sets (5-10 min
      working time). If a screw doesn't thread cleanly, reposition the
      spacer while epoxy is still workable.
- [ ] **C6: Cure 24 h.**

---

## Phase D — Face sub-assembly

Can run in parallel with B/C.

- [ ] **D1: Face on clean bench, letter side down.**
- [ ] **D2: Peel adhesive diffusion film backing**, lay film adhesive-side
      down on the back of the face. Press from center outward to expel
      air bubbles. Any loose inner-letter islands in the apertures stick
      automatically; stray islands get placed with tweezers before the
      film fully adheres.
- [ ] **D3: Cut opal acrylic to fit.** Target ~178 × 178 mm (fits inside
      the 179.2 mm frame inner opening with ~0.5 mm clearance per side).
      X-Acto score + snap works on 2-3 mm acrylic, or use existing bandsaw.
- [ ] **D4: Leave acrylic floating** for now (it's retained by the light
      channel pressing against it from behind). Do NOT glue the acrylic to
      the diffusion film — it needs to be replaceable.

---

## Phase E — Face-to-frame glue-up

- [ ] **E1: Dry-fit the face on the frame.** Face 192 × 192 mm sits flush
      with the 192 × 192 mm frame top. All edges align.
- [ ] **E2: Apply Titebond III** in a continuous bead along all 4 top edges
      of the frame (the 6.4 mm wide top surface of each strip). Spread
      with a finger or spreader to a thin even layer.
- [ ] **E3: Place face letter-side up** on the frame top, align flush at
      all edges.
- [ ] **E4: Clamp with bar clamps** across the face, ideally 4 (one per
      side). Cauls (scrap wood between clamp and face) protect the wood.
- [ ] **E5: Wipe squeeze-out** at the joint line with a damp rag
      immediately.
- [ ] **E6: Cure 24 h.**

---

## Phase F — Back panel internals

Do F1–F8 with the back panel on the bench, still separate from the frame.

- [ ] **F1: Verify back-panel cutouts.** 4 × M3 corner screw holes, 3 ×
      button actuator holes (6.5 mm), 1 × USB cable grommet hole (6 mm),
      1 × speaker vent pattern. Clean up any char with P220.
- [ ] **F2: Install rubber grommet** in the USB cable hole. Lube with
      dish soap if stiff.
- [ ] **F3: Thread USB cable through the grommet** from outside (USB-C
      plug trails outside) to inside (micro-USB plug lies loose inside).
- [ ] **F4: Epoxy PCB standoff posts × 4** to the back panel interior at
      the PCB corner positions. Use E6000. Hold 60 s. Diagonals: measure
      corner-to-corner to confirm the 4 standoffs form a square matching
      the PCB footprint (177.8 × 177.8 mm).
- [ ] **F5: Epoxy speaker cradle** to the back panel interior behind the
      vent cutout. E6000, 60 s hold.
- [ ] **F6: Install speaker in cradle** (2 × M2 self-tapping screws into
      the cradle's mount tabs). Connect JST-PH 2-pin lead to the cradle's
      routing path but leave the other end loose for now.
- [ ] **F7: Install button actuator caps × 3** through the back-panel
      button holes. These are friction-fit tiered cylinders — push from
      outside, they seat against the inner shoulder.
- [ ] **F8: Cure all epoxies 24 h** before Phase G.

---

## Phase G — Final closeout

The compression-sandwich moment. Everything has been prepped to slot
together; now it does.

- [ ] **G1: Inspect the frame + face** (cured from E) interior. Remove any
      glue drips with a chisel. Dry-fit the light channel into the frame,
      letter-side aligned. It should slide in and rest against the back of
      the face.
- [ ] **G2: Plug the micro-USB end** of the captive cable into the ESP32
      module on the PCB.
- [ ] **G3: Connect the speaker JST** to the PCB's speaker header.
- [ ] **G4: Place the PCB on the standoff posts** in the back panel. The
      PCB's 4 corners should rest on the 4 standoff tops with LEDs facing
      UP (toward the light channel / face).
- [ ] **G5: Verify button alignment.** The PCB's 3 tact switches
      (SW1/SW2/SW3) must line up with the 3 button actuator caps in the
      back panel. If misaligned, the buttons won't press through — STOP
      and fix before closing.
- [ ] **G6: Lower the frame + face assembly** onto the back-panel-plus-PCB.
      PCB slides up into the frame interior, face-side light channel
      meets PCB front, back panel edges sit flush with frame bottom
      edges.
- [ ] **G7: Align back panel corner holes** with the 4 hex spacers in
      the frame. The screw holes should line up exactly.
- [ ] **G8: Insert 4 × M3 × 12 mm screws** through the back panel corner
      holes into the hex spacers. Hand-tight first, all 4, before any
      torquing.
- [ ] **G9: Torque in cross-pattern** (diagonal, not around the
      perimeter) to ~0.5 Nm (snug, not gorilla-tight). The
      compression sandwich is now engaged: light channel pressed against
      face and PCB, PCB held between standoffs and its light-channel side,
      all internals locked in with zero fasteners on the PCB itself.
- [ ] **G10: Power on via USB-C.** Flashing and daily operation use this
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
- Confirm SD card is FAT32 with the two audio files at root (per
  `TODO.md` Phase 2 decisions: `lullaby.mp3`, `birth.mp3`, 128 kbps CBR
  mono 44.1 kHz)

---

## Long-term maintenance

### DS3231 CR2032 replacement — every 5 years

The DS3231 RTC module (ZS-042) keeps time during power outages on a CR2032 coin cell. In a mostly-powered clock the battery drain is minimal (~2 μA only during outages), so a typical 220 mAh cell has >10 year practical life. We replace every 5 years conservatively — this also keeps the cell voltage comfortably above the 2.6V threshold where the onboard trickle-charge circuit would start engaging (see `docs/hardware/pinout.md` §Critical Issues #1 for the full physics).

**Back panel is removable** via 4 × M3 brass corner screws — unscrew, swap cell, screw back. Takes 2 minutes per clock.

**Replacement schedule:**

| Clock | First CR2032 install | Next replacement | Then every |
|---|---|---|---|
| Emory | TBD (~2026 breadboard → ~2030 delivery) | +5 years from install | 5 years |
| Nora | TBD (~2032 build) | +5 years from install | 5 years |

**Set calendar reminders at install time.** The clock won't warn you — the battery just goes flat, and on the next power outage the RTC loses the time and reverts to 2000-01-01 until NTP re-syncs. No catastrophic failure, but a 5-minute avoidable inconvenience.

**Why not remove the trickle-charge resistor instead?** Considered and rejected 2026-04-20. The trickle circuit is inert on 3.3V with a fresh cell (diode reverse-biased). The actual risk is end-of-life batteries, mitigated cheaper by the replacement schedule than by a desolder step that could damage the board.
