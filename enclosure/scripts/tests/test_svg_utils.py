"""Tests for SVG conventions: Ponoko-required formatting."""
from enclosure.scripts.svg_utils import (
    PONOKO_CUT_COLOR,
    PONOKO_STROKE_WIDTH_MM,
    new_svg_document,
    add_cut_path,
    add_cut_rect,
)


def test_ponoko_cut_color_is_pure_blue():
    assert PONOKO_CUT_COLOR == "#0000FF"


def test_stroke_width_is_hairline():
    assert PONOKO_STROKE_WIDTH_MM == 0.01


def test_new_svg_document_uses_mm_units():
    dwg = new_svg_document(width_mm=192.0, height_mm=192.0)
    xml = dwg.tostring()
    assert 'width="192.0mm"' in xml
    assert 'height="192.0mm"' in xml
    assert 'viewBox="0 0 192.0 192.0"' in xml


def test_add_cut_rect_uses_blue_hairline_no_fill():
    dwg = new_svg_document(width_mm=192.0, height_mm=192.0)
    add_cut_rect(dwg, x=0, y=0, width=192.0, height=192.0)
    xml = dwg.tostring()
    assert 'stroke="#0000FF"' in xml
    assert 'stroke-width="0.01"' in xml
    assert 'fill="none"' in xml
