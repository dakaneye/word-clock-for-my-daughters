# Enclosure

Laser-cut SVG files for the daughters' word clocks (Ponoko-ready) plus the
generators that produce them. See `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`
for the design spec.

## Files

| File | Description | Material |
|---|---|---|
| `emory-face.svg` | Emory's face — 192×192 mm, Jost Bold letters | Maple Hardwood 3.20 mm |
| `emory-frame.svg` | Emory's 4 frame strips with box joints | Maple Hardwood 6.40 mm |
| `nora-face.svg` | Nora's face — 192×192 mm, Fraunces Medium letters | Walnut Hardwood 3.20 mm |
| `nora-frame.svg` | Nora's 4 frame strips with box joints | Walnut Hardwood 6.40 mm |
| `cardboard-test-face.svg` | Emory face on cardboard for validation cut | Cardboard ~3 mm |

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
