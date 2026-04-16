"""Tests for the PCB ↔ face alignment overlay.

These tests anchor the claims the overlay makes to the actual PCB source so
the overlay cannot silently drift from hardware reality.
"""
import pytest

from enclosure.scripts.render_overlay import (
    BORDER,
    CELL,
    FACE_SIZE,
    GRID_SIZE,
    KICAD_PCB,
    _led_face_positions,
    _mounting_hole_face_positions,
)
from enclosure.scripts.render_face import (
    BORDER_MM,
    CELL_MM,
    FACE_SIZE_MM,
    GRID_SIZE_MM,
)


def test_overlay_geometry_matches_face_render():
    """Overlay and face renderer must share identical grid geometry, else the
    overlay's cell lines won't land on the actual letter positions.
    """
    assert FACE_SIZE == FACE_SIZE_MM
    assert GRID_SIZE == GRID_SIZE_MM
    assert BORDER == BORDER_MM
    assert CELL == pytest.approx(CELL_MM, abs=1e-9)


def test_all_35_leds_read_from_kicad_pcb():
    """Clock has 35 WS2812B LEDs. If the overlay reads fewer, the PCB file has
    drifted from the design and the overlay is hiding LEDs.
    """
    leds = _led_face_positions(KICAD_PCB)
    assert len(leds) == 35, f"expected 35 LEDs, got {len(leds)}"
    refs = {ref for ref, _, _ in leds}
    assert refs == {f"D{i}" for i in range(1, 36)}


def test_led_positions_within_grid_bounds():
    """Every LED must fall inside the 177.8 × 177.8 mm grid region when
    translated into face coordinates. An LED outside this box would indicate
    a PCB layout error (or the overlay's coordinate translation is wrong).
    """
    leds = _led_face_positions(KICAD_PCB)
    grid_x0, grid_y0 = BORDER, BORDER
    grid_x1, grid_y1 = BORDER + GRID_SIZE, BORDER + GRID_SIZE
    for ref, x, y in leds:
        assert grid_x0 <= x <= grid_x1, f"{ref} at x={x} is outside grid"
        assert grid_y0 <= y <= grid_y1, f"{ref} at y={y} is outside grid"


def test_led_offset_from_word_center_is_documented_anomaly():
    """Pick the simplest LED (IT, row 0 cols 0-1) and verify the known PCB
    placement offset matches the documented (-1.5, -0.5) mm systematic delta.
    This test will fail if someone moves an LED back to its ideal position
    without updating the overlay's offset narrative.
    """
    leds = dict((ref, (x, y)) for ref, x, y in _led_face_positions(KICAD_PCB))
    # IT spans cols 0-1 on row 0 → word center at col 1.0, row 0.5.
    word_center_x = BORDER + 1.0 * CELL
    word_center_y = BORDER + 0.5 * CELL
    led_x, led_y = leds["D1"]  # LED_IT
    assert led_x == pytest.approx(word_center_x - 1.5, abs=0.01)
    assert led_y == pytest.approx(word_center_y - 0.5, abs=0.01)


def test_four_mounting_holes_at_corners():
    """PCB should have 4 mounting holes, each in a distinct corner region of
    the 177.8 mm board.
    """
    holes = _mounting_hole_face_positions(KICAD_PCB)
    assert len(holes) == 4, f"expected 4 mounting holes, got {len(holes)}"
    # Translate back to PCB-local for the corner check, then assert each hole
    # is within 15 mm of exactly one board corner.
    pcb_corners = [(0, 0), (GRID_SIZE, 0), (0, GRID_SIZE), (GRID_SIZE, GRID_SIZE)]
    matched = set()
    for fx, fy in holes:
        px, py = fx - BORDER, fy - BORDER
        for i, (cx, cy) in enumerate(pcb_corners):
            if abs(px - cx) < 15 and abs(py - cy) < 15:
                matched.add(i)
                break
    assert matched == {0, 1, 2, 3}, f"holes did not cover all 4 corners: {matched}"
