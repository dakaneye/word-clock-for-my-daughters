"""Font handling: download Jost-Variable.ttf and Fraunces-Variable.ttf
from Google Fonts (SIL OFL), cache locally, instance at a given weight,
and extract glyph outlines as SVG path data.
"""
from pathlib import Path
from typing import Optional, Tuple
import urllib.request

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen

FONTS_DIR = Path(__file__).parent / "fonts"

FONT_URLS = {
    "Jost-Variable.ttf": "https://github.com/google/fonts/raw/main/ofl/jost/Jost%5Bwght%5D.ttf",
    "Fraunces-Variable.ttf": "https://github.com/google/fonts/raw/main/ofl/fraunces/Fraunces%5BSOFT%2CWONK%2Copsz%2Cwght%5D.ttf",
}


def download_fonts() -> None:
    """Download Jost + Fraunces TTFs to FONTS_DIR if not already present.
    Idempotent: existing files are not re-downloaded.
    """
    FONTS_DIR.mkdir(parents=True, exist_ok=True)
    for filename, url in FONT_URLS.items():
        target = FONTS_DIR / filename
        if target.is_file() and target.stat().st_size > 0:
            continue
        with urllib.request.urlopen(url) as response:
            target.write_bytes(response.read())


def load_font_instance(filename: str, weight: int, opsz: Optional[float] = None,
                       soft: float = 0, wonk: float = 0) -> TTFont:
    """Load a variable font from FONTS_DIR and return a static instance at the given axes.

    Jost has only `wght` axis; Fraunces has `wght`, `opsz`, `SOFT`, `WONK`.
    Pass opsz only for Fraunces.
    """
    download_fonts()
    font = TTFont(FONTS_DIR / filename)
    axes = {"wght": float(weight)}
    if "fvar" in font:
        available_axes = {a.axisTag for a in font["fvar"].axes}
        if "opsz" in available_axes and opsz is not None:
            axes["opsz"] = float(opsz)
        if "SOFT" in available_axes:
            axes["SOFT"] = float(soft)
        if "WONK" in available_axes:
            axes["WONK"] = float(wonk)
    return instantiateVariableFont(font, axes)


def render_glyph(font: TTFont, char: str) -> Tuple[str, Optional[Tuple[float, float, float, float]]]:
    """Render a single character to (svg_path_data, bbox).

    bbox is (xMin, yMin, xMax, yMax) in font units, or None for empty glyphs.
    Path data is in the font's native coordinate system (Y up).
    """
    cmap = font.getBestCmap()
    if ord(char) not in cmap:
        raise ValueError(f"Character {char!r} not in font")
    glyph_name = cmap[ord(char)]
    glyph_set = font.getGlyphSet()
    glyph = glyph_set[glyph_name]

    pen = SVGPathPen(glyph_set)
    glyph.draw(pen)
    path_data = pen.getCommands()

    bp = BoundsPen(glyph_set)
    glyph.draw(bp)

    return path_data, bp.bounds
