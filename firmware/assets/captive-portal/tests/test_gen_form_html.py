# firmware/assets/captive-portal/tests/test_gen_form_html.py
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).parents[1] / "gen_form_html.py"


def run_preview(kid: str) -> str:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--preview", "--kid", kid],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


def test_preview_contains_kid_name():
    for kid, expected in [("emory", "Emory"), ("nora", "Nora")]:
        out = run_preview(kid)
        assert f"Setting up {expected}'s clock" in out


def test_preview_has_all_eight_tz_options():
    out = run_preview("emory")
    for label in (
        "Pacific (Los Angeles, Seattle)",
        "Mountain (Denver, Salt Lake City)",
        "Arizona (Phoenix, no DST)",
        "Central (Chicago, Dallas)",
        "Eastern (New York, Miami)",
        "Alaska (Anchorage)",
        "Hawaii (Honolulu)",
        "UTC",
    ):
        assert label in out


def test_pacific_is_preselected():
    out = run_preview("emory")
    assert 'selected value="PST8PDT,M3.2.0,M11.1.0"' in out or \
           'value="PST8PDT,M3.2.0,M11.1.0" selected' in out


def test_kid_accent_color_differs_per_kid():
    emory = run_preview("emory")
    nora = run_preview("nora")
    assert "#b88b4a" in emory
    assert "#b88b4a" not in nora
    assert "#3d2817" in nora
    assert "#3d2817" not in emory


def test_embedded_header_is_valid_c_literal(tmp_path):
    out_file = tmp_path / "form_html.h"
    subprocess.run(
        [sys.executable, str(SCRIPT), "--kid", "emory", "--out", str(out_file)],
        check=True,
    )
    contents = out_file.read_text()
    assert "const char FORM_HTML[]" in contents
    assert "PROGMEM" in contents
    # Ensure no raw backslashes, quotes, or newlines break the literal
    # (quick sanity — a full parse would need a C compiler)
    start = contents.index('"')
    end = contents.rindex('"')
    literal_body = contents[start + 1 : end]
    for forbidden in ["\n", "\r"]:
        assert forbidden not in literal_body, f"Raw {forbidden!r} in string literal"
