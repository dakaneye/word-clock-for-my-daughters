"""Font handling: download Jost-Variable.ttf and Fraunces-Variable.ttf
from Google Fonts (SIL OFL) and cache locally.
"""
from pathlib import Path
import urllib.request

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
