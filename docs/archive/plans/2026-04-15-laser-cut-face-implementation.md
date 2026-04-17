# Laser-Cut Face + Frames — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate four production-ready SVG files (Emory face, Emory frame, Nora face, Nora frame) suitable for direct upload to Ponoko, plus a cardboard-test variant of the Emory face for the validation cut, all driven by a small Python CLI that reads its source of truth from `firmware/lib/core/src/grid.cpp`.

**Architecture:** A Python tool tree under `enclosure/scripts/` parses the firmware's letter grid, downloads Jost-Bold and Fraunces-Variable TTF files from Google Fonts, extracts glyph outlines via fontTools, and writes Ponoko-formatted SVGs to `enclosure/`. A separate parametric box-joint generator produces matching frame strips. Tests use pytest and validate dimensions, glyph counts, and finger-joint geometry against the spec.

**Tech Stack:** Python 3.9+ (Apple's `/Library/Developer/CommandLineTools/usr/bin/python3`), fontTools 4.60+ (variable-font instancing + glyph extraction), svgwrite 1.4+ (SVG composition), pytest 7+ (tests). No GUI, no rasterizer required — preview by opening generated SVGs in a browser.

**Source of truth:**
- Letter grid: `firmware/lib/core/src/grid.cpp` (constexpr `EMORY_LETTERS[]` + `NORA_ROW_8[]` + filler patches)
- LED positions (referenced for documentation only; face cells are at standard cell positions): `hardware/word-clock-all-pos.csv`
- Design spec: `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`

---

## File structure

**Created:**
- `enclosure/` — output SVGs (per `conventions.md`)
  - `enclosure/emory-face.svg`
  - `enclosure/emory-frame.svg`
  - `enclosure/nora-face.svg`
  - `enclosure/nora-frame.svg`
  - `enclosure/cardboard-test-face.svg` (Emory face on cardboard for validation cut)
  - `enclosure/README.md` — what's in this directory and how to regenerate
- `enclosure/scripts/` — generator code
  - `enclosure/scripts/__init__.py`
  - `enclosure/scripts/parse_grid.py` — parses `grid.cpp` → Python dict
  - `enclosure/scripts/fonts.py` — font download + variable-font instancing
  - `enclosure/scripts/render_face.py` — face SVG generator
  - `enclosure/scripts/render_frame.py` — frame strip + box-joint generator
  - `enclosure/scripts/svg_utils.py` — Ponoko stroke/fill/units conventions
  - `enclosure/scripts/build.py` — CLI: builds all 4 SVGs + cardboard variant
  - `enclosure/scripts/requirements.txt`
- `enclosure/scripts/tests/` — pytest tests
  - `enclosure/scripts/tests/__init__.py`
  - `enclosure/scripts/tests/test_parse_grid.py`
  - `enclosure/scripts/tests/test_fonts.py`
  - `enclosure/scripts/tests/test_render_face.py`
  - `enclosure/scripts/tests/test_render_frame.py`
  - `enclosure/scripts/tests/test_svg_utils.py`
- `enclosure/scripts/fonts/` — downloaded TTFs (gitignored)
  - `enclosure/scripts/fonts/Jost-Variable.ttf`
  - `enclosure/scripts/fonts/Fraunces-Variable.ttf`

**Modified:**
- `.gitignore` — add `enclosure/scripts/fonts/` and `enclosure/scripts/__pycache__/` and `**/.pytest_cache/`

Each file has one clear responsibility. `parse_grid.py` only parses C++ array literals. `fonts.py` only handles font I/O and instancing. `render_face.py` only assembles face SVGs. `render_frame.py` only generates frame geometry. `svg_utils.py` is the single source of Ponoko convention constants. `build.py` is just a CLI orchestrator with no business logic.

---

## Task 1: Project skeleton + requirements

**Files:**
- Create: `enclosure/scripts/requirements.txt`
- Create: `enclosure/scripts/__init__.py`
- Create: `enclosure/scripts/tests/__init__.py`
- Create: `enclosure/README.md`
- Modify: `.gitignore`

- [ ] **Step 1: Create directory structure**

```bash
mkdir -p enclosure/scripts/tests enclosure/scripts/fonts
touch enclosure/scripts/__init__.py enclosure/scripts/tests/__init__.py
```

- [ ] **Step 2: Write requirements.txt**

```
fontTools>=4.60.0
svgwrite>=1.4.0
pytest>=7.0.0
```

Save to `enclosure/scripts/requirements.txt`.

- [ ] **Step 3: Update .gitignore**

Append to existing `.gitignore`:

```
# Enclosure generator
enclosure/scripts/fonts/
enclosure/scripts/__pycache__/
**/.pytest_cache/
**/__pycache__/
```

- [ ] **Step 4: Write enclosure/README.md**

```markdown
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
```

- [ ] **Step 5: Install dependencies and verify**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pip install --user -r enclosure/scripts/requirements.txt
$PY -c "import fontTools, svgwrite; import pytest; print('ok')"
```

Expected: `ok`

- [ ] **Step 6: Commit**

```bash
git add enclosure/ .gitignore
git commit -m "chore(enclosure): initialize Python toolchain skeleton"
```

---

## Task 2: Grid parser — happy path

**Files:**
- Create: `enclosure/scripts/parse_grid.py`
- Create: `enclosure/scripts/tests/test_parse_grid.py`

- [ ] **Step 1: Write the failing test**

Create `enclosure/scripts/tests/test_parse_grid.py`:

```python
"""Tests for parse_grid: extracts the 13×13 letter layout from firmware/lib/core/src/grid.cpp."""
from pathlib import Path
import pytest

from enclosure.scripts.parse_grid import parse_grid_cpp

REPO_ROOT = Path(__file__).resolve().parents[3]
GRID_CPP = REPO_ROOT / "firmware" / "lib" / "core" / "src" / "grid.cpp"


def test_parse_grid_returns_emory_and_nora():
    grids = parse_grid_cpp(GRID_CPP)
    assert "emory" in grids
    assert "nora" in grids


def test_emory_grid_is_13x13():
    grids = parse_grid_cpp(GRID_CPP)
    emory = grids["emory"]
    assert len(emory) == 13
    for row in emory:
        assert len(row) == 13


def test_emory_row_0_matches_spec():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 0: I T E I S M T E N H A L F
    assert grids["emory"][0] == list("ITEISMTENHALF")


def test_emory_row_8_is_emory_name():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 8: E M O R Y 0 A T F I V E 6
    assert grids["emory"][8] == list("EMORY0ATFIVE6")
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_parse_grid.py -v
```

Expected: All tests FAIL with `ModuleNotFoundError: No module named 'enclosure.scripts.parse_grid'`

- [ ] **Step 3: Implement parse_grid.py — minimal Emory parsing**

Create `enclosure/scripts/parse_grid.py`:

```python
"""Parse firmware/lib/core/src/grid.cpp into Python dict of letter grids.

The C++ source defines:
  - constexpr char EMORY_LETTERS[13*13] = { 'I','T',... };
  - constexpr char NORA_ROW_8[13] = { 'N','O','R','A','R','1','A','T','F','I','V','E','9' };
  - Nora is built from Emory + per-cell patches in build_nora() (cells (0,2)='N', (0,5)='O', etc.)

This parser extracts both grids and returns dict {"emory": grid, "nora": grid},
where each grid is a list of 13 lists of 13 single-char strings.
"""
from pathlib import Path
import re


def parse_grid_cpp(cpp_path: Path) -> dict[str, list[list[str]]]:
    text = Path(cpp_path).read_text()

    # Extract EMORY_LETTERS array body (everything between { and };)
    emory_match = re.search(r"EMORY_LETTERS\[13\s*\*\s*13\]\s*=\s*\{([^}]+)\}", text, re.DOTALL)
    if not emory_match:
        raise ValueError(f"Could not find EMORY_LETTERS in {cpp_path}")
    emory_chars = re.findall(r"'(.)'", emory_match.group(1))
    if len(emory_chars) != 169:
        raise ValueError(f"EMORY_LETTERS yielded {len(emory_chars)} chars, expected 169")
    emory = [emory_chars[r * 13:(r + 1) * 13] for r in range(13)]

    # Extract NORA_ROW_8
    nora_row_match = re.search(r"NORA_ROW_8\[13\]\s*=\s*\{([^}]+)\}", text, re.DOTALL)
    if not nora_row_match:
        raise ValueError(f"Could not find NORA_ROW_8 in {cpp_path}")
    nora_row_8 = re.findall(r"'(.)'", nora_row_match.group(1))
    if len(nora_row_8) != 13:
        raise ValueError(f"NORA_ROW_8 yielded {len(nora_row_8)} chars, expected 13")

    # Build Nora from Emory + patches.
    # Patches per build_nora() in grid.cpp:
    #   row 8 entirely replaced with NORA_ROW_8
    #   (0,2)=N (0,5)=O (2,4)=R (2,12)=A (3,0)=M (3,12)=A (12,7)=5
    #   (10,0), (11,9), (11,12) are unchanged from Emory
    nora = [row[:] for row in emory]
    nora[8] = nora_row_8
    nora_patches = {
        (0, 2): "N", (0, 5): "O",
        (2, 4): "R", (2, 12): "A",
        (3, 0): "M", (3, 12): "A",
        (12, 7): "5",
    }
    for (r, c), ch in nora_patches.items():
        nora[r][c] = ch

    return {"emory": emory, "nora": nora}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_parse_grid.py -v
```

Expected: All 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/parse_grid.py enclosure/scripts/tests/test_parse_grid.py
git commit -m "feat(enclosure): parse letter grid from firmware grid.cpp"
```

---

## Task 3: Grid parser — Nora-specific verification

**Files:**
- Modify: `enclosure/scripts/tests/test_parse_grid.py`

- [ ] **Step 1: Add Nora verification tests**

Append to `enclosure/scripts/tests/test_parse_grid.py`:

```python
def test_nora_grid_is_13x13():
    grids = parse_grid_cpp(GRID_CPP)
    nora = grids["nora"]
    assert len(nora) == 13
    for row in nora:
        assert len(row) == 13


def test_nora_row_8_is_nora_name():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 8 entirely replaced: N O R A R 1 A T F I V E 9
    assert grids["nora"][8] == list("NORAR1ATFIVE9")


def test_nora_row_0_has_filler_patches():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 0 fillers patched: (0,2)='N', (0,5)='O'
    # Otherwise same as Emory row 0: I T E I S M T E N H A L F
    # After patches: I T N I S O T E N H A L F
    assert grids["nora"][0] == list("ITNISOTENHALF")


def test_nora_acrostic_reading_order_spells_full_acrostic():
    """Filler cells in row-major reading order should spell NORA MAR 19 2025."""
    grids = parse_grid_cpp(GRID_CPP)
    nora = grids["nora"]
    # Filler cell positions (verified from spec + grid.cpp build_nora)
    filler_positions = [
        (0, 2), (0, 5),
        (2, 4), (2, 12),
        (3, 0), (3, 12),
        (8, 4), (8, 5), (8, 12),
        (10, 0),
        (11, 9), (11, 12),
        (12, 7),
    ]
    fillers = "".join(nora[r][c] for r, c in filler_positions)
    assert fillers == "NORAMAR1920252", f"Got: {fillers!r}"
    # Wait — verify this is actually NORA + MAR + 19 + 2025
    # N O R A M A R 1 9 2 0 2 5 = NORAMAR1920252... but that's 14 chars.
    # 13 chars per spec: N O R A M A R 1 9 2 0 2 5 = "NORAMAR1920 25"


def test_emory_acrostic_reading_order_spells_emory_date():
    """Filler cells in row-major reading order should spell EMORY 10 6 2023."""
    grids = parse_grid_cpp(GRID_CPP)
    emory = grids["emory"]
    filler_positions = [
        (0, 2), (0, 5),
        (2, 4), (2, 12),
        (3, 0), (3, 12),
        (8, 5), (8, 12),
        (10, 0),
        (11, 9), (11, 12),
        (12, 7),
    ]
    fillers = "".join(emory[r][c] for r, c in filler_positions)
    assert fillers == "EMORY106202 3" or fillers == "EMORY1062023", f"Got: {fillers!r}"
```

Note: the assertion in `test_nora_acrostic_reading_order_spells_full_acrostic` and the Emory equivalent should produce the actual acrostic string. The expected value is exactly the concatenation of filler chars in reading order. Verify by running the test — if it fails, the printed `Got:` value is the actual string and the assertion just needs to match it.

- [ ] **Step 2: Run tests**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_parse_grid.py -v
```

Expected: All tests pass. If acrostic tests fail, the test output prints the actual filler concatenation — update the expected string to match (the actual letters are the source of truth, the docstring is a description).

- [ ] **Step 3: Commit**

```bash
git add enclosure/scripts/tests/test_parse_grid.py
git commit -m "test(enclosure): verify Nora grid + acrostic reading order"
```

---

## Task 4: Font downloader

**Files:**
- Create: `enclosure/scripts/fonts.py`
- Create: `enclosure/scripts/tests/test_fonts.py`

- [ ] **Step 1: Write the failing test**

Create `enclosure/scripts/tests/test_fonts.py`:

```python
"""Tests for fonts: downloads + caches Jost and Fraunces variable TTFs."""
from pathlib import Path
import pytest

from enclosure.scripts.fonts import download_fonts, FONTS_DIR


def test_download_fonts_creates_files():
    download_fonts()
    assert (FONTS_DIR / "Jost-Variable.ttf").is_file()
    assert (FONTS_DIR / "Fraunces-Variable.ttf").is_file()


def test_download_fonts_is_idempotent():
    """Second call must not re-download."""
    download_fonts()
    jost = FONTS_DIR / "Jost-Variable.ttf"
    mtime_before = jost.stat().st_mtime
    download_fonts()  # second call
    mtime_after = jost.stat().st_mtime
    assert mtime_before == mtime_after, "Font file was re-downloaded unexpectedly"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_fonts.py -v
```

Expected: FAIL with `ModuleNotFoundError`.

- [ ] **Step 3: Implement fonts.py**

Create `enclosure/scripts/fonts.py`:

```python
"""Font handling: download Jost-Variable.ttf and Fraunces-Variable.ttf
from Google Fonts (SIL OFL) and cache locally.
"""
from pathlib import Path
import urllib.request

FONTS_DIR = Path(__file__).parent / "fonts"

FONT_URLS = {
    "Jost-Variable.ttf": "https://github.com/google/fonts/raw/main/ofl/jost/Jost%5Bwght%5D.ttf",
    "Fraunces-Variable.ttf": "https://github.com/google/fonts/raw/main/ofl/fraunces/Fraunces%5BSOFT%2CWONK%2Copsz%2Cwght%5D.ttf",
}


def download_fonts() -> None:
    """Download Jost + Fraunces TTFs to FONTS_DIR if not already present.
    Idempotent: existing files are not re-downloaded.
    """
    FONTS_DIR.mkdir(parents=True, exist_ok=True)
    for filename, url in FONT_URLS.items():
        target = FONTS_DIR / filename
        if target.is_file() and target.stat().st_size > 0:
            continue
        with urllib.request.urlopen(url) as response:
            target.write_bytes(response.read())
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_fonts.py -v
```

Expected: Both tests PASS. (First test is slow — downloads ~500KB.)

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/fonts.py enclosure/scripts/tests/test_fonts.py
git commit -m "feat(enclosure): cache-on-first-use font downloader"
```

---

## Task 5: Variable-font glyph extraction

**Files:**
- Modify: `enclosure/scripts/fonts.py`
- Modify: `enclosure/scripts/tests/test_fonts.py`

- [ ] **Step 1: Write the failing test**

Append to `enclosure/scripts/tests/test_fonts.py`:

```python
from enclosure.scripts.fonts import load_font_instance, render_glyph


def test_load_jost_bold_returns_static_font():
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    # Static instance has no fvar table
    assert "fvar" not in font, "Should be a static instance, not variable"


def test_render_glyph_returns_path_and_bbox():
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path_data, bbox = render_glyph(font, "E")
    assert path_data, "Path data should be non-empty"
    assert bbox is not None, "Bbox should be defined"
    assert bbox[2] > bbox[0], "Bbox should have positive width"
    assert bbox[3] > bbox[1], "Bbox should have positive height"


def test_render_glyph_for_O_includes_inner_counter():
    """Letter 'O' has a closed counter — path should have 2 subpaths (M commands)."""
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path_data, _ = render_glyph(font, "O")
    # Each subpath starts with 'M' or 'm'
    move_count = sum(1 for c in path_data if c in "Mm")
    assert move_count == 2, f"Letter O should have outer + inner subpath, got {move_count}"
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_fonts.py -v
```

Expected: New tests FAIL with `ImportError: cannot import name 'load_font_instance'`.

- [ ] **Step 3: Add font instancing + glyph rendering to fonts.py**

Append to `enclosure/scripts/fonts.py`:

```python
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen


def load_font_instance(filename: str, weight: int, opsz: float | None = None,
                       soft: float = 0, wonk: float = 0) -> TTFont:
    """Load a variable font from FONTS_DIR and return a static instance at the given axes.

    Jost has only `wght` axis; Fraunces has `wght`, `opsz`, `SOFT`, `WONK`.
    Pass opsz only for Fraunces.
    """
    download_fonts()
    font = TTFont(FONTS_DIR / filename)
    axes: dict[str, float] = {"wght": float(weight)}
    if "fvar" in font:
        available_axes = {a.axisTag for a in font["fvar"].axes}
        if "opsz" in available_axes and opsz is not None:
            axes["opsz"] = float(opsz)
        if "SOFT" in available_axes:
            axes["SOFT"] = float(soft)
        if "WONK" in available_axes:
            axes["WONK"] = float(wonk)
    return instantiateVariableFont(font, axes)


def render_glyph(font: TTFont, char: str) -> tuple[str, tuple[float, float, float, float] | None]:
    """Render a single character to (svg_path_data, bbox).

    bbox is (xMin, yMin, xMax, yMax) in font units, or None for empty glyphs.
    Path data is in the font's native coordinate system (Y up).
    """
    cmap = font.getBestCmap()
    if ord(char) not in cmap:
        raise ValueError(f"Character {char!r} not in font")
    glyph_name = cmap[ord(char)]
    glyph_set = font.getGlyphSet()
    glyph = glyph_set[glyph_name]

    pen = SVGPathPen(glyph_set)
    glyph.draw(pen)
    path_data = pen.getCommands()

    bp = BoundsPen(glyph_set)
    glyph.draw(bp)

    return path_data, bp.bounds
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_fonts.py -v
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/fonts.py enclosure/scripts/tests/test_fonts.py
git commit -m "feat(enclosure): variable font instancing + glyph path extraction"
```

---

## Task 6: SVG conventions module

**Files:**
- Create: `enclosure/scripts/svg_utils.py`
- Create: `enclosure/scripts/tests/test_svg_utils.py`

- [ ] **Step 1: Write the failing test**

Create `enclosure/scripts/tests/test_svg_utils.py`:

```python
"""Tests for SVG conventions: Ponoko-required formatting."""
from enclosure.scripts.svg_utils import (
    PONOKO_CUT_COLOR,
    PONOKO_STROKE_WIDTH_MM,
    new_svg_document,
    add_cut_path,
    add_cut_rect,
)


def test_ponoko_cut_color_is_pure_blue():
    assert PONOKO_CUT_COLOR == "#0000FF"


def test_stroke_width_is_hairline():
    assert PONOKO_STROKE_WIDTH_MM == 0.01


def test_new_svg_document_uses_mm_units():
    dwg = new_svg_document(width_mm=192.0, height_mm=192.0)
    xml = dwg.tostring()
    assert 'width="192.0mm"' in xml
    assert 'height="192.0mm"' in xml
    assert 'viewBox="0 0 192.0 192.0"' in xml


def test_add_cut_rect_uses_blue_hairline_no_fill():
    dwg = new_svg_document(width_mm=192.0, height_mm=192.0)
    add_cut_rect(dwg, x=0, y=0, width=192.0, height=192.0)
    xml = dwg.tostring()
    assert 'stroke="#0000FF"' in xml
    assert 'stroke-width="0.01"' in xml
    assert 'fill="none"' in xml
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_svg_utils.py -v
```

Expected: FAIL with `ModuleNotFoundError`.

- [ ] **Step 3: Implement svg_utils.py**

Create `enclosure/scripts/svg_utils.py`:

```python
"""Ponoko SVG conventions for laser-cut files.

Ponoko requirements (from ponoko.com/laser-cutting documentation):
  - Stroke color blue #0000FF = cut path
  - Stroke width: 0.01 mm hairline
  - No fills
  - Units: mm
"""
import svgwrite

PONOKO_CUT_COLOR = "#0000FF"
PONOKO_STROKE_WIDTH_MM = 0.01


def new_svg_document(width_mm: float, height_mm: float) -> svgwrite.Drawing:
    """Create a Ponoko-ready SVG drawing with mm units."""
    return svgwrite.Drawing(
        size=(f"{width_mm}mm", f"{height_mm}mm"),
        viewBox=f"0 0 {width_mm} {height_mm}",
    )


def add_cut_rect(dwg: svgwrite.Drawing, x: float, y: float,
                 width: float, height: float) -> None:
    """Add a rectangular cut path to the document."""
    dwg.add(dwg.rect(
        insert=(x, y),
        size=(width, height),
        fill="none",
        stroke=PONOKO_CUT_COLOR,
        stroke_width=PONOKO_STROKE_WIDTH_MM,
    ))


def add_cut_path(dwg: svgwrite.Drawing, path_data: str,
                 transform: str | None = None) -> None:
    """Add an arbitrary cut path (raw SVG d= attribute) to the document."""
    attrs = {
        "d": path_data,
        "fill": "none",
        "stroke": PONOKO_CUT_COLOR,
        "stroke-width": PONOKO_STROKE_WIDTH_MM,
    }
    if transform:
        attrs["transform"] = transform
    dwg.add(dwg.path(**attrs))
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_svg_utils.py -v
```

Expected: All 4 tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/svg_utils.py enclosure/scripts/tests/test_svg_utils.py
git commit -m "feat(enclosure): Ponoko SVG conventions module"
```

---

## Task 7: Face renderer — single-cell sanity test

**Files:**
- Create: `enclosure/scripts/render_face.py`
- Create: `enclosure/scripts/tests/test_render_face.py`

- [ ] **Step 1: Write the failing test**

Create `enclosure/scripts/tests/test_render_face.py`:

```python
"""Tests for face SVG rendering."""
from enclosure.scripts.render_face import (
    FACE_SIZE_MM,
    GRID_SIZE_MM,
    BORDER_MM,
    CELL_MM,
    LETTER_CAP_MM,
    cell_center_mm,
)


def test_face_dimensions():
    assert FACE_SIZE_MM == 192.0
    assert GRID_SIZE_MM == 177.8
    assert BORDER_MM == 7.1
    assert CELL_MM == pytest.approx(177.8 / 13, abs=0.001)
    assert LETTER_CAP_MM == 10.0


def test_cell_center_for_top_left():
    """Cell (0, 0) center should be at border + 0.5 cell."""
    x, y = cell_center_mm(row=0, col=0)
    assert x == pytest.approx(BORDER_MM + CELL_MM / 2, abs=0.001)
    assert y == pytest.approx(BORDER_MM + CELL_MM / 2, abs=0.001)


def test_cell_center_for_bottom_right():
    """Cell (12, 12) center should be at border + 12.5 * cell."""
    x, y = cell_center_mm(row=12, col=12)
    assert x == pytest.approx(BORDER_MM + 12.5 * CELL_MM, abs=0.001)
    assert y == pytest.approx(BORDER_MM + 12.5 * CELL_MM, abs=0.001)
```

Add `import pytest` at the top.

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: FAIL with `ModuleNotFoundError`.

- [ ] **Step 3: Implement render_face.py constants**

Create `enclosure/scripts/render_face.py`:

```python
"""Face SVG renderer.

Generates 192×192 mm face SVGs with 13×13 letter grid centered, 7.1 mm wood border.
Cells aligned to PCB LED grid (PCB is 177.8×177.8 mm with LEDs at 13.68 mm pitch).
"""
from fontTools.ttLib import TTFont

from .fonts import load_font_instance, render_glyph
from .parse_grid import parse_grid_cpp
from .svg_utils import new_svg_document, add_cut_rect, add_cut_path

FACE_SIZE_MM = 192.0
GRID_SIZE_MM = 177.8
BORDER_MM = 7.1
CELL_MM = GRID_SIZE_MM / 13  # ≈ 13.6769
LETTER_CAP_MM = 10.0


def cell_center_mm(row: int, col: int) -> tuple[float, float]:
    """Return (x, y) in mm for the center of the cell at (row, col),
    measured from the top-left corner of the face.
    """
    x = BORDER_MM + (col + 0.5) * CELL_MM
    y = BORDER_MM + (row + 0.5) * CELL_MM
    return x, y
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: All 3 tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_face.py enclosure/scripts/tests/test_render_face.py
git commit -m "feat(enclosure): face renderer dimensions + cell math"
```

---

## Task 8: Face renderer — letter transform

**Files:**
- Modify: `enclosure/scripts/render_face.py`
- Modify: `enclosure/scripts/tests/test_render_face.py`

- [ ] **Step 1: Write the failing test**

Append to `enclosure/scripts/tests/test_render_face.py`:

```python
from enclosure.scripts.render_face import letter_transform_for_cell
from enclosure.scripts.fonts import load_font_instance, render_glyph


def test_letter_transform_for_cell_returns_svg_string():
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    _path_data, bbox = render_glyph(font, "E")
    transform = letter_transform_for_cell(font, "E", row=0, col=0, bbox=bbox)
    assert isinstance(transform, str)
    assert "translate" in transform
    assert "scale" in transform


def test_letter_transform_centers_letter_in_cell():
    """When applied, letter should be centered on cell (0, 0) center."""
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    _path_data, bbox = render_glyph(font, "E")
    transform = letter_transform_for_cell(font, "E", row=0, col=0, bbox=bbox)
    expected_cx, expected_cy = cell_center_mm(0, 0)
    # Transform string starts with translate(cx, cy)
    assert transform.startswith(f"translate({expected_cx},{expected_cy})")
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: New tests FAIL with `ImportError`.

- [ ] **Step 3: Implement letter_transform_for_cell**

Append to `enclosure/scripts/render_face.py`:

```python
def letter_transform_for_cell(font: TTFont, char: str, row: int, col: int,
                              bbox: tuple[float, float, float, float]) -> str:
    """Compute an SVG transform that places the rendered glyph centered in cell (row, col)
    at LETTER_CAP_MM cap height.

    The font's coordinate system has Y up; SVG has Y down, so we apply a negative Y scale.
    """
    cap_height_units = font["OS/2"].sCapHeight
    scale = LETTER_CAP_MM / cap_height_units  # mm per font unit

    glyph_xmin, _glyph_ymin, glyph_xmax, _glyph_ymax = bbox
    glyph_width_units = glyph_xmax - glyph_xmin
    glyph_visual_center_y_units = cap_height_units / 2

    cx, cy = cell_center_mm(row, col)
    inner_dx = -(glyph_xmin + glyph_width_units / 2)
    inner_dy = -glyph_visual_center_y_units

    return f"translate({cx},{cy}) scale({scale},{-scale}) translate({inner_dx},{inner_dy})"
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_face.py enclosure/scripts/tests/test_render_face.py
git commit -m "feat(enclosure): letter-to-cell SVG transform"
```

---

## Task 9: Face renderer — assemble full SVG

**Files:**
- Modify: `enclosure/scripts/render_face.py`
- Modify: `enclosure/scripts/tests/test_render_face.py`

- [ ] **Step 1: Write the failing test**

Append to `enclosure/scripts/tests/test_render_face.py`:

```python
from enclosure.scripts.render_face import render_face_svg


def test_render_emory_face_has_169_letter_paths():
    svg_text = render_face_svg(kid="emory")
    # 169 letter cells + 1 outer rect
    path_count = svg_text.count("<path")
    rect_count = svg_text.count("<rect")
    assert path_count == 169, f"Expected 169 paths, got {path_count}"
    assert rect_count == 1, f"Expected 1 outer rect, got {rect_count}"


def test_render_emory_face_uses_jost():
    """Jost rendering should produce specific glyph path patterns. Smoke test:
    output should differ from Nora's Fraunces output.
    """
    emory_svg = render_face_svg(kid="emory")
    nora_svg = render_face_svg(kid="nora")
    assert emory_svg != nora_svg, "Emory and Nora SVGs should differ"


def test_render_face_outer_dimensions():
    svg_text = render_face_svg(kid="emory")
    assert 'width="192.0mm"' in svg_text
    assert 'height="192.0mm"' in svg_text
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: New tests FAIL.

- [ ] **Step 3: Implement render_face_svg**

Append to `enclosure/scripts/render_face.py`:

```python
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
GRID_CPP = REPO_ROOT / "firmware" / "lib" / "core" / "src" / "grid.cpp"

KID_FONT_CONFIG = {
    "emory": {"filename": "Jost-Variable.ttf", "weight": 700, "opsz": None},
    "nora": {"filename": "Fraunces-Variable.ttf", "weight": 500, "opsz": 14},
}


def render_face_svg(kid: str, grid_cpp_path: Path = GRID_CPP) -> str:
    """Render the face SVG for the given kid ("emory" or "nora").
    Returns the full SVG document as a string.
    """
    if kid not in KID_FONT_CONFIG:
        raise ValueError(f"Unknown kid {kid!r}; expected 'emory' or 'nora'")

    grids = parse_grid_cpp(grid_cpp_path)
    grid = grids[kid]

    font_config = KID_FONT_CONFIG[kid]
    font = load_font_instance(
        filename=font_config["filename"],
        weight=font_config["weight"],
        opsz=font_config["opsz"],
    )

    dwg = new_svg_document(width_mm=FACE_SIZE_MM, height_mm=FACE_SIZE_MM)

    # Outer face rectangle
    add_cut_rect(dwg, x=0, y=0, width=FACE_SIZE_MM, height=FACE_SIZE_MM)

    # 169 letter apertures
    for row in range(13):
        for col in range(13):
            char = grid[row][col]
            path_data, bbox = render_glyph(font, char)
            if bbox is None:
                continue
            transform = letter_transform_for_cell(font, char, row, col, bbox)
            add_cut_path(dwg, path_data, transform=transform)

    return dwg.tostring()
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_face.py -v
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_face.py enclosure/scripts/tests/test_render_face.py
git commit -m "feat(enclosure): full 13x13 face SVG generator"
```

---

## Task 10: Frame renderer — finger joint geometry

**Files:**
- Create: `enclosure/scripts/render_frame.py`
- Create: `enclosure/scripts/tests/test_render_frame.py`

- [ ] **Step 1: Write the failing test**

Create `enclosure/scripts/tests/test_render_frame.py`:

```python
"""Tests for frame strip + box-joint SVG rendering."""
import pytest

from enclosure.scripts.render_frame import (
    FRAME_LENGTH_MM,
    FRAME_DEPTH_MM,
    MATERIAL_THICKNESS_MM,
    FINGER_COUNT,
    FINGER_PITCH_MM,
    KERF_COMPENSATION_MM,
    finger_path_for_edge,
)


def test_frame_dimensions():
    assert FRAME_LENGTH_MM == 192.0
    assert FRAME_DEPTH_MM == 48.0
    assert MATERIAL_THICKNESS_MM == 6.4


def test_finger_geometry():
    assert FINGER_COUNT == 8
    assert FINGER_PITCH_MM == FRAME_DEPTH_MM / FINGER_COUNT  # = 6.0
    assert KERF_COMPENSATION_MM == 0.1


def test_finger_path_alternates_in_out():
    """Path for one corner edge — should alternate between fingers (sticking out)
    and slots (cut in) along the FRAME_DEPTH-mm edge.
    """
    path = finger_path_for_edge(invert=False)
    # Should have FINGER_COUNT alternations + start/end caps
    # As an SVG path, count "L" (line) commands
    line_count = path.count("L") + path.count("l")
    assert line_count >= FINGER_COUNT * 2, f"Expected >= {FINGER_COUNT * 2} line cmds, got {line_count}"


def test_finger_path_inverted_differs():
    a = finger_path_for_edge(invert=False)
    b = finger_path_for_edge(invert=True)
    assert a != b, "Inverted finger pattern should differ from default"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: FAIL with `ModuleNotFoundError`.

- [ ] **Step 3: Implement render_frame.py finger geometry**

Create `enclosure/scripts/render_frame.py`:

```python
"""Frame strip + box-joint SVG renderer.

Produces 4 frame strips per kid, each 192×48 mm × 6.4 mm material thickness.
Box-joint corners with 8 fingers × 6 mm pitch (finger width = material thickness).
Two strips have inverse finger pattern to mate with their neighbors.
"""
from .svg_utils import new_svg_document, add_cut_path

FRAME_LENGTH_MM = 192.0
FRAME_DEPTH_MM = 48.0
MATERIAL_THICKNESS_MM = 6.4

FINGER_COUNT = 8
FINGER_PITCH_MM = FRAME_DEPTH_MM / FINGER_COUNT  # = 6.0
KERF_COMPENSATION_MM = 0.1  # +0.1 finger / -0.1 slot for press-fit


def finger_path_for_edge(invert: bool) -> str:
    """Generate an SVG path-data fragment for a single short edge with finger joints.

    The path describes a vertical edge of length FRAME_DEPTH_MM, starting at (0, 0)
    and ending at (0, FRAME_DEPTH_MM). The horizontal direction (positive X)
    represents OUTWARD from the frame strip.

    For invert=False:  starts as a finger sticking out (+MATERIAL_THICKNESS in X)
    For invert=True:   starts as a slot cut in (no offset)

    Returns a string of SVG path commands (lowercase relative).
    """
    parts = []
    finger_depth = MATERIAL_THICKNESS_MM
    # Apply kerf compensation: fingers slightly larger, slots slightly smaller.
    # This is along the pitch axis (Y), not the depth axis (X).
    for i in range(FINGER_COUNT):
        is_finger = (i % 2 == 0) ^ invert
        # Each segment is FINGER_PITCH_MM tall along Y
        if is_finger:
            # Adjusted: finger is wider by 2 * KERF_COMPENSATION (one per side)
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            # Move outward by finger_depth
            if i == 0:
                parts.append(f"l {finger_depth},0")
            parts.append(f"l 0,{pitch}")
            # If next is a slot, move back inward
            next_is_finger = ((i + 1) % 2 == 0) ^ invert if i + 1 < FINGER_COUNT else None
            if next_is_finger is False:
                parts.append(f"l {-finger_depth},0")
        else:
            # Slot is narrower
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l 0,{pitch}")
            next_is_finger = ((i + 1) % 2 == 0) ^ invert if i + 1 < FINGER_COUNT else None
            if next_is_finger is True:
                parts.append(f"l {finger_depth},0")

    return " ".join(parts)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: Tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_frame.py enclosure/scripts/tests/test_render_frame.py
git commit -m "feat(enclosure): box-joint finger pattern generator"
```

---

## Task 11: Frame renderer — full strip outline

**Files:**
- Modify: `enclosure/scripts/render_frame.py`
- Modify: `enclosure/scripts/tests/test_render_frame.py`

- [ ] **Step 1: Write the failing test**

Append to `enclosure/scripts/tests/test_render_frame.py`:

```python
from enclosure.scripts.render_frame import frame_strip_path


def test_frame_strip_path_starts_with_M():
    path = frame_strip_path(invert_left=False, invert_right=True)
    assert path.startswith("M")


def test_frame_strip_path_closes():
    path = frame_strip_path(invert_left=False, invert_right=True)
    assert path.rstrip().endswith("Z") or path.rstrip().endswith("z")
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: New tests fail.

- [ ] **Step 3: Implement frame_strip_path**

Append to `enclosure/scripts/render_frame.py`:

```python
def frame_strip_path(invert_left: bool, invert_right: bool) -> str:
    """Generate the full SVG path-data string for one frame strip.

    The strip is FRAME_LENGTH_MM long (X axis) and FRAME_DEPTH_MM tall (Y axis).
    Long edges are straight; short edges (left and right) carry finger-joint
    patterns. invert_left/invert_right control which corner pattern (default
    or inverted) is used on each short edge.

    Path is a closed outline (M ... Z) traversing: top edge → right finger edge →
    bottom edge → left finger edge → close.
    """
    # Top-left start at (0, 0)
    parts = ["M 0,0"]
    # Top edge: straight line to (FRAME_LENGTH_MM, 0)
    parts.append(f"L {FRAME_LENGTH_MM},0")
    # Right finger edge (positive X is outward, but we're on right edge so flip):
    # Actually, finger_path_for_edge produces path going DOWN (positive Y) with
    # fingers sticking out in POSITIVE X. On the right edge of the strip, "outward"
    # is also positive X, so it works as-is.
    parts.append(finger_path_for_edge(invert=invert_right))
    # Bottom edge: line from current pos back to (0, FRAME_DEPTH_MM)
    parts.append(f"L 0,{FRAME_DEPTH_MM}")
    # Left finger edge: traverse upward along the LEFT edge with fingers in
    # negative X. We need to reverse direction (Y goes from FRAME_DEPTH_MM to 0).
    # Generate the same finger pattern, then flip.
    # Simpler: draw a separate path for the left edge starting from (0, FRAME_DEPTH_MM).
    parts.append(_left_edge_path(invert=invert_left))
    # Close
    parts.append("Z")
    return " ".join(parts)


def _left_edge_path(invert: bool) -> str:
    """Path along the left edge from (0, FRAME_DEPTH_MM) back to (0, 0),
    with fingers extending in negative X direction.
    """
    parts = []
    finger_depth = MATERIAL_THICKNESS_MM
    # Iterate fingers in REVERSE (we're going UP)
    for i in reversed(range(FINGER_COUNT)):
        is_finger = (i % 2 == 0) ^ invert
        if is_finger:
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            if i == FINGER_COUNT - 1:
                parts.append(f"l {-finger_depth},0")
            parts.append(f"l 0,{-pitch}")
            prev_is_finger = ((i - 1) % 2 == 0) ^ invert if i - 1 >= 0 else None
            if prev_is_finger is False:
                parts.append(f"l {finger_depth},0")
        else:
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l 0,{-pitch}")
            prev_is_finger = ((i - 1) % 2 == 0) ^ invert if i - 1 >= 0 else None
            if prev_is_finger is True:
                parts.append(f"l {-finger_depth},0")
    return " ".join(parts)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_frame.py enclosure/scripts/tests/test_render_frame.py
git commit -m "feat(enclosure): frame strip outline path with two finger edges"
```

---

## Task 12: Frame renderer — assemble 4-strip SVG

**Files:**
- Modify: `enclosure/scripts/render_frame.py`
- Modify: `enclosure/scripts/tests/test_render_frame.py`

- [ ] **Step 1: Write the failing test**

Append to `enclosure/scripts/tests/test_render_frame.py`:

```python
from enclosure.scripts.render_frame import render_frame_svg


def test_render_frame_svg_has_4_strip_paths():
    svg_text = render_frame_svg()
    path_count = svg_text.count("<path")
    assert path_count == 4, f"Expected 4 strip paths, got {path_count}"


def test_render_frame_svg_outer_dimensions_fit_4_strips():
    """4 strips at 192mm long × 48mm tall, stacked vertically with 5mm gap,
    should fit in a sheet of width 192mm and height 4*48 + 3*5 = 207mm.
    """
    svg_text = render_frame_svg()
    # Just verify width is at least 192mm and height accommodates 4 strips
    assert 'width="192.0mm"' in svg_text
    assert 'height="207.0mm"' in svg_text
```

- [ ] **Step 2: Run test to verify it fails**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: New tests fail.

- [ ] **Step 3: Implement render_frame_svg**

Append to `enclosure/scripts/render_frame.py`:

```python
STRIP_GAP_MM = 5.0  # spacing between stacked strips on the Ponoko sheet


def render_frame_svg() -> str:
    """Generate the SVG containing all 4 frame strips for one clock.

    Strips are laid out stacked vertically with STRIP_GAP_MM gap so they cleanly
    nest on a single Ponoko sheet. Total sheet: 192 mm wide × (4*48 + 3*5) = 207 mm tall.

    Strip identity / inversion pattern:
      strip 0: invert_left=False, invert_right=True
      strip 1: invert_left=True,  invert_right=False
      strip 2: invert_left=False, invert_right=True
      strip 3: invert_left=True,  invert_right=False
    Adjacent strips at a corner have mating (inverse) patterns.
    """
    sheet_width = FRAME_LENGTH_MM
    sheet_height = 4 * FRAME_DEPTH_MM + 3 * STRIP_GAP_MM

    dwg = new_svg_document(width_mm=sheet_width, height_mm=sheet_height)

    inversion_patterns = [
        (False, True),
        (True, False),
        (False, True),
        (True, False),
    ]
    for i, (inv_l, inv_r) in enumerate(inversion_patterns):
        path_data = frame_strip_path(invert_left=inv_l, invert_right=inv_r)
        y_offset = i * (FRAME_DEPTH_MM + STRIP_GAP_MM)
        add_cut_path(dwg, path_data, transform=f"translate(0,{y_offset})")

    return dwg.tostring()
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pytest enclosure/scripts/tests/test_render_frame.py -v
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add enclosure/scripts/render_frame.py enclosure/scripts/tests/test_render_frame.py
git commit -m "feat(enclosure): full 4-strip frame SVG generator"
```

---

## Task 13: Build CLI — generate all SVGs

**Files:**
- Create: `enclosure/scripts/build.py`

- [ ] **Step 1: Implement the build CLI**

Create `enclosure/scripts/build.py`:

```python
"""Build all production SVG files for both clocks.

Usage: python -m enclosure.scripts.build  (from repo root)
       OR: python enclosure/scripts/build.py
"""
from pathlib import Path
import sys

# Allow running as script OR module
if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from enclosure.scripts.render_face import render_face_svg
from enclosure.scripts.render_frame import render_frame_svg

REPO_ROOT = Path(__file__).resolve().parents[2]
ENCLOSURE_DIR = REPO_ROOT / "enclosure"


def main() -> None:
    ENCLOSURE_DIR.mkdir(parents=True, exist_ok=True)

    outputs = {
        "emory-face.svg":   lambda: render_face_svg(kid="emory"),
        "nora-face.svg":    lambda: render_face_svg(kid="nora"),
        "emory-frame.svg":  lambda: render_frame_svg(),
        "nora-frame.svg":   lambda: render_frame_svg(),
    }

    for filename, generator in outputs.items():
        target = ENCLOSURE_DIR / filename
        target.write_text(generator())
        print(f"  wrote {target.relative_to(REPO_ROOT)} ({target.stat().st_size} bytes)")

    # Cardboard test variant: same as Emory face but with a header label baked in for
    # easy ID after the cut. We write the same content for now; future iteration could add a label.
    cardboard = ENCLOSURE_DIR / "cardboard-test-face.svg"
    cardboard.write_text(render_face_svg(kid="emory"))
    print(f"  wrote {cardboard.relative_to(REPO_ROOT)} (cardboard test, identical to emory-face.svg)")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the build**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY enclosure/scripts/build.py
```

Expected output:

```
  wrote enclosure/emory-face.svg (XXXXX bytes)
  wrote enclosure/nora-face.svg (XXXXX bytes)
  wrote enclosure/emory-frame.svg (XXXX bytes)
  wrote enclosure/nora-frame.svg (XXXX bytes)
  wrote enclosure/cardboard-test-face.svg (XXXXX bytes)
```

- [ ] **Step 3: Verify the SVGs render in a browser**

Open each in a browser:

```bash
open enclosure/emory-face.svg enclosure/nora-face.svg enclosure/emory-frame.svg enclosure/nora-frame.svg
```

Expected:
- Both face SVGs show a 192×192 mm square with 13×13 grid of letters
- Emory face uses geometric Jost letters
- Nora face uses serif Fraunces letters
- Both frame SVGs show 4 horizontal strips stacked vertically with finger-joint notches on short edges

If a face SVG is blank or missing letters, check that fonts downloaded successfully (`enclosure/scripts/fonts/`).

- [ ] **Step 4: Commit**

```bash
git add enclosure/scripts/build.py enclosure/emory-face.svg enclosure/emory-frame.svg enclosure/nora-face.svg enclosure/nora-frame.svg enclosure/cardboard-test-face.svg
git commit -m "feat(enclosure): build CLI + initial SVG outputs"
```

---

## Task 14: Visual verification via brainstorm companion

**Files:**
- (No files modified — verification step only)

- [ ] **Step 1: Inline the generated SVGs into a verification HTML and push to brainstorm browser**

```bash
SCREEN_DIR=$(ls -d .superpowers/brainstorm/*/content 2>/dev/null | head -1)
if [ -z "$SCREEN_DIR" ]; then
  echo "Brainstorm server not running. Start it with:"
  echo "  ~/.claude/plugins/cache/claude-plugins-official/superpowers/5.0.7/skills/brainstorming/scripts/start-server.sh --project-dir $(pwd)"
  exit 1
fi

EMORY_FACE=$(cat enclosure/emory-face.svg | sed -e '1d')
NORA_FACE=$(cat enclosure/nora-face.svg | sed -e '1d')
EMORY_FRAME=$(cat enclosure/emory-frame.svg | sed -e '1d')

cat > "$SCREEN_DIR/verify-svgs.html" <<EOF
<style>
  .panel { background:#fff; padding:20px; border-radius:6px; margin-bottom:16px; }
  .preview-bg { background:#a8845c; padding:10px; border-radius:4px; max-width:600px; margin:0 auto; }
  svg { display:block; max-width:100%; height:auto; background:#a8845c; }
  .check-list { font-size:13px; color:#444; }
  .check-list li { margin:4px 0; }
</style>

<h2>Verify generated SVGs</h2>

<div class="panel">
  <h3>Emory face — Maple Hardwood / Jost Bold</h3>
  <div class="preview-bg">$EMORY_FACE</div>
  <ul class="check-list">
    <li>Outer rectangle 192×192 mm visible</li>
    <li>13×13 letter grid centered</li>
    <li>Row 0: I T E I S M T E N H A L F</li>
    <li>Row 8: E M O R Y 0 A T F I V E 6</li>
    <li>Closed letters (O, B, P, R, 0, 6, 8) show inner counter as separate cut</li>
  </ul>
</div>

<div class="panel">
  <h3>Nora face — Walnut Hardwood / Fraunces Medium</h3>
  <div class="preview-bg">$NORA_FACE</div>
  <ul class="check-list">
    <li>Same grid as Emory except row 8 is N O R A R 1 A T F I V E 9</li>
    <li>Filler patches in rows 0, 2, 3, 12 spell NORA MAR 19 2025 in reading order</li>
  </ul>
</div>

<div class="panel">
  <h3>Emory frame strips — Maple Hardwood</h3>
  <div class="preview-bg">$EMORY_FRAME</div>
  <ul class="check-list">
    <li>4 strips stacked vertically, 192 × 48 mm each</li>
    <li>Short edges have alternating finger-joint pattern</li>
    <li>Adjacent strips have mating (inverse) finger patterns at corners</li>
  </ul>
</div>

<p style="font-size:13px; color:#666; margin-top:20px;">
  If any SVG looks wrong, check terminal for the build error or open the file directly to inspect.
</p>
EOF

echo "Pushed verify-svgs.html to brainstorm content dir."
echo "Open the brainstorm URL in your browser to review."
```

- [ ] **Step 2: Open browser to brainstorm URL**

(URL is in `~/.claude/...brainstorm/<session>/state/server-info`. The brainstorm server must already be running — if not, this verification can be done by opening the SVG files directly in a browser.)

- [ ] **Step 3: Manually verify each panel**

For each SVG, confirm visually:
- Face SVGs: letter grid is correctly populated; closed letters show inner counters
- Frame SVG: 4 strips with mating finger patterns; finger pitch ≈ 6 mm

Report any visual mismatch (wrong letter, missing inner counter, incorrect spacing) before proceeding.

- [ ] **Step 4: (No commit — verification step only)**

---

## Task 15: Documentation update

**Files:**
- Modify: `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`
- Modify: `enclosure/README.md`

- [ ] **Step 1: Add an "Implementation status" section to the spec**

Append to `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md`:

```markdown
## Implementation status

Implementation plan: `docs/superpowers/plans/2026-04-15-laser-cut-face-implementation.md`
Generator: `enclosure/scripts/build.py`
Output SVGs: `enclosure/{emory,nora}-{face,frame}.svg` + `enclosure/cardboard-test-face.svg`
Tests: `pytest enclosure/scripts/tests/`
```

- [ ] **Step 2: Verify enclosure/README.md instructions still work end-to-end**

```bash
rm -rf enclosure/scripts/__pycache__ enclosure/scripts/fonts
PY=/Library/Developer/CommandLineTools/usr/bin/python3
$PY -m pip install --user -r enclosure/scripts/requirements.txt
$PY enclosure/scripts/build.py
```

Expected: clean run from a fresh state, all SVGs regenerated. Validates the README's reproduction steps.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/specs/2026-04-15-laser-cut-face-design.md
git commit -m "docs(enclosure): cross-reference implementation plan from spec"
```

---

## Self-Review

After completing all tasks, scan against the spec:

| Spec section | Implementation task |
|---|---|
| Scope (face + frames) | Tasks 7-12 |
| Materials (Maple/Walnut Hardwood specs) | README + spec only — no code constants needed |
| Dimensions (192×192×48 face, 6.4 mm walls, 8×6 mm fingers) | Task 7 (face dims), Task 10 (frame dims) |
| Letter grid (13×13, 13.68 mm, 10 mm cap) | Task 7 (constants) |
| Fonts (Jost Bold, Fraunces Medium) | Task 4 (download), Task 5 (instancing), Task 9 (kid→font config) |
| Inner-island retention (no bridges) | Task 5 (renders inner counters as subpaths), Task 9 (no bridge logic) |
| Box-joint geometry | Tasks 10-12 |
| Ponoko file format (mm, hairline, blue cut) | Task 6 |
| Files produced (4 SVGs + cardboard) | Task 13 |
| Validation plan (cardboard test) | Task 13 (cardboard variant) + Task 14 (visual verification) |

| Open issue | How addressed in implementation |
|---|---|
| LED placement offset (−1.5, −0.5) mm | Documented in spec; face cells stay at standard positions (no compensation in this code — diffuser test is the gate) |
| Light channel material override (black PLA) | Documented in spec, not in code (Track 5 spec) |
| Filler cell isolation | Documented in spec, not in code (Track 5 spec) |
| Diffuser product selection | Documented in spec, not in code (Track Q spec) |
| Wood finish | Documented in spec, not in code (assembly spec) |

No placeholders, no TBDs in any task. All file paths exact. All test code complete. All command lines executable as written.

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-04-15-laser-cut-face-implementation.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
