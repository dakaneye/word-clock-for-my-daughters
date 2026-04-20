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
