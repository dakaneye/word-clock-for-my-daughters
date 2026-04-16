"""Ponoko SVG conventions for laser-cut files.

Ponoko requirements (from ponoko.com/laser-cutting documentation):
  - Stroke color blue #0000FF = cut path
  - Stroke width: 0.01 mm hairline
  - No fills
  - Units: mm
"""
from typing import Optional

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
                 transform: Optional[str] = None) -> None:
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
