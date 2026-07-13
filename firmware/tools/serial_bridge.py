"""Replay-safe serial session for the clock's clone CP2102 bridge.

The AITRIP module's USB-serial bridge re-delivers stale buffer contents
endlessly (project memory: 6.4 MB of phantom text was once received while
the ESP32 was held in reset). Every tool that talks to the board needs the
same three defenses, so they live here once:

  1. Open with DTR/RTS deasserted BEFORE the port opens — pyserial's
     defaults would pulse the auto-reset circuit.
  2. Reset by holding EN low (RTS asserted) while draining the bridge's
     replay buffer, then release — a fresh boot read this way is trustworthy.
  3. Suppress consecutive duplicate lines on read — surviving replays
     repeat verbatim.

Consumers: sd_load.py (Clock) and bench_acceptance.py (Board). The hold
and poll constants were tuned on the bench (2026-07-04); change them here
and both tools follow.
"""
import time

import serial

RESET_HOLD_S = 1.5
RESET_POLL_S = 0.1


class SerialBridge:
    """Context manager owning one 115200-baud session against the clock."""

    def __init__(self, port, timeout=1.0):
        self.s = serial.Serial()
        self.s.port = port
        self.s.baudrate = 115200
        self.s.timeout = timeout
        self.s.dtr = False          # configure lines BEFORE open —
        self.s.rts = False          # no accidental reset pulse
        self.s.open()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def reset(self):
        """Hold the chip in reset while flushing replayed garbage, then
        release so the next reads are the genuine boot."""
        self.s.rts = True           # EN low
        deadline = time.time() + RESET_HOLD_S
        while time.time() < deadline:
            self.s.reset_input_buffer()
            time.sleep(RESET_POLL_S)
        self.s.rts = False

    def lines(self, seconds):
        """Yield decoded lines for up to `seconds`, suppressing
        consecutive duplicates (the replay signature)."""
        start, last = time.time(), None
        while time.time() - start < seconds:
            raw = self.s.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").strip()
            if text and text != last:
                last = text
                yield text

    def close(self):
        self.s.close()
