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
