"""Tests for fonts: downloads + caches Jost and Fraunces variable TTFs."""
from pathlib import Path
import pytest

from enclosure.scripts.fonts import download_fonts, FONTS_DIR


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
