"""Button actuator cap — 3 of these per clock (Hour / Minute / Audio).

Each cap is dropped into a 6.5 mm hole in the back panel from the
INTERIOR side during assembly, bridges the ~15 mm air gap between panel
interior and tact-switch plunger, and transmits the user's press to the
switch.

Shape (exterior face at top, plunger contact at bottom):
         |
         |  NECK     <- 6.2 mm dia, slides through the 6.5 mm panel
         |              hole (0.15 mm clearance per side); top face is
         |              the press surface, sits 1 mm proud at rest
         |
    +----------+
    |          |    <- FLANGE: 8 mm dia, wider than the panel hole.
    |  FLANGE  |       Catches on the panel INTERIOR surface — both
    +----------+       the install retainer (drops in from inside,
         |             can't pass through the hole) and the rest stop
         |             (spring-loaded plunger pushes the cap back out
         |             until the flange hits the panel interior again).
         |  STEM     <- 5 mm dia; spans the air gap between panel
         |              interior and the tact-switch plunger
         |
       +---+
       |   |         <- ANVIL: 8.2 mm dia flat disc that presses the
       +---+            plunger anywhere within ±2 mm of nominal; its
                        cylindrical section rides in the carrier's
                        per-cap guide bore (see pcb_carrier.py)

Install procedure (Phase F8 of `docs/hardware/assembly-plan.md`):
  Lay back panel interior-side-up on the bench. Drop a cap into each
  button hole from above — neck enters the hole and protrudes out the
  exterior side, flange catches on the panel interior. Stem dangles
  free into the case toward the eventual plunger.

History: original design had an 8 mm HEAD on top of the neck so the
cap could be pushed in from outside and self-retain. That made it
geometrically impossible to install through a 6.5 mm hole (both the
head and the flange were 8 mm — neither could thread through). Fix
applied 2026-04-25: drop the head, install from interior. Caps
originally fell loose whenever the back panel came off; since
2026-07-11 each cap SNAPS through a hole in the carrier plate's
retention pad roof (pcb_carrier.py) — captive with the panel through
any service event, yet interchangeable: press the neck from the panel
exterior (board off) and the cap pops back out. Inward over-press is
limited by the tact switch's own travel rather than a head-on-panel
hard stop.

All dimensions in millimeters. Sources:
  - Tact switch geometry: DAOKI 6x6x4.3 mm 4-pin THT — total height
    4.3 mm. Plunger stands 1.6 mm above the body face (measured from
    the user's physical switch); body alone is therefore 2.7 mm.
    Plunger diameter 3.52 mm measured 2026-04-21.
  - Back-panel hole (6.5 mm) and panel thickness (3.2 mm) from
    `enclosure/scripts/render_back_panel.py`.
  - Air-gap budget: standoff 22 mm (base 2 + post 20) minus switch
    body 2.7 mm minus plunger extension 1.6 mm = 17.7 mm.

Print in PLA. Recommended orientation: neck down (cap's press face on
the build plate). The flange overhang above the neck is ~0.9 mm per
side, well within PLA's no-support angle. Stem points up; the plunger
pocket faces up so it's easy to clear of any stringing.

Run from repo root:
    enclosure/3d/.venv/bin/python enclosure/3d/button_cap.py

Output: enclosure/3d/out/button_cap.stl  (print 3 per clock)
"""
from build123d import *
from pathlib import Path

# ─── Measured / referenced parameters ────────────────────────────
# Tune these if a re-measure disagrees.

PLUNGER_DIA_MM            = 3.52  # measured
PLUNGER_HEIGHT_ABOVE_BODY =  1.6  # measured: plunger protrudes 1.6 mm
                                  # above the body's top face. The cap
                                  # pocket needs to wrap around this
                                  # height, not just kiss the tip.

# From render_back_panel.py + pcb_standoff.py:
PANEL_HOLE_DIA_MM   =  6.5
PANEL_THICKNESS_MM  =  3.2
STANDOFF_LIFT_MM    = 28.4  # carrier web 2.5 + post 25.9.
                            # STACK CORRECTION 2026-07-09: the frame cavity
                            # is the full 48.0 (face ON TOP of the walls,
                            # panel UNDERNEATH); the old 22.0 came from a
                            # table that counted face+panel inside the 48.
