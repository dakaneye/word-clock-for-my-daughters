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

---

## Current state (continuing later)

Last commit: `fix(enclosure): aspect filter for Jost B + simplify for Fraunces serifs`

**What's working:**
- 43 pytest tests passing
- Build script generates clean `emory-face.svg`, `nora-face.svg`, `frame.svg`
- `frame.svg` already verified in Ponoko's design checker, dimensions correct (192 × 207 mm)
- Both face SVGs include closed-counter struts (1mm width, asymmetric bridges)
- Self-review workflow: cairosvg-based rendering of SVGs to PNG at `/tmp/svg_review/` for visual inspection without round-tripping through Ponoko

**Latest renders** (saved locally — reviewed before committing):
- `/tmp/svg_review/zoom_jost_v21.png` — Jost Bold closed letters with struts, zoom
- `/tmp/svg_review/zoom_fraunces_v21.png` — Fraunces Medium closed letters, zoom
- `/tmp/svg_review/full_emory_v21.png` — full Emory face
- `/tmp/svg_review/full_nora_v21.png` — full Nora face

**Strut rules** (in `enclosure/scripts/path_ops.py::BRIDGE_RULES`):
- `O Q 0`: 2 vertical struts at 12 + 6 o'clock
- `R P D B`: 1 horizontal strut on left stem (B: per counter = 2 total)
- `A`: 1 horizontal strut on left diagonal
- `G 6 9`: 1 horizontal strut on left

**Strut geometry** (`path_ops.py::_apply_bridges`):
- Width: 1 mm (Chelsea's clock precedent — below Ponoko's published 3mm min; user confirmed 1mm works on their laser)
- Asymmetric bridge: `body_margin = 3mm` (punches through thick strokes) + `counter_margin = 1.5mm` (just visible inside counter, avoids hitting interior features)
- Filters: skip holes where `min(w,h) < strut_width` OR aspect > 2.0 (sliver/crossbar artifacts from overlapping primitives)
- Post-union cleanup: `buffer(0)` + `simplify(0.05 × strut_width)` to remove sub-laser-precision spikes

## Known remaining issues / open questions

1. **Fraunces letters at full-face scale may still show fine artifacts.** User reported "splash effect on almost every letter" on Fraunces non-strut letters in earlier renders. Changes since then:
   - `opsz` 14 → 72 (display variant, cleaner outlines)
   - Bezier flattening 16 → 64 steps
   - `simplify()` pass after union
   
   Still needs user confirmation that current render (v21) actually resolves the splash on non-strut Fraunces letters (T, I, H, etc.). If splash persists:
   - Try different weight (500 → 700 Bold → might render cleaner)
   - Tune `simplify` tolerance larger (trade: more smoothing vs letter fidelity)
   - Investigate if specific Fraunces axes (SOFT, WONK) need different settings despite being set to 0

2. **Jost B visual density.** B with 2 horizontal struts on left stem looks like "E with a bowl" at zoom. At actual face scale (10mm letter), the 1mm notches are only ~10% of letter height each → 2 struts = 20% notched → acceptable. User can confirm when reviewing full-face render.

3. **Ponoko design-checker verification for face SVGs.** Frame.svg is verified. Face SVGs haven't been re-uploaded since the strut fixes started. Once v21 passes user review, re-upload for final geometry check before ordering.

## How to pick up

1. `git log --oneline -20` for recent changes.
2. `cd enclosure/scripts && pytest` — should be 43/43.
3. Review latest renders:
   ```bash
   PY=/Library/Developer/CommandLineTools/usr/bin/python3
   DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib $PY <<'PYEOF'
   import re, sys, shutil
   sys.path.insert(0, '.')
   import cairosvg
   from pathlib import Path
   OUT = Path("/tmp/svg_review")
   OUT.mkdir(exist_ok=True)
   for name, src, fill, bg in [
       ("emory", Path("enclosure/emory-face.svg"), "#1a1208", "#dcc09a"),
       ("nora", Path("enclosure/nora-face.svg"), "#f0e0c0", "#3d2817"),
   ]:
       txt = src.read_text()
       txt = txt.replace('fill="none" stroke="#0000FF" stroke-width="0.01"', f'fill="{fill}"')
       txt = re.sub(r'(<svg [^>]*>)', lambda m: m.group(1) + f'<rect width="192" height="192" fill="{bg}"/>', txt, count=1)
       tmp = OUT / f"{name}_preview.svg"
       tmp.write_text(txt)
       cairosvg.svg2png(url=str(tmp), write_to=str(OUT / f"{name}_preview.png"), output_width=2000)
       print(f"Rendered {OUT / f'{name}_preview.png'}")
   PYEOF
   ```
4. Open `/tmp/svg_review/*.png` to review.
5. If struts or letter shapes still wrong: edit `enclosure/scripts/path_ops.py` (strut logic) or `enclosure/scripts/render_face.py` (font config, letter sizing).

## Relevant files

- Spec: `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`
- Implementation plan: `docs/archive/plans/2026-04-15-laser-cut-face-implementation.md` (archived — SVGs shipped)
- Grid source of truth: `firmware/lib/core/src/grid.cpp`
- PCB (for LED positions): `hardware/word-clock.kicad_pcb`
- Brainstorm session with visual history: `.superpowers/brainstorm/5705-1776357082/`
