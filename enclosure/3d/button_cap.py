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
STANDOFF_LIFT_MM    = 22.0  # base 2 + post 20 (stub sits inside PCB)
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
FLANGE_DIA_MM      = 8.0
FLANGE_HEIGHT_MM   = 1.0

# Stem: bridges the air gap to the plunger AND extends slightly BELOW
# the plunger tip at rest, so the bottom of the stem (the pocket
# entrance) wraps AROUND the plunger rather than just resting on top
# of it. Without this capture, the cap can slide laterally off the
# plunger when pressed even slightly off-axis.
#
# AT_REST_CAPTURE: how far the plunger sits inside the pocket when the
# cap is at rest (flange against panel interior, spring force holding
# it up). 0.6 mm = ~37% of the 1.6 mm plunger height, deep enough to
# feel positively engaged with PLA ±0.2 mm tolerance.
AT_REST_CAPTURE_MM = 0.6
STEM_DIA_MM        = 5.0
STEM_HEIGHT_MM     = AIR_GAP_MM - FLANGE_HEIGHT_MM + AT_REST_CAPTURE_MM

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
POCKET_DIA_MM      = PLUNGER_DIA_MM + 0.3  # 0.15 mm clearance per side
POCKET_DEPTH_MM    = 1.0

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
