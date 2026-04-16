"""Tests for the back-panel SVG renderer.

Checks the structural invariants: outer dimensions, fastener count and
placement, and button-hole alignment to the actual PCB switch positions.
"""
import re

import pytest

from enclosure.scripts.render_back_panel import (
    BORDER_MM,
    BUTTON_HOLE_DIA_MM,
    FRAME_THICKNESS_MM,
    PANEL_SIZE_MM,
    SCREW_CLEARANCE_MM,
    SCREW_CORNER_INSET_MM,
    _read_switch_positions,
    render_back_panel_svg,
)


def _count_circles(svg: str) -> int:
    return len(re.findall(r"<circle\b", svg))


def _count_rects(svg: str) -> int:
    return len(re.findall(r"<rect\b", svg))


def test_corner_screw_inset_sized_for_hex_spacer():
    """Corner inset must leave room for a 5 mm AF brass hex spacer (half-width
    2.5 mm) epoxied inside the frame corner, plus a small clearance so the
    screw doesn't foul the spacer's outer edge.
    """
    hex_half_af = 2.5
    clearance = 0.5
    assert SCREW_CORNER_INSET_MM >= hex_half_af + clearance


@pytest.mark.parametrize("kid", ["emory", "nora"])
def test_panel_has_expected_cutouts(kid):
    svg = render_back_panel_svg(kid)
    # 4 corner screws + 3 buttons + 2 USB-C mount screws = 9 cut circles,
    # plus the speaker vent grid (5×5 = 25) = 34 total.
    circles = _count_circles(svg)
    assert circles == 4 + 3 + 2 + 25, (
        f"{kid}: expected 34 circles, got {circles}"
    )
    # Outer panel + USB-C cutout + SD slot = 3 rects.
    rects = _count_rects(svg)
    assert rects == 3, f"{kid}: expected 3 rects, got {rects}"


@pytest.mark.parametrize("kid", ["emory", "nora"])
def test_panel_has_dedication_engraving(kid):
    svg = render_back_panel_svg(kid)
    # Engraved text is rendered as filled paths (fill="#000000"). Each
    # dedication has three lines of several characters — expect many paths.
    engrave_paths = re.findall(r'<path[^>]*fill="#000000"', svg)
    assert len(engrave_paths) >= 15, (
        f"{kid}: too few engraved glyphs ({len(engrave_paths)}) — dedication missing?"
    )


@pytest.mark.parametrize("kid", ["emory", "nora"])
def test_panel_outer_dimensions_match_face(kid):
    svg = render_back_panel_svg(kid)
    assert f'width="{PANEL_SIZE_MM}mm"' in svg
    assert f'height="{PANEL_SIZE_MM}mm"' in svg


def test_switch_positions_align_with_pcb_mirrored():
    """Buttons are at PCB X=168.5, which — after the outside-view X-mirror —
    puts them on the LEFT side of the back panel. This is the correct
    orientation: when the panel is installed (not flipped), the cutouts
    align with the tact switches on the PCB's bottom side.
    """
    from enclosure.scripts.render_back_panel import PANEL_SIZE_MM
    switches = _read_switch_positions()
    refs = {s[0] for s in switches}
    assert refs == {"SW1", "SW2", "SW3"}
    # Mirror: panel X = PANEL_SIZE - (PCB X + BORDER)
    expected_panel_x = PANEL_SIZE_MM - (168.5 + BORDER_MM)
    for _ref, x, _y in switches:
        assert x == pytest.approx(expected_panel_x, abs=0.01)
    # Switches are in the upper-LEFT region of the panel (outside view) —
    # same Y as on the PCB, but mirrored to the left.
    for _ref, x, y in switches:
        assert x < PANEL_SIZE_MM / 2, f"switch X {x} should be on the left half"
        assert 20 <= y <= 55, f"switch Y {y} outside expected vertical range"


def test_corner_screws_at_expected_inset():
    svg = render_back_panel_svg("emory")
    # svgwrite emits circle attributes alphabetically: cx, cy, fill, r, stroke.
    circle_re = re.compile(
        r'<circle\b[^>]*?cx="([\d.]+)"[^>]*?cy="([\d.]+)"[^>]*?r="([\d.]+)"',
        re.DOTALL,
    )
    screw_r = SCREW_CLEARANCE_MM / 2
    holes = [
        (float(cx), float(cy))
        for cx, cy, r in circle_re.findall(svg)
        if abs(float(r) - screw_r) < 0.01
    ]
    # 4 corner screws — the only circles with the M3 clearance radius.
    assert len(holes) == 4
    # Each screw must be `SCREW_CORNER_INSET_MM` in from BOTH adjacent edges
    # (not a single-edge inset like the old mid-edge layout).
    for cx, cy in holes:
        assert min(cx, PANEL_SIZE_MM - cx) == pytest.approx(SCREW_CORNER_INSET_MM, abs=0.01)
        assert min(cy, PANEL_SIZE_MM - cy) == pytest.approx(SCREW_CORNER_INSET_MM, abs=0.01)
