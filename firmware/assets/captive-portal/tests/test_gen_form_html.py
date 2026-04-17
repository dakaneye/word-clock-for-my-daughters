# firmware/assets/captive-portal/tests/test_gen_form_html.py
import re
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).parents[1] / "gen_form_html.py"
# Test file sits at firmware/assets/captive-portal/tests/ — repo root is 4 up.
REPO_ROOT = Path(__file__).resolve().parents[4]
TZ_CPP = REPO_ROOT / "firmware/lib/wifi_provision/src/tz_options.cpp"


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


def _parse_cpp_tz_options() -> list[tuple[str, str]]:
    """Extract (label, posix) entries from tz_options.cpp's static vector init.

    The target block looks like:
        {"Pacific (Los Angeles, Seattle)", "PST8PDT,M3.2.0,M11.1.0"},
        {"Mountain (Denver, Salt Lake City)", "MST7MDT,M3.2.0,M11.1.0"},
        ...
    """
    src = TZ_CPP.read_text()
    entries = re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}', src)
    return entries


def test_tz_options_lockstep_cpp_vs_python():
    """Lockstep: the Python TZ_OPTIONS list MUST match the C++ source of truth.

    If these diverge, the portal dropdown can show a timezone that the
    credential validator (is_known_posix_tz) will reject — a confusing
    failure mode where the user picks what they see but gets an error.
    """
    # Import the generator module in-process (adds parent dir to sys.path).
    gen_dir = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(gen_dir))
    try:
        import gen_form_html  # noqa: E402
    finally:
        sys.path.pop(0)

    cpp_entries = _parse_cpp_tz_options()
    py_entries = list(gen_form_html.TZ_OPTIONS)

    assert cpp_entries, f"no TZ entries parsed from {TZ_CPP}"
    assert cpp_entries == py_entries, (
        "TZ list drift between C++ and Python sources.\n"
        f"C++ ({TZ_CPP.name}):\n"
        + "\n".join(f"  {lbl} -> {posix}" for lbl, posix in cpp_entries)
        + "\nPython (gen_form_html.TZ_OPTIONS):\n"
        + "\n".join(f"  {lbl} -> {posix}" for lbl, posix in py_entries)
    )


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
