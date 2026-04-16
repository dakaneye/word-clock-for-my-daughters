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
