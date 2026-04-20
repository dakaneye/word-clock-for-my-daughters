#!/usr/bin/env python3
"""Capture serial output from the ESP32 and print it live.

Usage:
    # Capture for 30 seconds (default) from auto-detected port:
    ./serial_capture.py

    # Capture for N seconds:
    ./serial_capture.py --seconds 90

    # Reset the ESP32 via RTS pulse before capturing:
    ./serial_capture.py --reset

    # Capture from a specific port:
    ./serial_capture.py --port /dev/tty.usbserial-0001

    # Show raw output (no NVS noise filtering):
    ./serial_capture.py --raw

Designed to run without a venv — relies on pyserial being installed via
platformio's bundled install at ~/Library/Python/3.9/lib/python/site-packages
(macOS default).  If pyserial isn't found there, falls back to the system
python path. If still not found, errors clearly.
"""
from __future__ import annotations

import argparse
import glob
import sys
import time

# PlatformIO's bundled pyserial is often the only copy installed on dev
# machines. Try that first; fall back to the active interpreter's sys.path.
for cand in [
    "/Users/samueldacanay/Library/Python/3.9/lib/python/site-packages",
    "/Users/samueldacanay/Library/Python/3.11/lib/python/site-packages",
]:
    if cand not in sys.path:
        sys.path.insert(0, cand)

try:
    import serial  # noqa: E402
except ImportError:
    sys.stderr.write(
        "serial_capture.py: pyserial not found.\n"
        "Install via: pip install pyserial --break-system-packages\n"
        "or activate the platformio venv / captive-portal tests venv.\n"
    )
    sys.exit(2)


def autodetect_port() -> str | None:
    """Return the most likely ESP32 serial device, or None."""
    for pattern in ("/dev/tty.usbserial-*", "/dev/tty.SLAB_*", "/dev/tty.wchusbserial*"):
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[0]
    return None


def should_drop(line: str, raw: bool) -> bool:
    """Filter out well-known noise unless --raw is passed."""
    if raw:
        return False
    if not line:
        return True
    # Count printable chars; ROM-bootloader garbage comes through at
    # 74880 baud and looks like random bytes.
    printable = sum(1 for c in line if c.isprintable())
    if printable < len(line) * 0.7:
        return True
    # Preferences.cpp's "nvs_open failed: NOT_FOUND" is normal first-boot
    # spam from modules whose NVS namespace hasn't been created yet.
    if "nvs_open failed" in line:
        return True
    if line.startswith("NOT_FOUND") or line == "NOT_FOUND":
        return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description="Live-stream ESP32 serial.")
    ap.add_argument("--port", default=None,
                    help="Serial device path (auto-detects /dev/tty.usbserial-* if omitted).")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=int, default=30,
                    help="How long to capture before exiting (default: 30).")
    ap.add_argument("--reset", action="store_true",
                    help="Pulse RTS to reset the ESP32 before capturing.")
    ap.add_argument("--raw", action="store_true",
                    help="Don't filter NVS noise or bootloader garbage.")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    if port is None:
        sys.stderr.write("serial_capture.py: no /dev/tty.usbserial-* found.\n")
        return 2

    try:
        s = serial.Serial(port, args.baud, timeout=0.2)
    except Exception as e:
        sys.stderr.write(f"serial_capture.py: failed to open {port}: {e}\n")
        return 2

    try:
        if args.reset:
            s.setDTR(False)
            s.setRTS(True)
            time.sleep(0.2)
            s.setRTS(False)
            time.sleep(0.5)
        else:
            # Drain any buffered output so the capture starts fresh.
            s.reset_input_buffer()
            time.sleep(0.2)
            s.reset_input_buffer()

        sys.stdout.write(f"--- capturing {args.seconds}s from {port} @ {args.baud} ---\n")
        sys.stdout.flush()

        start = time.time()
        buffer = b""
        while time.time() - start < args.seconds:
            chunk = s.read(4096)
            if not chunk:
                continue
            buffer += chunk
            # Print complete lines as they arrive.
            while b"\n" in buffer:
                line_b, buffer = buffer.split(b"\n", 1)
                line = line_b.decode("utf-8", errors="replace").rstrip("\r")
                if should_drop(line, args.raw):
                    continue
                sys.stdout.write(line + "\n")
                sys.stdout.flush()

        # Flush any remainder.
        if buffer:
            line = buffer.decode("utf-8", errors="replace").rstrip()
            if not should_drop(line, args.raw):
                sys.stdout.write(line + "\n")
        sys.stdout.write(f"--- capture done ({args.seconds}s) ---\n")
    finally:
        s.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
