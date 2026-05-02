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
         +
         X           <- POCKET: small cavity at the bottom of the stem
         X              that fits over the plunger tip (0.15 mm
         +              clearance per side); keeps the cap centered
                        on the plunger so it doesn't slide sideways

Install procedure (Phase F8 of `docs/hardware/assembly-plan.md`):
  Lay back panel interior-side-up on the bench. Drop a cap into each
  button hole from above — neck enters the hole and protrudes out the
  exterior side, flange catches on the panel interior. Stem dangles
  free into the case toward the eventual plunger.

History: original design had an 8 mm HEAD on top of the neck so the
cap could be pushed in from outside and self-retain. That made it
geometrically impossible to install through a 6.5 mm hole (both the
head and the flange were 8 mm — neither could thread through). Fix
applied 2026-04-25: drop the head, install from interior, accept that
caps fall loose when the back panel is removed for service (manageable
on the 5-year battery cadence by holding the panel cap-side-up during
swaps). Inward over-press is now limited by the tact switch's own
travel rather than a head-on-panel hard stop.

All dimensions in millimeters. Sources:
  - Tact switch plunger diameter (3.52 mm) + height above switch body
    (1.5 mm) measured from a physical switch 2026-04-21.
  - Back-panel hole (6.5 mm) and panel thickness (3.2 mm) from
    `enclosure/scripts/render_back_panel.py`.
  - Air-gap budget: standoff 22 mm (base 2 + post 20) minus switch
    body ~5 mm minus plunger extension 1.5 mm = ~15.5 mm.

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
PLUNGER_HEIGHT_ABOVE_BODY =  1.5  # measured, approx

# From render_back_panel.py + pcb_standoff.py:
PANEL_HOLE_DIA_MM   =  6.5
PANEL_THICKNESS_MM  =  3.2
STANDOFF_LIFT_MM    = 22.0  # base 2 + post 20 (stub sits inside PCB)
SWITCH_BODY_HEIGHT  =  5.0  # typical SW_PUSH_6mm body above PCB; verify
                            # against your physical switches before committing

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
FLANGE_DIA_MM      = 8.0
FLANGE_HEIGHT_MM   = 1.0

# Stem: bridges the air gap to the plunger. Sized 0.2 mm shorter than
# the measured air gap so the cap is free to float when released
# (avoids pre-activating the switch at rest) but the press feels crisp
# rather than mushy — first 0.2 mm of press closes the gap, then 0.5 mm
# depresses the plunger.
STEM_DIA_MM        = 5.0
STEM_HEIGHT_MM     = AIR_GAP_MM - FLANGE_HEIGHT_MM - 0.2

# Pocket at the bottom of the stem — fits over the plunger tip.
POCKET_DIA_MM      = PLUNGER_DIA_MM + 0.3  # 0.15 mm clearance per side
POCKET_DEPTH_MM    = 3.0   # was 2.0 — deeper pocket better captures the
                           # plunger if cap-to-plunger lateral alignment
                           # is off by the worst-case tolerance stack

# ─── Geometry ────────────────────────────────────────────────────
# Stack from bottom (build-plate) to top: NECK → FLANGE → STEM.
# Print orientation: neck-down (press face on the build plate); the
# flange overhang on the neck is ~0.9 mm per side and prints fine
# without support.

BOTTOM = (Align.CENTER, Align.CENTER, Align.MIN)

neck = Cylinder(NECK_DIA_MM / 2, NECK_HEIGHT_MM, align=BOTTOM)

flange_z = NECK_HEIGHT_MM
flange = Pos(0, 0, flange_z) * Cylinder(
    FLANGE_DIA_MM / 2, FLANGE_HEIGHT_MM, align=BOTTOM
)

stem_z = flange_z + FLANGE_HEIGHT_MM
stem = Pos(0, 0, stem_z) * Cylinder(
    STEM_DIA_MM / 2, STEM_HEIGHT_MM, align=BOTTOM
)

solid = neck + flange + stem

# Pocket: subtract a cylinder from the top of the stem (which becomes
# the plunger contact face after the cap is flipped into install
# orientation).
pocket_z = stem_z + STEM_HEIGHT_MM - POCKET_DEPTH_MM
pocket = Pos(0, 0, pocket_z) * Cylinder(
    POCKET_DIA_MM / 2, POCKET_DEPTH_MM + 0.01, align=BOTTOM
)
cap = solid - pocket

# ─── Export ──────────────────────────────────────────────────────

out_dir = Path(__file__).parent / "out"
out_dir.mkdir(exist_ok=True)
stl_path = out_dir / "button_cap.stl"
export_stl(cap, str(stl_path))

total_height = NECK_HEIGHT_MM + FLANGE_HEIGHT_MM + STEM_HEIGHT_MM
print(f"wrote {stl_path}")
print(f"  neck:    {NECK_DIA_MM:>5.1f} mm dia x {NECK_HEIGHT_MM:>4.1f} mm "
      f"({NECK_HEIGHT_MM - PANEL_THICKNESS_MM:.1f} mm proud of "
      f"{PANEL_THICKNESS_MM:.1f} mm panel at rest)")
print(f"  flange:  {FLANGE_DIA_MM:>5.1f} mm dia x {FLANGE_HEIGHT_MM:>4.1f} mm "
      f"(catches on panel interior)")
print(f"  stem:    {STEM_DIA_MM:>5.1f} mm dia x {STEM_HEIGHT_MM:>4.1f} mm")
print(f"  pocket:  {POCKET_DIA_MM:>5.1f} mm dia x {POCKET_DEPTH_MM:>4.1f} mm deep")
print(f"  overall: {total_height:>5.1f} mm tall (air-gap budget {AIR_GAP_MM:.1f} mm)")
