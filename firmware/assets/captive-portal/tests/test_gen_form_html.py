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


# -----------------------------------------------------------------------------
# Tier 1 structural checks for the /scan polling logic.
# These scan the raw template for expected identifiers; they do NOT execute
# the JS. Tier 2 (test_form_behavior.py) covers actual runtime behavior.
# -----------------------------------------------------------------------------

TEMPLATE_PATH = Path(__file__).resolve().parents[1] / "form.html.template"


def _template_text() -> str:
    return TEMPLATE_PATH.read_text()


def test_polling_interval_is_2000ms():
    """POLL_INTERVAL_MS constant exists and equals 2000."""
    assert re.search(r"POLL_INTERVAL_MS\s*=\s*2000", _template_text()), (
        "POLL_INTERVAL_MS constant missing or changed from 2000"
    )


def test_polling_timeout_is_30s():
    """TIMEOUT_MS constant exists and equals 30000."""
    assert re.search(r"TIMEOUT_MS\s*=\s*30000", _template_text()), (
        "TIMEOUT_MS constant missing or changed from 30000"
    )


def test_polling_stops_on_nonempty_result():
    """Stop-on-success path exists (length>0 check + timer cleared)."""
    text = _template_text()
    assert re.search(r"nets\.length\s*>\s*0", text), \
        "Missing nets.length > 0 stop condition"
    assert "clearTimeout" in text, \
        "Missing clearTimeout call on success path"


def test_timeout_shows_refresh_button():
    """Timeout path creates a visible Refresh button that restarts polling."""
    text = _template_text()
    assert "Refresh networks" in text, "Missing 'Refresh networks' button text"
    assert re.search(r"renderTimeoutRetry", text), \
        "Missing renderTimeoutRetry function"


def test_no_single_line_comments_in_js():
    """//-style comments are forbidden inside <script> — they consume the
    rest of the line after _collapse_newlines() joins everything with spaces."""
    text = _template_text()
    # Extract <script>...</script> body (the template has one script block).
    m = re.search(r"<script>(.*?)</script>", text, re.DOTALL)
    assert m, "Could not locate <script> block in template"
    script_body = m.group(1)
    # Strip string literals (single + double + backtick) before checking for //.
    stripped = re.sub(r'"(?:[^"\\]|\\.)*"', '', script_body)
    stripped = re.sub(r"'(?:[^'\\]|\\.)*'", '', stripped)
    stripped = re.sub(r"`(?:[^`\\]|\\.)*`", '', stripped)
    # Any //... sequence left is a real line comment.
    assert "//" not in stripped, (
        "Found // comment inside <script>; use /* ... */ instead. "
        "// comments break when _collapse_newlines() joins lines with spaces."
    )


def test_generated_js_parses_as_valid_js():
    """Collapsed preview HTML's JS must be syntactically valid.

    Runs `node --check` on the extracted <script> body. This catches
    post-collapse syntax errors — e.g., missing semicolons where ASI fails
    once newlines become spaces.
    """
    import shutil
    import tempfile

    node = shutil.which("node")
    if node is None:
        pytest.skip("node not available; install Node.js to run this check")

    # Emit a header (which triggers _collapse_newlines), then extract JS.
    with tempfile.TemporaryDirectory() as td:
        header = Path(td) / "form_html.h"
        subprocess.run(
            [sys.executable, str(SCRIPT), "--kid", "emory", "--out", str(header)],
            check=True,
        )
        body = header.read_text()

    # Header contains FORM_HTML raw string; extract the HTML payload.
    m = re.search(r'R"WCHTML\((.*?)\)WCHTML"', body, re.DOTALL)
    assert m, "Could not find raw string literal in generated header"
    collapsed_html = m.group(1)

    # Extract the script body from collapsed HTML.
    ms = re.search(r"<script>(.*?)</script>", collapsed_html, re.DOTALL)
    assert ms, "Collapsed HTML lost its <script> block"
    js_body = ms.group(1)

    # Hand to node --check. Success = exit code 0.
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False) as f:
        f.write(js_body)
        js_path = f.name
    try:
        result = subprocess.run(
            [node, "--check", js_path],
            capture_output=True, text=True,
        )
        assert result.returncode == 0, (
            f"node --check failed:\nstderr:\n{result.stderr}\nstdout:\n{result.stdout}"
        )
    finally:
        Path(js_path).unlink()
