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
    # Spot-check option text contents.
    expect(dropdown.locator("option")).to_contain_text(
        ["HomeWiFi-5G", "HomeWiFi-2.4", "Guest (open)", "Neighbor"]
    )


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
