# Captive Portal Scan Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship three fixes to the shipped `wifi_provision` captive portal: enable station-mode scanning (`WIFI_AP_STA`), raise AP client slots (`max_connection = 4`), and convert the form's one-shot `/scan` fetch into a polling loop with timeout + manual refresh button — so first-time provisioning actually completes.

**Architecture:** Two-line firmware edits in `wifi_provision.cpp` + full JS replacement in `form.html.template`. Test coverage in three layers: structural pytest (regex checks on the generated template), Playwright behavior tests (render the preview, mock `/scan`, assert DOM changes over time), manual hardware checks. Preview mock gets extended so behavior tests can script `/scan` responses across calls.

**Tech Stack:** Arduino-ESP32 core, WebServer, WiFi.softAP, pytest, playwright + pytest-playwright (new dev deps), Node.js (already present; used by the existing PlatformIO toolchain).

---

## Spec reference

Implements `docs/superpowers/specs/2026-04-20-captive-portal-scan-fix-design.md`. Read the spec in full — it pins every decision (2 s polling interval, 30 s timeout, stop-on-success, Refresh button UX, regression scope across the state machine, the rationale for each of the three Tiers of testing). If a decision feels underspecified, re-read the spec before inventing.

## File structure

```
firmware/
  lib/wifi_provision/
    src/wifi_provision.cpp              # MODIFIED (2 one-line edits)
  assets/captive-portal/
    form.html.template                  # MODIFIED (JS block replaced, ~40 lines)
    gen_form_html.py                    # MODIFIED (extend emit_preview mock)
    tests/
      test_gen_form_html.py             # MODIFIED (append 6 Tier 1 tests)
      test_form_behavior.py             # NEW (5 Tier 2 Playwright tests)
      requirements.txt                  # NEW (playwright, pytest-playwright)
      conftest.py                       # NEW (fixture helpers for Playwright)
  test/hardware_checks/
    wifi_provision_checks.md            # MODIFIED (append checks #8, #9, #10)
TODO.md                                 # MODIFIED (mark captive-portal-scan-fix [x])
```

## Build / test verification recipes (used throughout)

Run from repository root unless noted.

- **Structural pytest (Tier 1):**
  `cd firmware/assets/captive-portal && python -m pytest tests/test_gen_form_html.py -v`
  - Baseline before this plan: **6 tests pass**.
  - After Task 5: **12 tests pass** (+ 6 new).

- **Playwright behavior tests (Tier 2):**
  `cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py -v`
  - Baseline: test file does not exist.
  - After Task 10: **5 tests pass**.

- **Native C++ test suite:**
  `cd firmware && pio test -e native`
  - Must remain **172/172 passing** throughout this plan.

- **ESP32 builds (Emory + Nora):**
  `cd firmware && pio run -e emory`
  `cd firmware && pio run -e nora`
  - Must remain green throughout.

Run the relevant checks before every commit.

---

## Task 1: Set up Playwright dev dependencies

Locks the Tier 2 tooling in place before any tests reference it. Adds a `requirements.txt` at the captive-portal test root so future contributors get the same setup.

**Files:**
- Create: `firmware/assets/captive-portal/tests/requirements.txt`

- [ ] **Step 1: Create `requirements.txt`**

Write this file:

```
# Tier 1 (structural) only needs pytest — no extra deps.
# Tier 2 (behavior) needs Playwright for headless browser assertions.
pytest>=8.0
playwright>=1.40
pytest-playwright>=0.5
```

- [ ] **Step 2: Install dependencies locally**

Run:
```bash
cd firmware/assets/captive-portal/tests
pip install -r requirements.txt
playwright install chromium
```

Expected: pip reports install of `pytest`, `playwright`, `pytest-playwright`. `playwright install chromium` downloads ~170 MB on first run (subsequent runs skip); final line says "chromium ... installed" or "already installed".

- [ ] **Step 3: Verify Playwright works**

Run:
```bash
cd firmware/assets/captive-portal/tests
python -c "from playwright.sync_api import sync_playwright; print('ok')"
```

Expected output: `ok` on its own line.

- [ ] **Step 4: Commit**

```bash
git add firmware/assets/captive-portal/tests/requirements.txt
git commit -m "test: add Playwright dev deps for captive portal behavior tests"
```

---

## Task 2: Extend preview mock to support scripted `/scan` responses

