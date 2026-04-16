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
