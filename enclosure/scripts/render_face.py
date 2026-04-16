"""Face SVG renderer.

Generates 192×192 mm face SVGs with 13×13 letter grid centered, 7.1 mm wood border.
Cells aligned to PCB LED grid (PCB is 177.8×177.8 mm with LEDs at 13.68 mm pitch).
"""
from pathlib import Path
from typing import Tuple

from fontTools.ttLib import TTFont

from .fonts import load_font_instance, render_glyph
from .parse_grid import parse_grid_cpp
from .svg_utils import new_svg_document, add_cut_rect, add_cut_path

FACE_SIZE_MM = 192.0
GRID_SIZE_MM = 177.8
BORDER_MM = 7.1
CELL_MM = GRID_SIZE_MM / 13  # ≈ 13.6769
LETTER_CAP_MM = 10.0


def cell_center_mm(row: int, col: int) -> Tuple[float, float]:
    """Return (x, y) in mm for the center of the cell at (row, col),
    measured from the top-left corner of the face.
    """
    x = BORDER_MM + (col + 0.5) * CELL_MM
    y = BORDER_MM + (row + 0.5) * CELL_MM
    return x, y


def letter_transform_for_cell(font: TTFont, char: str, row: int, col: int,
                              bbox: Tuple[float, float, float, float]) -> str:
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
