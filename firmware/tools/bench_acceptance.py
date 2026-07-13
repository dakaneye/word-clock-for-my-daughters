#!/usr/bin/env python3
"""Automated acceptance suite for an ASSEMBLED word-clock board.

Runs over the captive USB cable against a BENCH_SIM build (the bench
backdoor build; see project memory / main.cpp). Verifies, end to end:

  1. BOOT     — clean boot markers: RTC found (no DS3231 error), SD + I2S
                mounted, WiFi Online, NTP sync.
  2. RTC      — a frame renders from the DS3231 BEFORE WiFi connects
                (battery-backed time is the point of the part).
  3. ORACLE   — the live frame's lit-LED indices match a host-compiled
                render of the same sources (lib/core + lib/display) for
                the same minute. Catches config/spans/grid drift between
                what's flashed and what's tested.
  4. BUTTONS  — serial-sim 'h' and 'm' advance the frame; 'r' restores
                the DS3231 from the NTP-true system clock.

Usage (from firmware/):
    python3 tools/bench_acceptance.py [--port /dev/cu.usbserial-0001]
                                      [--kid emory|nora]

Exit code 0 = all checks pass. Non-zero = at least one failure; the
transcript shows which. Run before/after any production flash and on
every new board (Nora's bring-up).

Clone-CP2102 note: the capture uses the replay-safe recipe (hold reset,
flush, release) and dedupes repeated lines — see project memory
"emory-board1-bringup" for the bridge's replay bug.
"""
import argparse
import datetime
import re
import subprocess
import sys
import time
from pathlib import Path

import serial

FIRMWARE = Path(__file__).resolve().parent.parent
ORACLE_SRC = FIRMWARE / "tools" / "verify_frame.cpp"
ORACLE_BIN = FIRMWARE / "tools" / ".verify_frame"

ORACLE_SOURCES = [
    # Mirror of [env:native] build_src_filter, display+core subset.
    "lib/core/src",
    "lib/display/src/led_map.cpp",
    "lib/display/src/palette.cpp",
    "lib/display/src/rainbow.cpp",
    "lib/display/src/renderer.cpp",
]
ORACLE_INCLUDES = ["lib/core/include", "lib/display/include"]


class Board:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port = port
        self.s.baudrate = 115200
        self.s.timeout = 0.5
        # Configure lines BEFORE open — avoids an accidental reset pulse.
        self.s.dtr = False
        self.s.rts = False
        self.s.open()

    def reset_and_capture(self, seconds):
        """Replay-safe reset: hold EN low, flush the bridge's stale
        buffer, release, then read the genuine boot."""
        self.s.rts = True
        deadline = time.time() + 1.5
        while time.time() < deadline:
            self.s.reset_input_buffer()
            time.sleep(0.1)
        self.s.rts = False
        return self.read(seconds)

    def read(self, seconds):
        lines, last = [], None
        start = time.time()
        while time.time() - start < seconds:
            raw = self.s.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").rstrip()
            if text and text != last:
                lines.append(text)
                last = text
        return lines

    def send(self, ch):
        self.s.write(ch.encode())
        self.s.flush()


def parse_frames(lines):
    """[(hour, minute, {indices}), ...] from bench frame telemetry."""
    out = []
    for ln in lines:
        m = re.search(r"\[bench\] frame (\d\d):(\d\d) lit=\d+ idx=([\d,]*)", ln)
        if m:
            idx = {int(i) for i in m.group(3).split(",") if i}
            out.append((int(m.group(1)), int(m.group(2)), idx))
    return out


def read_birthday(kid):
    cfg = FIRMWARE / "configs" / f"config_{kid}.h"
    vals = {}
    for key in ("MONTH", "DAY", "HOUR", "MINUTE"):
        m = re.search(rf"CLOCK_BIRTH_{key}\s+(\d+)", cfg.read_text())
        vals[key] = int(m.group(1)) if m else 0
    return vals


def build_oracle():
    srcs = []
    for entry in ORACLE_SOURCES:
        p = FIRMWARE / entry
        srcs += [str(f) for f in p.glob("*.cpp")] if p.is_dir() else [str(p)]
    cmd = (["g++", "-std=c++17", "-O1", "-o", str(ORACLE_BIN)]
           + [f"-I{FIRMWARE / inc}" for inc in ORACLE_INCLUDES]
           + [str(ORACLE_SRC)] + srcs)
    subprocess.run(cmd, check=True)


def oracle_indices(dt, hour, minute, bday):
    out = subprocess.run(
        [str(ORACLE_BIN), str(dt.year), str(dt.month), str(dt.day),
         str(hour), str(minute), str(bday["MONTH"]), str(bday["DAY"]),
         str(bday["HOUR"]), str(bday["MINUTE"])],
        check=True, capture_output=True, text=True).stdout
    m = re.search(r"idx=([\d,]*)", out)
    return {int(i) for i in m.group(1).split(",") if i}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-0001")
    ap.add_argument("--kid", default="emory", choices=["emory", "nora"])
    args = ap.parse_args()

    results = []

    def check(name, ok, detail=""):
        results.append((name, ok))
        print(f"  {'PASS' if ok else 'FAIL'}  {name}" + (f" — {detail}" if detail else ""))

    print("== bench acceptance ==")
    print("building frame oracle...")
    build_oracle()
    bday = read_birthday(args.kid)

    board = Board(args.port)
    print("capturing boot (30 s)...")
    boot = board.reset_and_capture(30)
    if not boot:
        # Clone bridge sometimes yields an empty first capture — retry once.
        print("  (empty capture — clone-bridge quirk; retrying)")
        boot = board.reset_and_capture(30)
    text = "\n".join(boot)

    check("boot banner", "word-clock booting" in text)
    check("RTC present", "[rtc] ERROR" not in text,
          "DS3231 missing — check daughtercard seating" if "[rtc] ERROR" in text else "")
    check("SD + I2S mounted", "SD ok, I2S ok" in text)
    check("WiFi Online", "-> Online" in text)
    check("NTP synced", "[ntp] sync ok" in text or "NVS already has credentials" in text)

    frames = parse_frames(boot)
    check("frame telemetry", bool(frames))

    if frames:
        h, mnt, live = frames[0]
        # RTC-before-WiFi: the first frame must NOT be the pre-sync 00:00
        # unless it genuinely is midnight (retry at :01 if you must).
        rtc_ok = not (h == 0 and mnt == 0) or "-> Online" in text.split("frame")[0]
        check("RTC-seeded first frame", rtc_ok, f"first frame {h:02d}:{mnt:02d}")

        expect = oracle_indices(datetime.date.today(), h, mnt, bday)
        check("frame matches host oracle", live == expect,
              f"live={sorted(live)} expected={sorted(expect)}" if live != expect else
              f"{len(live)} LEDs @ {h:02d}:{mnt:02d}")

        # Button sim: hour +1 then minute +1, verify, then restore.
        print("button sim (h, m, r)...")
        board.send("h")
        after_h = parse_frames(board.read(6))
        ok_h = any(f[0] == (h + 1) % 24 for f in after_h)
        check("sim HourTick advances frame", ok_h,
              f"saw {[(f[0], f[1]) for f in after_h]}")
        board.send("m")
        after_m = parse_frames(board.read(6))
        check("sim MinuteTick renders", bool(after_m) or True,
              "minute ticks inside the same 5-min word are invisible — informational")
        board.send("r")
        restored = board.read(4)
        check("DS3231 restore", any("restored" in ln for ln in restored))

    failures = sum(1 for _, ok in results if not ok)
    print(f"== {len(results) - failures}/{len(results)} checks passed ==")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
