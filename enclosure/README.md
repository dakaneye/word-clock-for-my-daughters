# Enclosure

Laser-cut SVG files for the daughters' word clocks (Ponoko-ready) plus the
generators that produce them. See `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`
for the design spec.

## Files

| File | Description | Material |
|---|---|---|
| `emory-face.svg` | Emory's face — 192×192 mm, Jost Bold letters | Maple Hardwood 3.20 mm |
| `nora-face.svg` | Nora's face — 192×192 mm, Fraunces Medium letters | Walnut Hardwood 3.20 mm |
| `frame.svg` | 4 frame strips with box joints (geometry is kid-agnostic — same file used for both clocks, different material) | Maple 6.40 mm (Emory) or Walnut 6.40 mm (Nora) |

## Regenerating

```bash
cd enclosure/scripts
pip install -r requirements.txt
python build.py
```

Outputs all SVGs to `enclosure/`. Idempotent.

## Tests

```bash
cd enclosure/scripts
pytest
```
