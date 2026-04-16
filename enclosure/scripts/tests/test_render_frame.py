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
    # Should have multiple line commands traversing fingers + slots
    line_count = path.count("l")
    assert line_count >= FINGER_COUNT, f"Expected >= {FINGER_COUNT} line cmds, got {line_count}"


def test_finger_path_inverted_differs():
    a = finger_path_for_edge(invert=False)
    b = finger_path_for_edge(invert=True)
    assert a != b, "Inverted finger pattern should differ from default"


from enclosure.scripts.render_frame import frame_strip_path


def test_frame_strip_path_starts_with_M():
    path = frame_strip_path(invert_left=False, invert_right=True)
    assert path.startswith("M")


def test_frame_strip_path_closes():
    path = frame_strip_path(invert_left=False, invert_right=True)
    assert path.rstrip().endswith("Z") or path.rstrip().endswith("z")


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
    assert 'width="192.0mm"' in svg_text
    assert 'height="207.0mm"' in svg_text


def _trace_path_x_extents(path_data: str) -> tuple:
    """Trace through a path-data string and return (min_x, max_x) reached.
    Handles M/L/H/V/Z and lowercase relative variants. Returns (min_x, max_x)."""
    import re
    tokens = re.findall(r'[MLHVZmlhvz]|-?\d+\.?\d*', path_data)
    x = y = 0.0
    min_x = max_x = 0.0
    i = 0
    cmd = None
    while i < len(tokens):
        t = tokens[i]
        if t in 'MLHVZmlhvz':
            cmd = t
            i += 1
            if t in 'Zz':
                continue
        if cmd in 'M':
            x = float(tokens[i]); y = float(tokens[i+1]); i += 2
        elif cmd == 'm':
            x += float(tokens[i]); y += float(tokens[i+1]); i += 2
        elif cmd == 'L':
            x = float(tokens[i]); y = float(tokens[i+1]); i += 2
        elif cmd == 'l':
            x += float(tokens[i]); y += float(tokens[i+1]); i += 2
        elif cmd == 'H':
            x = float(tokens[i]); i += 1
        elif cmd == 'h':
            x += float(tokens[i]); i += 1
        elif cmd == 'V':
            y = float(tokens[i]); i += 1
        elif cmd == 'v':
            y += float(tokens[i]); i += 1
        min_x = min(min_x, x)
        max_x = max(max_x, x)
    return min_x, max_x


def test_strip_path_stays_within_strip_body():
    """Critical: fingers must NOT extend beyond the strip's nominal length.
    Slots cut INWARD; fingers stay at the body edge.
    """
    path = frame_strip_path(invert_left=False, invert_right=True)
    min_x, max_x = _trace_path_x_extents(path)
    assert min_x >= 0, f"Path went to X={min_x}, should not go below 0 (left strip edge)"
    assert max_x <= FRAME_LENGTH_MM, (
        f"Path reached X={max_x}, must not exceed FRAME_LENGTH_MM={FRAME_LENGTH_MM}"
    )


def test_strip_path_uses_full_strip_length():
    """Sanity: the path SHOULD reach both X=0 and X=FRAME_LENGTH_MM (strip edges)."""
    path = frame_strip_path(invert_left=False, invert_right=True)
    min_x, max_x = _trace_path_x_extents(path)
    assert min_x == pytest.approx(0, abs=0.01)
    assert max_x == pytest.approx(FRAME_LENGTH_MM, abs=0.01)
