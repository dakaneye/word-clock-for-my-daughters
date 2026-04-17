# 3D-printed internals

Scripts that generate STL files for the parts inside the clock case:
PCB standoff posts, button actuator caps, speaker cradle, light channel.
Each script is parametric Python via [build123d](https://build123d.readthedocs.io/) —
edit a dimension at the top, re-run, get a new STL.

## First-time setup

```bash
cd enclosure/3d
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install -r requirements.txt
```

(The `.venv/` directory is gitignored — it's ~500 MB of cached
OpenCascade CAD geometry, rebuilt locally on demand.)

## Generate STLs

Generate one part:
```bash
enclosure/3d/.venv/bin/python enclosure/3d/pcb_standoff.py
```

Generate every part at once:
```bash
enclosure/3d/.venv/bin/python enclosure/3d/build_all.py
```

Outputs go to `enclosure/3d/out/*.stl` (gitignored — regenerate any time).

## Printer workflow

See `docs/hardware/3d-printing-setup.md` — Bambu Studio setup, A1
onboarding, per-part slicer settings, VS Code live-preview workflow.

## Printing one of these

1. Open Bambu Studio (the slicer that ships with the A1).
2. `File → Import → Import 3MF/STL` → pick the `.stl` from `out/`.
3. Orient the part on the build plate. For the standoff: flange down so
   the glue surface prints flat against the bed; no support needed.
4. Choose a filament profile (PLA is fine for every internal part).
5. Click *Slice → Send* to the printer.

## Parts in this directory

| Script | Part | Qty per clock | Design source |
|---|---|---|---|
| `pcb_standoff.py` | PCB standoff post | 4 | `docs/hardware/pinout.md` §standoff |
| (future) `button_cap.py` | Button actuator cap | 3 | `TODO.md` 3D-printed internals |
| (future) `speaker_cradle.py` | Speaker mount | 1 | `TODO.md` |
| (future) `light_channel.py` | Honeycomb LED isolator | 1 | `TODO.md` + `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md` §Track 5 |

## Authoring a new part — the quick shape

Every script in this directory follows the same three sections:

```python
"""Explain the part in plain English at the top."""
from build123d import *
from pathlib import Path

# ─── Parameters ──
# All dimensions in mm. Source of numbers in comments.
MY_DIAMETER = 10.0

# ─── Geometry ──
# Primitive shapes combined with +, -, &.
# `align=(CENTER, CENTER, MIN)` = bottom of shape at Z=0, centered in XY.
BOTTOM = (Align.CENTER, Align.CENTER, Align.MIN)
part = Cylinder(MY_DIAMETER / 2, 10, align=BOTTOM)

# ─── Export ──
out_dir = Path(__file__).parent / "out"
out_dir.mkdir(exist_ok=True)
export_stl(part, str(out_dir / "my_part.stl"))
```

## build123d quick reference

Primitive shapes:
```python
Cylinder(radius, height, align=...)
Box(length, width, height, align=...)
Sphere(radius, align=...)
```

Boolean combine:
```python
part = a + b      # union
part = a - b      # subtract
part = a & b      # intersect
```

Transforms (apply with `*`):
```python
moved  = Pos(x, y, z) * shape
rotated = Rot(rx, ry, rz) * shape   # degrees
```

More complex shapes via 2D sketches then `extrude()`:
```python
profile = Polygon((0,0), (10,0), (10,5), (0,5))
part = extrude(profile, amount=3)
```

Docs: https://build123d.readthedocs.io/en/latest/
