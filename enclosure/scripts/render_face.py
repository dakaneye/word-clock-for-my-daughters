"""Face SVG renderer.

Generates 192×192 mm face SVGs with 13×13 letter grid centered, 7.1 mm wood border.
Cells aligned to PCB LED grid (PCB is 177.8×177.8 mm with LEDs at 13.68 mm pitch).
"""
from pathlib import Path
from typing import Tuple

from fontTools.ttLib import TTFont

from .fonts import load_font_instance, render_glyph
from .parse_grid import parse_grid_cpp
from .path_ops import union_glyph_path
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
    add_cut_rect(dwg, x=0, y=0, width=FACE_SIZE_MM, height=FACE_SIZE_MM)

    for row in range(13):
        for col in range(13):
            char = grid[row][col]
            path_data, bbox = render_glyph(font, char)
            if bbox is None:
                continue
            # Union overlapping sub-shapes (e.g., Jost E = stem + 3 bars) into
            # a single clean outline so the laser cuts the letter as one piece
            # rather than as separate fragments.
            unioned = union_glyph_path(path_data)
            transform = letter_transform_for_cell(font, char, row, col, bbox)
            add_cut_path(dwg, unioned, transform=transform)

    return dwg.tostring()
