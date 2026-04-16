"""Tests for fonts: downloads + caches Jost and Fraunces variable TTFs,
plus glyph-path extraction from static instances.
"""
from pathlib import Path
import pytest

from enclosure.scripts.fonts import (
    download_fonts,
    FONTS_DIR,
    load_font_instance,
    render_glyph,
)


def test_download_fonts_creates_files():
    download_fonts()
    assert (FONTS_DIR / "Jost-Variable.ttf").is_file()
    assert (FONTS_DIR / "Fraunces-Variable.ttf").is_file()


def test_download_fonts_is_idempotent():
    """Second call must not re-download."""
    download_fonts()
    jost = FONTS_DIR / "Jost-Variable.ttf"
    mtime_before = jost.stat().st_mtime
    download_fonts()  # second call
    mtime_after = jost.stat().st_mtime
    assert mtime_before == mtime_after, "Font file was re-downloaded unexpectedly"


def test_load_jost_bold_returns_static_font():
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    # Static instance has no fvar table
    assert "fvar" not in font, "Should be a static instance, not variable"


def test_render_glyph_returns_path_and_bbox():
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path_data, bbox = render_glyph(font, "E")
    assert path_data, "Path data should be non-empty"
    assert bbox is not None, "Bbox should be defined"
    assert bbox[2] > bbox[0], "Bbox should have positive width"
    assert bbox[3] > bbox[1], "Bbox should have positive height"


def test_render_glyph_for_O_includes_inner_counter():
    """Letter 'O' has a closed counter — path should have 2 subpaths (M commands)."""
    download_fonts()
    font = load_font_instance("Jost-Variable.ttf", weight=700)
    path_data, _ = render_glyph(font, "O")
    move_count = sum(1 for c in path_data if c in "Mm")
    assert move_count == 2, f"Letter O should have outer + inner subpath, got {move_count}"
