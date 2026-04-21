"""Button actuator cap — 3 of these per clock (Hour / Minute / Audio).

Each cap slides through a 6.5 mm hole in the back panel, bridges the
~15 mm air gap between panel interior and tact-switch plunger, and
transmits the user's press to the switch.

Shape (exterior face at top, plunger contact at bottom):
    +----------+    <- HEAD: wider than the panel hole so it sits
    |          |       proud of the panel exterior and can't pull
    |   HEAD   |       through inward
    +----------+
         |
         |  NECK     <- slides through the 6.5 mm panel hole
         |              (0.15 mm clearance per side)
    +----------+
    |          |    <- FLANGE: wider than the panel hole so the cap
    |  FLANGE  |       can't pop out of the panel exterior under
    +----------+       the tact-switch spring
         |
         |
         |  STEM     <- spans the air gap between panel interior and
         |              the tact-switch plunger
         |
         |
         +
         X           <- POCKET: small cavity at the bottom of the stem
         X              that fits over the plunger tip (0.15 mm
         +              clearance per side); keeps the cap centered
                        on the plunger so it doesn't slide sideways

All dimensions in millimeters. Sources:
  - Tact switch plunger diameter (3.52 mm) + height above switch body
    (1.5 mm) measured from a physical switch 2026-04-21.
  - Back-panel hole (6.5 mm) and panel thickness (3.2 mm) from
    `enclosure/scripts/render_back_panel.py`.
  - Air-gap budget: standoff 22 mm (base 2 + post 20) minus switch
    body ~5 mm minus plunger extension 1.5 mm = ~15.5 mm.

Print in PLA. Recommended orientation on the build plate: head down
(cap sitting on its face) — avoids needing support under the flange
and leaves the stem-bottom pocket facing up (easier to clear of any
stringing).

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

# Head: wider than the panel hole, sits proud of the panel face.
HEAD_DIA_MM        = 8.0
HEAD_HEIGHT_MM     = 1.0   # how far the cap sticks out when resting

# Neck: slides through the panel hole.
NECK_DIA_MM        = PANEL_HOLE_DIA_MM - 0.3  # 0.15 mm clearance per side
NECK_HEIGHT_MM     = PANEL_THICKNESS_MM

# Flange: retains the cap against the panel interior.
FLANGE_DIA_MM      = HEAD_DIA_MM
FLANGE_HEIGHT_MM   = 1.0

# Stem: bridges the air gap to the plunger. Deliberately sized 0.5 mm
# shorter than the measured air gap so the cap is free to float when
# released — better to have a tiny rattle than to press the switch at
# rest, which would pre-activate it.
STEM_DIA_MM        = 5.0
STEM_HEIGHT_MM     = AIR_GAP_MM - FLANGE_HEIGHT_MM - 0.5

# Pocket at the bottom of the stem — fits over the plunger tip.
POCKET_DIA_MM      = PLUNGER_DIA_MM + 0.3  # 0.15 mm clearance per side
POCKET_DEPTH_MM    = 2.0

# ─── Geometry ────────────────────────────────────────────────────

BOTTOM = (Align.CENTER, Align.CENTER, Align.MIN)

head = Cylinder(HEAD_DIA_MM / 2, HEAD_HEIGHT_MM, align=BOTTOM)

neck = Pos(0, 0, HEAD_HEIGHT_MM) * Cylinder(
    NECK_DIA_MM / 2, NECK_HEIGHT_MM, align=BOTTOM
)

flange_z = HEAD_HEIGHT_MM + NECK_HEIGHT_MM
flange = Pos(0, 0, flange_z) * Cylinder(
    FLANGE_DIA_MM / 2, FLANGE_HEIGHT_MM, align=BOTTOM
)

stem_z = flange_z + FLANGE_HEIGHT_MM
stem = Pos(0, 0, stem_z) * Cylinder(
    STEM_DIA_MM / 2, STEM_HEIGHT_MM, align=BOTTOM
)

solid = head + neck + flange + stem

# Pocket: subtract a cylinder from the bottom of the stem.
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

total_height = HEAD_HEIGHT_MM + NECK_HEIGHT_MM + FLANGE_HEIGHT_MM + STEM_HEIGHT_MM
print(f"wrote {stl_path}")
print(f"  head:    {HEAD_DIA_MM:>5.1f} mm dia x {HEAD_HEIGHT_MM:>4.1f} mm")
print(f"  neck:    {NECK_DIA_MM:>5.1f} mm dia x {NECK_HEIGHT_MM:>4.1f} mm")
print(f"  flange:  {FLANGE_DIA_MM:>5.1f} mm dia x {FLANGE_HEIGHT_MM:>4.1f} mm")
print(f"  stem:    {STEM_DIA_MM:>5.1f} mm dia x {STEM_HEIGHT_MM:>4.1f} mm")
print(f"  pocket:  {POCKET_DIA_MM:>5.1f} mm dia x {POCKET_DEPTH_MM:>4.1f} mm deep")
print(f"  overall: {total_height:>5.1f} mm tall (air-gap budget {AIR_GAP_MM:.1f} mm)")