SWITCH_BODY_HEIGHT  =  2.7  # body alone (4.3 mm total - 1.6 mm plunger).
                            # Verify with calipers if your switches differ.

# Air gap from panel interior to plunger tip = standoff lift - body - plunger height
AIR_GAP_MM = STANDOFF_LIFT_MM - SWITCH_BODY_HEIGHT - PLUNGER_HEIGHT_ABOVE_BODY

# ─── Design parameters ───────────────────────────────────────────

# Neck: slides through the panel hole AND extends 1 mm above the panel
# exterior surface as the press surface. (No separate "head" — the neck
# top doubles as the finger pad. This is the simplification that made
# the cap installable: previously a wider HEAD on top of a NECK on top
# of a wider FLANGE meant neither end fit through the 6.5 mm hole.)
NECK_DIA_MM        = PANEL_HOLE_DIA_MM - 0.3  # 6.2 mm (0.15 mm clearance per side)
NECK_HEIGHT_MM     = PANEL_THICKNESS_MM + 2.5  # 5.7 mm — 2.5 mm proud at rest

# Flange: wider than the hole, catches on the panel INTERIOR surface.
# Doubles as install retainer (drop cap in from interior side, flange
# stops at the panel) and rest stop (spring pushes cap back out until
# flange hits panel interior again). 8 mm > 6.5 mm hole — never passes
# through, by design.
#
# SNAP FIT (2026-07-11): the flange also snaps through the carrier pad's
# roof hole (SNAP_HOLE_D_MM in pcb_carrier.py, ~7.9 nominal). Going IN,
# the 45° lower cone cams the flange through; coming OUT (press the neck
# from the panel exterior, board off), the REMOVAL CHAMFER on top of the
# rim cams it back. Between snaps the ⌀8 band rides captive under the
# roof lip. Calibrate the hole with pcb_carrier.py --part snap-test.
# RESIZED ⌀9→⌀8 (2026-07-12): ⌀8 still can't pass the 6.5 panel hole,
# and the smaller flange is what makes the carrier's per-cap guide bores
# printable at the 10 mm pitch (1.0 mm webs between ⌀9 bores).
FLANGE_DIA_MM      = 8.0    # > 6.5 panel hole; fits the ⌀9 guide bore
FLANGE_CONE_H_MM   = 0.9    # 45-deg cone from neck to flange = no support
FLANGE_BAND_H_MM   = 0.4    # cylindrical land at full ⌀8 (the snap lip
                            # engagement surface)
FLANGE_RCONE_H_MM  = 0.6    # removal chamfer, ⌀8 → ⌀6.8 (45°)
FLANGE_RCONE_TOP_D = 6.8
FLANGE_HEIGHT_MM   = (FLANGE_CONE_H_MM + FLANGE_BAND_H_MM
                      + FLANGE_RCONE_H_MM)

# Stem: bridges the air gap to the plunger AND extends slightly BELOW
# the plunger tip at rest, so the bottom of the stem (the pocket
# entrance) wraps AROUND the plunger rather than just resting on top
# of it. Without this capture, the cap can slide laterally off the
# plunger when pressed even slightly off-axis.
#
# ANVIL (2026-07-09, resized 2026-07-12): the stem ends in a flat disc
# that presses the plunger anywhere within ±2.0 mm of nominal — 4× the
# real position error a THT switch can have (4 pins in plated holes cap
# it at ~±0.5 mm; the "~3 mm offset" once measured on the bench was the
# mis-built early coupon, not the switch). The old plunger-hugging
# pocket is obsolete: the carrier's snap pocket keeps the cap captive
# and its GUIDE BORE (TOWER_BORE_D_MM in pcb_carrier.py) rides the
# anvil disc, holding the cap rigid at three heights (neck / flange /
# anvil) so it meets its switch square with no lean. The disc is 2.4
# tall so the bore guides the cylindrical section, not the cone; the
# cone is 42° to stay support-free printed neck-down.
AT_REST_GAP_MM     = 0.3    # anvil hovers this far above the plunger tip
STEM_DIA_MM        = 5.0
ANVIL_DIA_MM       = 8.2    # keep in sync with pcb_carrier.py CAP_ANVIL_D_MM
ANVIL_CONE_H_MM    = 1.8
ANVIL_T_MM         = 2.4
STEM_HEIGHT_MM     = (AIR_GAP_MM - FLANGE_HEIGHT_MM - ANVIL_CONE_H_MM
                      - ANVIL_T_MM - AT_REST_GAP_MM)

