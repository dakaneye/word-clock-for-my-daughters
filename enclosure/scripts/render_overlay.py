"""Print-at-1:1 alignment overlay for PCB ↔ face ↔ letter-grid verification.

Purpose: produce a PDF that, when printed on US Letter paper at 100 % scale,
can be laid against the laser-cut face or the finished PCB to confirm the
LEDs sit where each word is supposed to be.

Drawn (all coordinates in face-frame millimetres, origin = top-left of face):

  • Face outline — 192×192 mm dashed rectangle.
  • 13×13 grid of cell boundaries, 13.68 mm pitch, 7.1 mm inset from face edge.
  • Letter labels (from firmware/lib/core/src/grid.cpp) centered in each cell.
  • PCB outline — 177.8×177.8 mm solid rectangle at (7.1, 7.1).
  • 4 × M3 mounting holes as 3.2 mm circles.
  • Ideal cell centers — tiny blue dots (design-intent word centers).
  • Actual LED centers — red crosshairs (from hardware/word-clock-all-pos.csv).
  • Registration crosses at all 4 face corners.
  • 50 mm scale bar at the bottom.
  • Title + legend.

Red crosshairs and blue dots should be offset by (−1.5, −0.5) mm per the
documented PCB placement anomaly. That's the whole point of printing this:
seeing the offset at 1:1 makes the impact obvious before committing to the
light channel CAD.

Usage:
    python -m enclosure.scripts.render_overlay          # writes PDFs to /tmp/word-clock-overlay/
    python -m enclosure.scripts.render_overlay --svg    # also keep the intermediate SVG
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable

import cairosvg

from .parse_grid import parse_grid_cpp

REPO_ROOT = Path(__file__).resolve().parents[2]
GRID_CPP = REPO_ROOT / "firmware" / "lib" / "core" / "src" / "grid.cpp"
KICAD_PCB = REPO_ROOT / "hardware" / "word-clock.kicad_pcb"
DEFAULT_OUT_DIR = Path("/tmp/word-clock-overlay")

# Geometry (all mm). Must stay in sync with render_face.py + the PCB.
FACE_SIZE = 192.0
GRID_SIZE = 177.8
BORDER = 7.1                # face_edge → grid_edge (also = pcb offset in face frame)
CELL = GRID_SIZE / 13       # ≈ 13.6769

# US Letter in portrait (mm).
PAGE_W = 215.9
PAGE_H = 279.4

MOUNTING_HOLE_DIA = 3.2


def _read_kicad_footprints(
    pcb_path: Path, footprint_name: str, want_ref: bool = True,
) -> list[tuple[str | None, float, float]]:
    """Return [(ref_or_None, x, y), ...] for every footprint whose library name
    matches ``footprint_name`` in the given .kicad_pcb file.

    Reads the authoritative KiCad source directly — no dependency on the CPL
    CSV, which can go stale if gerbers aren't re-exported after edits. X and Y
    are in the PCB's own frame (origin at Edge.Cuts top-left, positive Y =
    toward bottom of board — verified against the CPL's negated Y convention).
    """
    text = pcb_path.read_text()
    # Pattern: lib name, skip to (at X Y), then (optionally) grab the Reference
    # property a bit further down. footprints are big multi-line blocks; use a
    # permissive multiline match bounded by the next '(footprint' or end of file.
    out: list[tuple[str | None, float, float]] = []
    fp_re = re.compile(
        r'\(footprint\s+"' + re.escape(footprint_name) + r'".*?'
        r'\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+[-\d.]+)?\s*\)'
        r'(.*?)(?=\(footprint\s|\Z)',
        re.DOTALL,
    )
    ref_re = re.compile(r'property\s+"Reference"\s+"([^"]+)"')
    for m in fp_re.finditer(text):
        x, y, body = float(m.group(1)), float(m.group(2)), m.group(3)
        ref = None
        if want_ref:
            rm = ref_re.search(body)
            if rm:
                ref = rm.group(1)
        out.append((ref, x, y))
    return out


def _led_face_positions(pcb_path: Path) -> list[tuple[str, float, float]]:
    """Return [(ref, x_face_mm, y_face_mm), ...] for every WS2812B LED.

    PCB frame → face frame: add the 7.1 mm border on both axes (because the
    PCB sits 7.1 mm inset from the face edge on all sides).
    """
    leds = _read_kicad_footprints(
        pcb_path, "LED_SMD:LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm", want_ref=True,
    )
    out: list[tuple[str, float, float]] = []
    for ref, x, y in leds:
        assert ref is not None, "LED without Reference property"
        out.append((ref, x + BORDER, y + BORDER))
    return out


def _mounting_hole_face_positions(pcb_path: Path) -> list[tuple[float, float]]:
    """Return mounting-hole center positions in face-frame millimetres."""
    holes = _read_kicad_footprints(
        pcb_path, "MountingHole:MountingHole_3.2mm_M3", want_ref=False,
    )
    return [(x + BORDER, y + BORDER) for _ref, x, y in holes]


def _cell_center(row: int, col: int) -> tuple[float, float]:
    return (BORDER + (col + 0.5) * CELL, BORDER + (row + 0.5) * CELL)


def _svg_header(title: str) -> str:
    # Place 192×192 face content box centered horizontally, 20 mm from the top.
    face_x0 = (PAGE_W - FACE_SIZE) / 2
    face_y0 = 24.0
    # Stash a g-transform at the file level so geometry can be drawn in face
    # coords and we never have to manually shift per-element.
    return (
        f'<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{PAGE_W}mm" height="{PAGE_H}mm" '
        f'viewBox="0 0 {PAGE_W} {PAGE_H}">\n'
        f'<rect x="0" y="0" width="{PAGE_W}" height="{PAGE_H}" fill="white"/>\n'
        f'<text x="{PAGE_W / 2}" y="12" font-family="Helvetica,Arial,sans-serif" '
        f'font-size="5" text-anchor="middle" font-weight="bold">{title}</text>\n'
        f'<text x="{PAGE_W / 2}" y="19" font-family="Helvetica,Arial,sans-serif" '
        f'font-size="3" text-anchor="middle" fill="#666">'
        f'Print at 100% scale. Verify with the 50 mm scale bar below.</text>\n'
        f'<g transform="translate({face_x0},{face_y0})">\n'
    )


def _svg_footer(content_offset_y: float) -> str:
    # Close the face-frame group, then draw legend + scale bar in page coords.
    legend_y = content_offset_y + FACE_SIZE + 14
    scale_y = legend_y
    scale_x0 = 20.0
    return (
        '</g>\n'
        # Scale bar (50 mm, portrait-bottom-left)
        f'<g transform="translate({scale_x0},{scale_y})">\n'
        '  <line x1="0" y1="0" x2="50" y2="0" stroke="black" stroke-width="0.4"/>\n'
        '  <line x1="0" y1="-2" x2="0" y2="2" stroke="black" stroke-width="0.4"/>\n'
        '  <line x1="10" y1="-1.5" x2="10" y2="1.5" stroke="black" stroke-width="0.3"/>\n'
        '  <line x1="20" y1="-1.5" x2="20" y2="1.5" stroke="black" stroke-width="0.3"/>\n'
        '  <line x1="30" y1="-1.5" x2="30" y2="1.5" stroke="black" stroke-width="0.3"/>\n'
        '  <line x1="40" y1="-1.5" x2="40" y2="1.5" stroke="black" stroke-width="0.3"/>\n'
        '  <line x1="50" y1="-2" x2="50" y2="2" stroke="black" stroke-width="0.4"/>\n'
        '  <text x="25" y="8" font-family="Helvetica,Arial,sans-serif" font-size="3" '
        'text-anchor="middle">50 mm — measure to verify print scale</text>\n'
        '</g>\n'
        # Legend
        f'<g transform="translate({PAGE_W - 80},{legend_y - 6})" '
        'font-family="Helvetica,Arial,sans-serif" font-size="2.8">\n'
        '  <rect x="0" y="0" width="76" height="22" fill="none" stroke="#666" stroke-width="0.2"/>\n'
        '  <line x1="2" y1="4" x2="6" y2="4" stroke="black" stroke-width="0.5"/>\n'
        '  <text x="8" y="4.8">PCB outline (177.8×177.8 mm)</text>\n'
        '  <circle cx="4" cy="9" r="0.7" fill="none" stroke="black" stroke-width="0.3"/>\n'
        '  <text x="8" y="9.8">M3 mounting hole (3.2 mm)</text>\n'
        '  <circle cx="4" cy="14" r="0.7" fill="#2b6" stroke="none"/>\n'
        '  <text x="8" y="14.8">Ideal word center (LED + 1.5, +0.5 mm)</text>\n'
        '  <g transform="translate(4,19)">\n'
        '    <line x1="-1.5" y1="0" x2="1.5" y2="0" stroke="#c33" stroke-width="0.4"/>\n'
        '    <line x1="0" y1="-1.5" x2="0" y2="1.5" stroke="#c33" stroke-width="0.4"/>\n'
        '  </g>\n'
        '  <text x="8" y="19.8">Actual LED center (read from kicad_pcb)</text>\n'
        '</g>\n'
        '</svg>\n'
    )


def _face_outline_svg() -> str:
    return (
        f'<rect x="0" y="0" width="{FACE_SIZE}" height="{FACE_SIZE}" '
        'fill="none" stroke="#aaa" stroke-width="0.2" stroke-dasharray="2,2"/>\n'
    )


def _pcb_outline_svg() -> str:
    return (
        f'<rect x="{BORDER}" y="{BORDER}" width="{GRID_SIZE}" height="{GRID_SIZE}" '
        'fill="none" stroke="black" stroke-width="0.35"/>\n'
    )


def _grid_lines_svg() -> str:
    out = []
    for i in range(14):
        v = BORDER + i * CELL
        out.append(
            f'<line x1="{v}" y1="{BORDER}" x2="{v}" y2="{BORDER + GRID_SIZE}" '
            'stroke="#ccc" stroke-width="0.1"/>'
        )
        out.append(
            f'<line x1="{BORDER}" y1="{v}" x2="{BORDER + GRID_SIZE}" y2="{v}" '
            'stroke="#ccc" stroke-width="0.1"/>'
        )
    return "\n".join(out) + "\n"


def _letter_labels_svg(grid: list[list[str]]) -> str:
    out = []
    for r, row in enumerate(grid):
        for c, ch in enumerate(row):
            cx, cy = _cell_center(r, c)
            # Offset y for visual centering (font baseline).
            out.append(
                f'<text x="{cx}" y="{cy + 3}" font-family="Helvetica,Arial,sans-serif" '
                f'font-size="9" text-anchor="middle" fill="#888" '
                f'font-weight="500">{ch}</text>'
            )
    return "\n".join(out) + "\n"


def _ideal_word_center_dots_svg(leds: Iterable[tuple[str, float, float]]) -> str:
    # The documented PCB anomaly is that each LED sits (−1.5, −0.5) mm from the
    # design-intent word center. Reverse the offset to draw a blue dot at the
    # ideal position for every LED — each blue dot gets a paired red crosshair
    # 1.5 mm to the right / 0.5 mm down, so the misalignment is visible as a
    # paired marker wherever an LED lives.
    out = []
    for _ref, led_x, led_y in leds:
        ideal_x = led_x + 1.5
        ideal_y = led_y + 0.5
        out.append(f'<circle cx="{ideal_x}" cy="{ideal_y}" r="0.7" fill="#2b6"/>')
    return "\n".join(out) + "\n"


def _led_crosshairs_svg(leds: Iterable[tuple[str, float, float]]) -> str:
    out = []
    for ref, x, y in leds:
        out.append(
            f'<g transform="translate({x},{y})">'
            '<line x1="-1.5" y1="0" x2="1.5" y2="0" stroke="#c33" stroke-width="0.35"/>'
            '<line x1="0" y1="-1.5" x2="0" y2="1.5" stroke="#c33" stroke-width="0.35"/>'
            '</g>'
        )
    return "\n".join(out) + "\n"


def _mounting_holes_svg(holes: Iterable[tuple[float, float]]) -> str:
    r = MOUNTING_HOLE_DIA / 2
    out = []
    for fx, fy in holes:
        out.append(
            f'<circle cx="{fx}" cy="{fy}" r="{r}" fill="none" '
            'stroke="black" stroke-width="0.3"/>'
        )
    return "\n".join(out) + "\n"


def _registration_crosses_svg() -> str:
    # Outside the face outline on each side so they don't overprint the letters.
    # Each cross is 8 mm arms, 5 mm out from the face edge.
    arm = 6.0
    offset = 5.0
    positions = [
        (-offset, -offset),
        (FACE_SIZE + offset, -offset),
        (-offset, FACE_SIZE + offset),
        (FACE_SIZE + offset, FACE_SIZE + offset),
    ]
    out = []
    for cx, cy in positions:
        out.append(
            f'<g transform="translate({cx},{cy})">'
            f'<line x1="{-arm}" y1="0" x2="{arm}" y2="0" stroke="black" stroke-width="0.3"/>'
            f'<line x1="0" y1="{-arm}" x2="0" y2="{arm}" stroke="black" stroke-width="0.3"/>'
            f'<circle cx="0" cy="0" r="1.2" fill="none" stroke="black" stroke-width="0.3"/>'
            '</g>'
        )
    return "\n".join(out) + "\n"


def _dimension_label_svg() -> str:
    # Small annotation of the known 192 mm width so the reader can verify by caliper.
    return (
        f'<text x="{FACE_SIZE / 2}" y="{FACE_SIZE + 4}" '
        f'font-family="Helvetica,Arial,sans-serif" font-size="2.5" '
        f'text-anchor="middle" fill="#666">'
        f'Face outline 192 mm × 192 mm · Grid 177.8 mm · Cell 13.68 mm</text>\n'
    )


def build_overlay_svg(kid: str) -> str:
    if kid not in ("emory", "nora"):
        raise ValueError(f"unknown kid: {kid!r}")
    grid = parse_grid_cpp(GRID_CPP)[kid]
    leds = _led_face_positions(KICAD_PCB)
    mounting_holes = _mounting_hole_face_positions(KICAD_PCB)

    title = f"Word Clock — {kid.capitalize()} — Overlay"
    face_offset_y = 24.0

    parts = [
        _svg_header(title),
        # Background / frame
        _face_outline_svg(),
        _grid_lines_svg(),
        _pcb_outline_svg(),
        _mounting_holes_svg(mounting_holes),
        # Letters (muted) so LED marks remain legible on top
        _letter_labels_svg(grid),
        # Design-intent + actual — paired so the (−1.5, −0.5) mm offset is
        # visible at every LED.
        _ideal_word_center_dots_svg(leds),
        _led_crosshairs_svg(leds),
        # Page furniture
        _registration_crosses_svg(),
        _dimension_label_svg(),
        _svg_footer(face_offset_y),
    ]
    return "".join(parts)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                    help="Directory to write PDFs (and optional SVGs) into.")
    ap.add_argument("--svg", action="store_true",
                    help="Also write the intermediate SVG alongside the PDF.")
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    for kid in ("emory", "nora"):
        svg_text = build_overlay_svg(kid)
        if args.svg:
            svg_path = args.out_dir / f"{kid}-overlay.svg"
            svg_path.write_text(svg_text)
            print(f"  wrote {svg_path}")
        pdf_path = args.out_dir / f"{kid}-overlay.pdf"
        cairosvg.svg2pdf(bytestring=svg_text.encode("utf-8"), write_to=str(pdf_path))
        print(f"  wrote {pdf_path}")


if __name__ == "__main__":
    main()
