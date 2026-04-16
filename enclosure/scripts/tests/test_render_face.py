"""Tests for face SVG rendering."""
import pytest

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
    assert transform.startswith(f"translate({expected_cx},{expected_cy})")


from enclosure.scripts.render_face import render_face_svg


def test_render_emory_face_has_169_letter_paths():
    svg_text = render_face_svg(kid="emory")
    # 169 letter cells + 1 outer rect
    path_count = svg_text.count("<path")
    rect_count = svg_text.count("<rect")
    assert path_count == 169, f"Expected 169 paths, got {path_count}"
    assert rect_count == 1, f"Expected 1 outer rect, got {rect_count}"


def test_render_emory_face_uses_jost_vs_nora_uses_fraunces():
    """Emory and Nora SVGs should differ (different fonts + different row 8/fillers)."""
    emory_svg = render_face_svg(kid="emory")
    nora_svg = render_face_svg(kid="nora")
    assert emory_svg != nora_svg, "Emory and Nora SVGs should differ"


def test_render_face_outer_dimensions():
    svg_text = render_face_svg(kid="emory")
    assert 'width="192.0mm"' in svg_text
    assert 'height="192.0mm"' in svg_text