# Pocket at the bottom of the stem — wraps around the plunger.
# Pocket depth must exceed AT_REST_CAPTURE so the plunger tip doesn't
# touch the pocket ceiling at rest (would pre-press the switch).
#
# Click budget at full press: pocket ceiling needs to descend onto the
# plunger tip (POCKET_DEPTH - AT_REST_CAPTURE), then push it 0.3 mm
# to the click. Total cap travel for click = D - K + 0.3.
# Travel before stem bottoms on switch body = plunger_height - K = 1.0
# (with K = 0.6). For click before bottom-out: D + 0.3 < 1.6, i.e.
# D < 1.3. POCKET_DEPTH = 1.0 leaves 0.3 mm margin AND keeps the
# plunger tip 0.4 mm clear of the ceiling at rest.


# ─── Geometry ────────────────────────────────────────────────────
# Stack from bottom (build-plate) to top: NECK → FLANGE → STEM.
# Print orientation: neck-down (press face on the build plate); the
# flange overhang on the neck is ~0.9 mm per side and prints fine
# without support.

BOTTOM = (Align.CENTER, Align.CENTER, Align.MIN)

neck = Cylinder(NECK_DIA_MM / 2, NECK_HEIGHT_MM, align=BOTTOM)

flange_z = NECK_HEIGHT_MM
fcone = Pos(0, 0, flange_z) * Cone(
    NECK_DIA_MM / 2, FLANGE_DIA_MM / 2, FLANGE_CONE_H_MM, align=BOTTOM)
fband = Pos(0, 0, flange_z + FLANGE_CONE_H_MM) * Cylinder(
    FLANGE_DIA_MM / 2, FLANGE_BAND_H_MM, align=BOTTOM)
frcone = Pos(0, 0, flange_z + FLANGE_CONE_H_MM + FLANGE_BAND_H_MM) * Cone(
    FLANGE_DIA_MM / 2, FLANGE_RCONE_TOP_D / 2, FLANGE_RCONE_H_MM,
    align=BOTTOM)

stem_z = flange_z + FLANGE_HEIGHT_MM
stem = Pos(0, 0, stem_z) * Cylinder(
    STEM_DIA_MM / 2, STEM_HEIGHT_MM, align=BOTTOM)
acone_z = stem_z + STEM_HEIGHT_MM
acone = Pos(0, 0, acone_z) * Cone(
    STEM_DIA_MM / 2, ANVIL_DIA_MM / 2, ANVIL_CONE_H_MM, align=BOTTOM)
anvil = Pos(0, 0, acone_z + ANVIL_CONE_H_MM) * Cylinder(
    ANVIL_DIA_MM / 2, ANVIL_T_MM, align=BOTTOM)
cap = neck + fcone + fband + frcone + stem + acone + anvil

# ─── Export ──────────────────────────────────────────────────────

out_dir = Path(__file__).parent / "out"
out_dir.mkdir(exist_ok=True)
stl_path = out_dir / "button_cap.stl"
export_stl(cap, str(stl_path))

total_height = (NECK_HEIGHT_MM + FLANGE_HEIGHT_MM + STEM_HEIGHT_MM
                + ANVIL_CONE_H_MM + ANVIL_T_MM)
print(f"wrote {stl_path}")
print(f"  flange: {FLANGE_DIA_MM:.1f} dia, snap band {FLANGE_BAND_H_MM:.1f}"
      f" + removal chamfer to {FLANGE_RCONE_TOP_D:.1f} (support-free)")
print(f"  anvil:  {ANVIL_DIA_MM:.1f} dia flat — tolerates ±{(ANVIL_DIA_MM-PLUNGER_DIA_MM)/2:.1f} mm switch offset")
print(f"  total:  {total_height:.1f} mm  (stack lift {STANDOFF_LIFT_MM:.1f})")