The existing preview mock (`emit_preview` in `gen_form_html.py`) injects a `window.fetch = async () => ...` stub that returns a static 4-network list. Tier 2 tests need the mock to return **different responses across calls** (e.g. `[]`, `[]`, networks) and must be scriptable from the test side.

**Approach:** parameterize `emit_preview` to accept an inline JS mock string; default to the current 4-network behavior. Tests pass their own mock string.

**Files:**
- Modify: `firmware/assets/captive-portal/gen_form_html.py` (function `emit_preview`)

- [ ] **Step 1: Read the current `emit_preview` function**

Open `firmware/assets/captive-portal/gen_form_html.py` and locate lines 107-124 (the `emit_preview` function).

- [ ] **Step 2: Extend `emit_preview` signature and default mock**

Replace the existing `emit_preview` function body with:

```python
DEFAULT_PREVIEW_MOCK = """
    <script>
      window.fetch = async () => ({ json: async () => ([
          {ssid: 'HomeWiFi-5G', rssi: -45, secured: true},
          {ssid: 'HomeWiFi-2.4', rssi: -52, secured: true},
          {ssid: 'Neighbor', rssi: -70, secured: true},
          {ssid: 'Guest', rssi: -60, secured: false},
      ])});
    </script>
    """


def emit_preview(kid: str, mock_script: str | None = None) -> str:
    template = TEMPLATE.read_text()
    html = substitute(template, kid,
                      csrf_placeholder="preview-csrf",
                      error_placeholder="")
    mock = mock_script if mock_script is not None else DEFAULT_PREVIEW_MOCK
    return html.replace("</body>", mock + "</body>")
```

- [ ] **Step 3: Re-run existing pytest to confirm no regression**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_gen_form_html.py -v
```

Expected: all 6 existing tests pass (the default behavior is unchanged).

- [ ] **Step 4: Commit**

```bash
git add firmware/assets/captive-portal/gen_form_html.py
git commit -m "test: parameterize preview mock for scripted /scan responses"
```

---

## Task 3: Add `conftest.py` fixtures for Playwright tests

Playwright tests need a helper that serves the rendered preview HTML to the browser. Cleanest approach: serve from a local HTTP server (required because `fetch()` doesn't work on `file://` in Chromium). Use Python's stdlib `http.server` in a background thread, one per test.

**Files:**
- Create: `firmware/assets/captive-portal/tests/conftest.py`

- [ ] **Step 1: Write the conftest**

Create `firmware/assets/captive-portal/tests/conftest.py`:

```python
# firmware/assets/captive-portal/tests/conftest.py
"""Pytest fixtures for Playwright behavior tests.

Renders the captive portal preview HTML with a scriptable /scan mock,
serves it from a local HTTP server (required so fetch() works in
Chromium), and yields the URL.
"""
from __future__ import annotations

import http.server
import socketserver
import sys
import threading
from pathlib import Path
from typing import Callable

import pytest

GEN_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GEN_DIR))
import gen_form_html  # noqa: E402


@pytest.fixture
def serve_preview(tmp_path: Path) -> Callable[[str, str | None], str]:
    """Render the preview for a kid + optional mock; return the served URL.

    Tests call `serve_preview(kid='emory', mock_script='<script>...</script>')`
    and get back a URL like http://localhost:<port>/ serving the HTML.
    """
    servers: list[socketserver.TCPServer] = []

    def _serve(kid: str = "emory", mock_script: str | None = None) -> str:
        html = gen_form_html.emit_preview(kid, mock_script)
        doc = tmp_path / f"{kid}.html"
        doc.write_text(html)

        handler = http.server.SimpleHTTPRequestHandler
        handler_factory = _make_handler(tmp_path)
        server = socketserver.TCPServer(("127.0.0.1", 0), handler_factory)
        servers.append(server)
        port = server.server_address[1]
        threading.Thread(target=server.serve_forever, daemon=True).start()
        return f"http://127.0.0.1:{port}/{kid}.html"

    yield _serve

    for s in servers:
        s.shutdown()
        s.server_close()


def _make_handler(root: Path) -> type:
    class QuietHandler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(root), **kwargs)

        def log_message(self, *args, **kwargs):  # silence stdout noise
            pass

    return QuietHandler
```

- [ ] **Step 2: Commit**

```bash
git add firmware/assets/captive-portal/tests/conftest.py
git commit -m "test: add conftest fixtures for captive portal Playwright tests"
```

---

## Task 4: Write Tier 1 structural pytest (6 new tests)

