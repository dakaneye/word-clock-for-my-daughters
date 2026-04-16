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
