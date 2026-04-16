"""Tests for path_ops: boolean-union of overlapping glyph subpaths."""
import pytest

from enclosure.scripts.path_ops import union_glyph_path
from enclosure.scripts.fonts import load_font_instance, render_glyph


def _count_subpaths(path: str) -> int:
    """Count M commands — each M starts a subpath."""
    return sum(1 for c in path if c in "Mm")


def test_union_preserves_simple_rectangle():
    """A single rectangle (like Jost I) should survive union unchanged in shape."""
    # Rectangle: 0,0 → 100,0 → 100,200 → 0,200 → close
    rect = "M0,0L100,0L100,200L0,200Z"
    result = union_glyph_path(rect)
    assert _count_subpaths(result) == 1, "Single rectangle should remain one subpath"


def test_union_merges_overlapping_rectangles():
    """Two overlapping rectangles should union into ONE subpath."""
    # L-shape: horizontal bar overlapping vertical bar
    r1 = "M0,0L100,0L100,20L0,20Z"   # horizontal bar at bottom
    r2 = "M0,0L20,0L20,100L0,100Z"   # vertical bar on left
    combined = r1 + r2
    result = union_glyph_path(combined)
    assert _count_subpaths(result) == 1, f"Two overlapping rects should union to 1, got: {result}"


def test_union_jost_H_collapses_to_one_outline():
    """Jost Bold H = 2 stems + crossbar = 3 subpaths. Union should be 1 outer outline (no inner holes)."""
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path, _ = render_glyph(font, "H")
    assert _count_subpaths(path) == 3, "Pre-union H should have 3 overlapping rectangles"
    unioned = union_glyph_path(path)
    assert _count_subpaths(unioned) == 1, f"Unioned H should be 1 outer path, got {_count_subpaths(unioned)}"


def test_union_jost_E_collapses_to_one_outline():
    """Jost Bold E = stem + 3 bars = 4 subpaths. E has NO closed counter.
    Union should yield a single outer outline."""
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path, _ = render_glyph(font, "E")
    assert _count_subpaths(path) == 4, "Pre-union E should have 4 overlapping rectangles"
    unioned = union_glyph_path(path)
    assert _count_subpaths(unioned) == 1, f"Unioned E should be 1 outer path, got {_count_subpaths(unioned)}"


def test_union_jost_O_keeps_outer_plus_inner():
    """Jost Bold O = outer + inner counter = 2 subpaths. Union should preserve both."""
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path, _ = render_glyph(font, "O")
    pre = _count_subpaths(path)
    assert pre == 2, "Pre-union O should have outer + inner counter"
    unioned = union_glyph_path(path)
    assert _count_subpaths(unioned) == 2, f"Unioned O should keep outer + inner hole, got {_count_subpaths(unioned)}"


def test_union_jost_A_keeps_outer_plus_inner():
    """Jost Bold A = outer with inner counter. Crossbar is inside the outer outline.
    Unioned result should have exterior + 0 or 1 interior (depending on whether
    the counter is fully enclosed or has a gap)."""
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path, _ = render_glyph(font, "A")
    unioned = union_glyph_path(path)
    count = _count_subpaths(unioned)
    # A has one outer path and at most one hole (above crossbar, if closed)
    assert count in (1, 2), f"Unioned A should have 1 or 2 subpaths, got {count}"


def test_union_empty_path_is_safe():
    result = union_glyph_path("")
    assert result == ""