Structural regression coverage for the polling JS. Each test reads the template file directly and runs regex checks to confirm the expected identifiers / constants / control-flow shapes are present. Catches accidental constant changes or code deletion during future refactors.

These tests are committed **before** the JS changes in Task 8; the ones checking new polling identifiers will **fail** against the current template (that's the red in red-green).

**Files:**
- Modify: `firmware/assets/captive-portal/tests/test_gen_form_html.py` (append tests)

- [ ] **Step 1: Append Tier 1 tests to the end of `test_gen_form_html.py`**

Add this block after the last test (`test_embedded_header_is_valid_c_literal`):

```python
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
    import subprocess
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
```

- [ ] **Step 2: Run new tests — expect failures**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_gen_form_html.py -v
```

Expected: the first 6 tests (existing) pass. The 6 new tests mostly FAIL because the polling code doesn't exist yet:
- `test_polling_interval_is_2000ms` — FAIL (no `POLL_INTERVAL_MS`)
- `test_polling_timeout_is_30s` — FAIL (no `TIMEOUT_MS`)
- `test_polling_stops_on_nonempty_result` — FAIL (no `clearTimeout`)
- `test_timeout_shows_refresh_button` — FAIL (no "Refresh networks")
- `test_no_single_line_comments_in_js` — PASS (current JS has no `//` comments either)
- `test_generated_js_parses_as_valid_js` — PASS (current JS is valid)

This mixed result is expected: 2/6 pass on old JS, 4/6 will go green after Task 8.

- [ ] **Step 3: Commit the failing tests**

```bash
git add firmware/assets/captive-portal/tests/test_gen_form_html.py
git commit -m "test: add Tier 1 structural pytest for /scan polling (red)"
```

---

## Task 5: Write Tier 2 test — immediate success path

The simplest Playwright test: mock `/scan` returns networks on the first call; dropdown must populate.

**Files:**
- Create: `firmware/assets/captive-portal/tests/test_form_behavior.py`

- [ ] **Step 1: Create test file with shared mock-builder + first test**

Write `firmware/assets/captive-portal/tests/test_form_behavior.py`:

```python
# firmware/assets/captive-portal/tests/test_form_behavior.py
"""Tier 2 Playwright behavior tests for the /scan polling JS.

Each test injects a scriptable /scan mock into the preview HTML, serves it
from a local HTTP server (see conftest.py), opens it in a headless
Chromium, and asserts DOM state over time.
"""
from __future__ import annotations

import json

from playwright.sync_api import Page, expect


def _make_mock(responses: list[list[dict]], hang: bool = False) -> str:
    """Build a <script> that returns `responses[call_index]` on each /scan.

    Once `responses` is exhausted, repeats the last response. If `hang=True`,
    the fetch Promise never resolves (used to simulate a dead server).
    """
    if hang:
        return "<script>window.fetch = () => new Promise(() => {});</script>"
    encoded = json.dumps(responses)
    return f"""
    <script>
      (function() {{
        const responses = {encoded};
        let i = 0;
        window.fetch = async () => {{
          const payload = responses[Math.min(i, responses.length - 1)];
          i += 1;
          return {{ json: async () => payload }};
        }};
      }})();
    </script>
    """


def test_dropdown_populates_on_immediate_success(serve_preview, page: Page):
    """First /scan returns 4 networks → dropdown shows 4 options within 2 s."""
    mock = _make_mock([[
        {"ssid": "HomeWiFi-5G", "rssi": -45, "secured": True},
        {"ssid": "HomeWiFi-2.4", "rssi": -52, "secured": True},
        {"ssid": "Guest", "rssi": -60, "secured": False},
        {"ssid": "Neighbor", "rssi": -70, "secured": True},
    ]])
    url = serve_preview(kid="emory", mock_script=mock)
    page.goto(url)
    dropdown = page.locator("#ssid")
    expect(dropdown.locator("option")).to_have_count(4, timeout=3000)
    # Spot-check one option text.
    expect(dropdown.locator("option")).to_contain_text(
        ["HomeWiFi-5G", "HomeWiFi-2.4", "Guest (open)", "Neighbor"]
    )
```

- [ ] **Step 2: Run the test — expect failure against old JS**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py::test_dropdown_populates_on_immediate_success -v
```

Expected: **PASS** on old JS. The old one-shot fetch does populate the dropdown from a non-empty first response. This test protects against regressing that behavior.

If it fails: Playwright setup or fixtures are wrong; fix before continuing.

- [ ] **Step 3: Commit**

```bash
git add firmware/assets/captive-portal/tests/test_form_behavior.py
git commit -m "test: Tier 2 — dropdown populates on immediate scan success"
```

---

## Task 6: Write Tier 2 test — polling after initial empty responses

Second behavior test: mock returns `[]` on calls 1 and 2, networks on call 3. Old JS (one-shot) gets the `[]` and gives up. New JS (polling) waits 2 s, polls again, eventually populates.

**Files:**
- Modify: `firmware/assets/captive-portal/tests/test_form_behavior.py` (append test)

- [ ] **Step 1: Append the test**

Add to the bottom of `test_form_behavior.py`:

```python
def test_dropdown_populates_after_polling(serve_preview, page: Page):
    """Two empty responses then networks → dropdown populates within ~6 s.

    At 2 s poll interval: T0=empty, T2=empty, T4=networks → DOM updates.
    """
    mock = _make_mock([
        [],
        [],
        [
            {"ssid": "Home", "rssi": -50, "secured": True},
            {"ssid": "Other", "rssi": -65, "secured": False},
        ],
    ])
    url = serve_preview(kid="emory", mock_script=mock)
    page.goto(url)
    dropdown = page.locator("#ssid")
    # Playwright's 2-count assertion waits up to `timeout` ms for the DOM.
    # 8000 ms covers two 2-second poll cycles + slack.
    expect(dropdown.locator("option")).to_have_count(2, timeout=8000)
    expect(dropdown.locator("option")).to_contain_text(
        ["Home", "Other (open)"]
    )
```

- [ ] **Step 2: Run the test — expect failure against old JS**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py::test_dropdown_populates_after_polling -v
```

Expected: **FAIL**. Old JS fetches once, gets `[]`, renders "No networks found — try again" as a single option — never retries.

- [ ] **Step 3: Commit the failing test**

```bash
git add firmware/assets/captive-portal/tests/test_form_behavior.py
git commit -m "test: Tier 2 — dropdown populates after polling (red)"
```

---

## Task 7: Write Tier 2 test — timeout shows Refresh button

Mock returns `[]` forever. After 30 s, dropdown should say "No networks found — click Refresh" and a `<button>Refresh networks</button>` must be visible next to the dropdown.

**Files:**
- Modify: `firmware/assets/captive-portal/tests/test_form_behavior.py` (append test)

- [ ] **Step 1: Append the test**

```python
def test_timeout_shows_refresh_button(serve_preview, page: Page):
    """Empty responses for 30 s → "Refresh networks" button appears."""
    mock = _make_mock([[]])  # repeats [] forever (list has 1 entry, mock repeats last)
    url = serve_preview(kid="emory", mock_script=mock)
    page.goto(url)

    refresh = page.get_by_role("button", name="Refresh networks")
    # 32-second timeout to accommodate the 30s TIMEOUT_MS + poll interval + slack.
    expect(refresh).to_be_visible(timeout=32000)

    dropdown = page.locator("#ssid")
    expect(dropdown.locator("option")).to_have_count(1)
    expect(dropdown.locator("option")).to_contain_text(["click Refresh"])
```

- [ ] **Step 2: Run the test — expect failure against old JS**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py::test_timeout_shows_refresh_button -v
```

Expected: **FAIL**. Old JS has no refresh button.

- [ ] **Step 3: Commit**

```bash
git add firmware/assets/captive-portal/tests/test_form_behavior.py
git commit -m "test: Tier 2 — timeout shows Refresh button (red)"
```

---

## Task 8: Write Tier 2 test — Refresh button restarts polling

After timeout, click the Refresh button; mock is then reconfigured to return networks; dropdown must populate.

**Trick:** switching the mock mid-test requires the mock script to read from a mutable source. Simplest approach: expose a `window.__testNetworks` array that the mock reads, and tweak it from Playwright via `page.evaluate`.

**Files:**
- Modify: `firmware/assets/captive-portal/tests/test_form_behavior.py` (append test + helper)

- [ ] **Step 1: Append a mutable-mock helper and the test**

```python
MUTABLE_MOCK = """
<script>
  window.__testNetworks = [];
  window.fetch = async () => ({ json: async () => window.__testNetworks });
</script>
"""


def test_refresh_button_restarts_polling(serve_preview, page: Page):
    """After timeout retry, changing the mock to networks → dropdown populates."""
    url = serve_preview(kid="emory", mock_script=MUTABLE_MOCK)
    page.goto(url)

    # Wait for timeout path (30 s) to surface the Refresh button.
    refresh = page.get_by_role("button", name="Refresh networks")
    expect(refresh).to_be_visible(timeout=32000)

    # Swap the mock to return networks, then click Refresh.
    page.evaluate(
        "window.__testNetworks = "
        "[{ssid:'Restarted', rssi:-50, secured:true}]"
    )
    refresh.click()

    dropdown = page.locator("#ssid")
    expect(dropdown.locator("option")).to_have_count(1, timeout=5000)
    expect(dropdown.locator("option")).to_contain_text(["Restarted"])
```

- [ ] **Step 2: Run the test — expect failure against old JS**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py::test_refresh_button_restarts_polling -v
```

Expected: **FAIL**. Old JS has no refresh button, no polling.

- [ ] **Step 3: Commit**

```bash
git add firmware/assets/captive-portal/tests/test_form_behavior.py
git commit -m "test: Tier 2 — Refresh restarts polling (red)"
```

---

## Task 9: Write Tier 2 test — polling stops after success

Mock returns networks immediately. Over 10 s, at most 2 `fetch` calls are made (the initial one + possibly one racing poll that hadn't been scheduled yet). Dropdown stays populated — critically, it is NOT reset to empty by a later poll.

**Files:**
- Modify: `firmware/assets/captive-portal/tests/test_form_behavior.py` (append test)

- [ ] **Step 1: Append the test**

```python
def test_polling_stops_after_success(serve_preview, page: Page):
    """First /scan returns networks; polling stops; dropdown stays populated."""
    counter_mock = """
    <script>
      window.__fetchCount = 0;
      window.fetch = async () => {
        window.__fetchCount += 1;
        return { json: async () => ([
          {ssid: 'Stable', rssi: -50, secured: true},
        ])};
      };
    </script>
    """
    url = serve_preview(kid="emory", mock_script=counter_mock)
    page.goto(url)

    dropdown = page.locator("#ssid")
    expect(dropdown.locator("option")).to_have_count(1, timeout=3000)
    expect(dropdown.locator("option")).to_contain_text(["Stable"])

    # Wait 10 s — several poll intervals. Polling must have stopped.
    page.wait_for_timeout(10000)

    count = page.evaluate("window.__fetchCount")
    assert count <= 2, f"expected ≤2 /scan calls, saw {count} (polling did not stop)"

    # Dropdown still populated (not reset to empty by a late poll).
    expect(dropdown.locator("option")).to_have_count(1)
    expect(dropdown.locator("option")).to_contain_text(["Stable"])
```

- [ ] **Step 2: Run the test — expect PASS on old JS**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/test_form_behavior.py::test_polling_stops_after_success -v
```

Expected: **PASS**. Old JS fetches exactly once — `__fetchCount == 1`, dropdown has 1 option, stays populated. This test locks in the "don't over-fetch" invariant for both old and new JS.

- [ ] **Step 3: Commit**

```bash
git add firmware/assets/captive-portal/tests/test_form_behavior.py
git commit -m "test: Tier 2 — polling stops after success"
```

---

## Task 10: Confirm full red baseline before implementation

Before implementing the new JS, make one final run of ALL new tests to confirm the expected red pattern. This is the explicit TDD gate.

- [ ] **Step 1: Run the full new test suite**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/ -v
```

Expected counts:

| Test file | Passing | Failing |
|---|---|---|
| `test_gen_form_html.py` — existing 6 | 6 | 0 |
| `test_gen_form_html.py` — new Tier 1 (6) | 2 | 4 |
| `test_form_behavior.py` — new Tier 2 (5) | 2 | 3 |

**Passing now, expected to still pass after Task 11:**
- `test_no_single_line_comments_in_js`
- `test_generated_js_parses_as_valid_js`
- `test_dropdown_populates_on_immediate_success`
- `test_polling_stops_after_success`

**Failing now, expected to pass after Task 11:**
- `test_polling_interval_is_2000ms`
- `test_polling_timeout_is_30s`
- `test_polling_stops_on_nonempty_result`
- `test_timeout_shows_refresh_button`
- `test_dropdown_populates_after_polling`
- `test_timeout_shows_refresh_button` (Tier 2)
- `test_refresh_button_restarts_polling`

If the actual pattern diverges from this table, stop and investigate before writing new JS.

- [ ] **Step 2: Record the baseline in the session log** (no commit — just verification)

---

## Task 11: Replace one-shot fetch with polling loop in `form.html.template`

The core JS change. Replaces the `<script>` block at the bottom of `form.html.template` with the polling state machine.

**Files:**
- Modify: `firmware/assets/captive-portal/form.html.template` (lines 40-63, `<script>` body)

- [ ] **Step 1: Replace the `<script>` block**

In `firmware/assets/captive-portal/form.html.template`, locate the current block:

```html
  <script>
    // Populate SSID dropdown from /scan once it arrives.
    fetch('/scan').then(r => r.json()).then(nets => {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '';
      const seen = new Set();
      for (const n of nets) {
        if (seen.has(n.ssid)) continue;
        seen.add(n.ssid);
        const opt = document.createElement('option');
        opt.value = n.ssid;
        opt.textContent = n.ssid + (n.secured ? '' : ' (open)');
        sel.appendChild(opt);
      }
      if (sel.children.length === 0) {
        const opt = document.createElement('option');
        opt.value = '';
        opt.textContent = 'No networks found — try again';
        sel.appendChild(opt);
      }
    }).catch(() => {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '<option value="">Scan failed — reload</option>';
    });
  </script>
```

Replace with:

```html
  <script>
    /* Poll /scan every 2s until populated or 30s timeout. Stop on first
       non-empty result — the server calls WiFi.scanDelete() after returning
       results, so a subsequent poll would return [] and erase the dropdown. */
    const POLL_INTERVAL_MS = 2000;
    const TIMEOUT_MS = 30000;
    let startedAt = 0;
    let timerId = null;

    function renderSsidList(nets) {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '';
      const seen = new Set();
      for (const n of nets) {
        if (seen.has(n.ssid)) continue;
        seen.add(n.ssid);
        const opt = document.createElement('option');
        opt.value = n.ssid;
        opt.textContent = n.ssid + (n.secured ? '' : ' (open)');
        sel.appendChild(opt);
      }
    }

    function renderTimeoutRetry() {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '';
      const opt = document.createElement('option');
      opt.value = '';
      opt.textContent = 'No networks found — click Refresh';
      sel.appendChild(opt);
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.textContent = 'Refresh networks';
      btn.onclick = () => { btn.remove(); startPolling(); };
      sel.parentNode.insertBefore(btn, sel.nextSibling);
    }

    function pollScanOnce() {
      fetch('/scan').then(r => r.json()).then(nets => {
        if (nets.length > 0) {
          clearTimeout(timerId);
          timerId = null;
          renderSsidList(nets);
          return;
        }
        if (Date.now() - startedAt >= TIMEOUT_MS) {
          renderTimeoutRetry();
          return;
        }
        timerId = setTimeout(pollScanOnce, POLL_INTERVAL_MS);
      }).catch(() => {
        if (Date.now() - startedAt >= TIMEOUT_MS) {
          renderTimeoutRetry();
          return;
        }
        timerId = setTimeout(pollScanOnce, POLL_INTERVAL_MS);
      });
    }

    function startPolling() {
      startedAt = Date.now();
      pollScanOnce();
    }

    startPolling();
  </script>
```

Note the `/* ... */` block comment at the top — intentional; `//` comments would be consumed after `_collapse_newlines()` joins lines.

- [ ] **Step 2: Run the full pytest suite**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/ -v
```

Expected: **17 tests pass, 0 fail** (6 existing + 6 new Tier 1 + 5 new Tier 2). If any fail, fix before committing.

- [ ] **Step 3: Commit**

```bash
git add firmware/assets/captive-portal/form.html.template
git commit -m "feat(captive-portal): poll /scan every 2s with 30s timeout + refresh

Replaces the one-shot fetch('/scan') with a polling state machine. Stops
on first non-empty response (critical: server WiFi.scanDelete()s after
serving results, so a second poll would return [] and erase the
dropdown). After 30s of empty/failed responses, shows 'No networks found
— click Refresh' plus a Refresh button that restarts polling.

Tier 1 pytest (6) + Tier 2 Playwright (5) now green."
```

---

## Task 12: Flip `WIFI_AP` to `WIFI_AP_STA`

Single-line edit. The firmware side of the real fix.

**Files:**
- Modify: `firmware/lib/wifi_provision/src/wifi_provision.cpp:105`

- [ ] **Step 1: Make the edit**

Change line 105:

```diff
-    WiFi.mode(WIFI_AP);
+    WiFi.mode(WIFI_AP_STA);
```

Full surrounding context (for reference):

```cpp
static void start_ap() {
    WiFi.mode(WIFI_AP_STA);          // <-- changed from WIFI_AP
    uint64_t mac = ESP.getEfuseMac();
```

- [ ] **Step 2: Build both envs**

Run:
```bash
cd firmware && pio run -e emory && pio run -e nora
```

Expected: both builds succeed. Flash size delta is zero or near-zero (same function, different constant).

- [ ] **Step 3: Run native tests**

Run:
```bash
cd firmware && pio test -e native
```

Expected: **172/172 tests pass**. (The native env does not compile `wifi_provision.cpp` — it's `#ifdef ARDUINO`-wrapped — but we run native tests to catch any incidental breakage.)

- [ ] **Step 4: Commit**

```bash
git add firmware/lib/wifi_provision/src/wifi_provision.cpp
git commit -m "fix(wifi_provision): use WIFI_AP_STA so /scan can find networks

WIFI_AP disables station-mode scanning, which broke the SSID dropdown on
the first-ever bench bring-up. WIFI_AP_STA lets the radio both broadcast
the captive AP and execute WiFi.scanNetworks()."
```

---

## Task 13: Raise `max_connection` from 1 to 4

Second single-line edit. Allows multi-device provisioning.

**Files:**
- Modify: `firmware/lib/wifi_provision/src/wifi_provision.cpp:111`

- [ ] **Step 1: Make the edit**

Change line 111:

```diff
-                /* ssid_hidden = */ 0, /* max_connection = */ 1);
+                /* ssid_hidden = */ 0, /* max_connection = */ 4);
```

- [ ] **Step 2: Build both envs**

Run:
```bash
cd firmware && pio run -e emory && pio run -e nora
```

Expected: both succeed, flash delta zero.

- [ ] **Step 3: Commit**

```bash
git add firmware/lib/wifi_provision/src/wifi_provision.cpp
git commit -m "fix(wifi_provision): allow up to 4 AP clients

max_connection=1 blocked the 'switch from phone to laptop if the first
device misbehaves' troubleshooting workflow. 4 covers real-world
multi-device provisioning (laptop + phone + 2 spares)."
```

---

## Task 14: Append hardware checks #8, #9, #10

Manual verification steps to run against a real ESP32 after flashing. Confirms the WIFI_AP_STA + max_connection fixes work on physical hardware (no automated test covers this).

**Files:**
- Modify: `firmware/test/hardware_checks/wifi_provision_checks.md`

- [ ] **Step 1: Append hardware checks**

Append to the end of `firmware/test/hardware_checks/wifi_provision_checks.md`:

```markdown

## 8. Multi-device connection (max_connection=4 regression)

- [ ] Flash the latest firmware: `cd firmware && pio run -e emory -t upload`
- [ ] Connect laptop to `WordClock-Setup-XXXX`.
- [ ] Without disconnecting laptop, connect phone to the same SSID.
- [ ] Expected: both devices connected. Form loads on both at `http://192.168.4.1`.

## 9. Scan dropdown populates (WIFI_AP_STA regression)

- [ ] Erase NVS + flash: `cd firmware && pio run -e emory -t erase && pio run -e emory -t upload`
- [ ] Connect laptop (or phone) to `WordClock-Setup-XXXX`.
- [ ] Open `http://192.168.4.1` in a browser.
- [ ] Expected: within 30 s, SSID dropdown shows nearby 2.4 GHz networks.
- [ ] Expected: dropdown stays populated — does NOT regress to empty / "No networks found"
      after displaying networks (a regression of the stop-on-success polling rule).

## 10. Timeout retry path (polling regression)

Run in a location where no 2.4 GHz networks are visible (rare in residential
settings), or simulate by powering down your home router temporarily.

- [ ] Connect a device to `WordClock-Setup-XXXX` and open the form.
- [ ] Wait 30 s.
- [ ] Expected: dropdown shows "No networks found — click Refresh" + a visible
      Refresh button.
- [ ] Click Refresh.
- [ ] Expected: polling restarts (visible as new /scan requests on the ESP32
      serial monitor if you're watching). If networks become available during
      retry, dropdown populates.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/wifi_provision_checks.md
git commit -m "docs: add hardware checks for multi-device + scan-poll paths"
```

---

## Task 15: Flash + run the new hardware checks

End-to-end verification on real hardware. Both firmware edits + the JS change together must produce a working captive portal provisioning experience.

- [ ] **Step 1: Erase NVS and flash Emory firmware**

Run:
```bash
cd firmware && pio run -e emory -t erase && pio run -e emory -t upload
```

Expected: upload succeeds. ESP32 boots into captive portal (no saved credentials).

- [ ] **Step 2: Walk through hardware check #9 (scan populates)**

Per the checklist added in Task 14 §9: connect a device, open `http://192.168.4.1`, verify dropdown populates within 30 s and stays populated.

If the dropdown stays stuck on "Scanning for networks…" or regresses to empty:
- Check serial monitor for scan-related errors (`pio device monitor`).
- Re-verify Tasks 11 and 12 were saved + flashed (git log should show the commits).

- [ ] **Step 3: Walk through hardware check #8 (multi-device)**

Per the checklist added in Task 14 §8: connect laptop, then phone to the same AP. Both must connect.

- [ ] **Step 4: Walk through hardware check #10 (timeout retry)**

Optional but recommended. Per §10: force an empty scan environment, confirm Refresh button appears + restarts polling.

- [ ] **Step 5: Complete the end-to-end captive-portal flow**

From the existing `wifi_provision_checks.md` §3–§7: pick your home SSID from the populated dropdown, submit form, press Audio button, verify the clock disconnects from the AP and connects to home WiFi.

Expected: full provisioning flow works.

- [ ] **Step 6: If anything fails, capture serial logs and stop**

If any hardware step fails, do NOT mark this task complete. Capture the serial output and file as a follow-up.

---

## Task 16: Update TODO.md + final doc sweep

Close the loop: mark the defect resolved in the live working list, and verify no dangling references to the old behavior remain in docs.

**Files:**
- Modify: `TODO.md` (status table if needed, add a "Shipped 2026-04-20" bullet under a suitable section)

- [ ] **Step 1: Check TODO.md for any tracking of this defect**

Run:
```bash
grep -n "captive portal\|wifi_provision\|scan" TODO.md
```

If a tracking bullet exists, mark it `[x]` with a note referencing this plan. If no tracking bullet exists, add one to the relevant section:

```markdown
- [x] **Captive portal scan fix (2026-04-20)** — WIFI_AP → WIFI_AP_STA,
      max_connection 1 → 4, /scan polling + 30 s timeout + Refresh button.
      Implements `docs/superpowers/specs/2026-04-20-captive-portal-scan-fix-design.md`.
```

- [ ] **Step 2: Run all pytest + native tests one final time**

Run:
```bash
cd firmware/assets/captive-portal && python -m pytest tests/ -v
cd firmware && pio test -e native
```

Expected: 17 Python tests pass + 172 native tests pass.

- [ ] **Step 3: Commit TODO update**

```bash
git add TODO.md
git commit -m "docs(todo): mark captive portal scan fix complete"
```

- [ ] **Step 4: Confirm full plan completion**

`git log --oneline -20` should show the commit chain:
1. `test: add Playwright dev deps...`
2. `test: parameterize preview mock...`
3. `test: add conftest fixtures...`
4. `test: add Tier 1 structural pytest... (red)`
5-9. Tier 2 test commits
10. `feat(captive-portal): poll /scan every 2s...`
11. `fix(wifi_provision): use WIFI_AP_STA...`
12. `fix(wifi_provision): allow up to 4 AP clients`
13. `docs: add hardware checks...`
14. `docs(todo): mark captive portal scan fix complete`

Clean, atomic, each commit green.

---

## Self-review checklist (executed during plan authoring — kept here as context for implementers)

- [x] **Spec coverage:** Every change in `2026-04-20-captive-portal-scan-fix-design.md` has a task (Change 1 → Task 12, Change 2 → Task 13, Change 3 → Task 11, Tier 1 → Task 4, Tier 2 → Tasks 5-9, hardware checks → Task 14).
- [x] **No placeholders:** Every code step has complete code. Every verification step has exact command + expected output.
- [x] **Type consistency:** `POLL_INTERVAL_MS`, `TIMEOUT_MS`, `renderSsidList`, `renderTimeoutRetry`, `pollScanOnce`, `startPolling` names used consistently across the JS and all Tier 1 tests that reference them.
- [x] **Red-green TDD cycles:** Tasks 4-9 write tests, Task 10 confirms red baseline, Task 11 flips to green. Tasks 12-13 covered by hardware checks in Task 15 (no automated red-green possible for hardware mode changes).
