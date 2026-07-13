#!/usr/bin/env python3
"""Load/update the clock's SD audio over the captive USB cable.

Stages (each a hard gate, all narrated):
  1 stage    convert inputs to spec WAVs (afconvert), validate, CRC
  2 backup   esptool read_flash of the ENTIRE flash to tools/.flash_backups/
  3 load     flash the loader firmware, serve staged files over HTTP,
             drive the serial protocol until every CRC matches
  4 restore  esptool write_flash the backup back, verify boot markers

If anything fails after the loader is flashed, the tool attempts the
restore automatically and, failing that, prints the exact
--restore-only recovery command. Design:
docs/superpowers/specs/2026-07-13-sd-loader-design.md
"""
import argparse
import functools
import http.server
import math
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import wave
from datetime import datetime, timezone
from pathlib import Path

import serial

import sd_load_core as core

TOOLS = Path(__file__).resolve().parent
FIRMWARE = TOOLS.parent
LOADER = TOOLS / "sd_loader"
BACKUPS = TOOLS / ".flash_backups"
PIO = Path.home() / "Library/Python/3.9/bin/pio"
ESPTOOL = Path.home() / ".platformio/packages/tool-esptoolpy/esptool.py"
BAUDS = (921600, 460800, 115200)
PORT_DEFAULT = "/dev/cu.usbserial-0001"


def say(msg):
    print(f"== {msg}", flush=True)


def run(cmd, **kw):
    print(f"   $ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def esptool(port, args, capture=False):
    """Run esptool with baud fallback (clone CP2102 flakes at high baud)."""
    last = None
    for baud in BAUDS:
        cmd = ["python3", ESPTOOL, "--port", port, "--baud", str(baud)] + args
        try:
            return run(cmd, capture_output=capture, text=True)
        except subprocess.CalledProcessError as e:
            last = e
            say(f"esptool failed at {baud} baud; retrying slower")
    raise last


def detect_flash_size(port) -> int:
    out = esptool(port, ["flash_id"], capture=True).stdout
    m = re.search(r"Detected flash size: (\d+)MB", out)
    if not m:
        raise RuntimeError(f"could not detect flash size:\n{out}")
    return int(m.group(1)) * 1024 * 1024


def lan_ip() -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))          # no packets sent; picks the LAN iface
    ip = s.getsockname()[0]
    s.close()
    return ip


class Clock:
    """Serial session against the loader (or clock) with the replay-safe
    reset recipe — the clone CP2102 re-delivers stale buffers."""

    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port = port
        self.s.baudrate = 115200
        self.s.timeout = 1.0
        self.s.dtr = False
        self.s.rts = False
        self.s.open()

    def reset(self):
        self.s.rts = True
        deadline = time.time() + 1.5
        while time.time() < deadline:
            self.s.reset_input_buffer()
            time.sleep(0.1)
        self.s.rts = False

    def lines(self, seconds):
        start, last = time.time(), None
        while time.time() - start < seconds:
            raw = self.s.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").strip()
            if text and text != last:
                last = text
                yield text

    def command(self, line, timeout=120):
        """Send one command; stream PROG lines; return the OK/ERR parse."""
        self.s.write(line.encode() if line.endswith("\n")
                     else (line + "\n").encode())
        self.s.flush()
        for text in self.lines(timeout):
            p = core.parse_line(text)
            if p["kind"] == "prog":
                print(f"   ... {p['name']} {p['pct']}%", flush=True)
            elif p["kind"] in ("ok", "err"):
                return p
            elif p["kind"] == "file":
                print(f"   {p['name']:<16} {p['size']:>9}  crc {p['crc']:08x}",
                      flush=True)
        return {"kind": "err", "detail": f"timeout waiting for reply to {line!r}"}

    def close(self):
        self.s.close()


def make_test_wav(path: Path):
    """100 KB 440 Hz test tone, spec format, for --dry-run."""
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(44100)
        n = 50_000
        frames = b"".join(
            struct.pack("<h", int(12000 * math.sin(
                2 * math.pi * 440 * i / 44100))) for i in range(n))
        w.writeframes(frames)


def stage(args, staging: Path) -> dict:
    say("stage: converting + validating inputs")
    plan = {}
    if args.dry_run:
        p = staging / "dryrun.tmp"
        make_test_wav(p)
        plan["dryrun.tmp"] = p
    else:
        for slot, src in core.resolve_inputs(args).items():
            dst = staging / slot
            if src.suffix.lower() == ".wav":
                shutil.copy(src, dst)
            else:
                run(core.afconvert_cmd(src, dst))
            core.validate_wav(dst)
            plan[slot] = dst
    for name, p in plan.items():
        print(f"   {name:<16} {p.stat().st_size:>9} bytes  "
              f"crc {core.file_crc32(p):08x}")
    return plan


def transfer(clock: Clock, plan: dict, ip: str, port: int):
    for name, path in plan.items():
        size, crc = path.stat().st_size, core.file_crc32(path)
        url = f"http://{ip}:{port}/{path.name}"
        say(f"load: {name} ({size} bytes)")
        for attempt in range(3):
            reply = clock.command(core.format_get(url, name, size, crc))
            if reply["kind"] == "ok":
                break
            say(f"retry {attempt + 1}/3: {reply['detail']}")
        else:
            raise RuntimeError(f"transfer failed for {name}")
    if "dryrun.tmp" in plan:
        say("dry run: deleting test file")
        reply = clock.command("D dryrun.tmp", timeout=30)
        if reply["kind"] != "ok":
            raise RuntimeError(f"dry-run cleanup failed: {reply['detail']}")
    say("card contents after load:")
    clock.command("L", timeout=300)


def boot_check(port) -> bool:
    say("boot check: capturing restored firmware boot")
    clock = Clock(port)
    try:
        clock.reset()
        seen = set()
        for text in clock.lines(30):
            print(f"   | {text}")
            if "word-clock booting" in text:
                seen.add("banner")
            if "[boot] audio::begin done" in text:
                seen.add("setup")
            if "[bench] frame" in text or "-> Online" in text:
                seen.add("alive")
                break
        return {"banner", "setup"} <= seen
    finally:
        clock.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("directory", nargs="?", help="dir with lullaby1.*/lullaby2.*/birth.*")
    ap.add_argument("--lullaby1")
    ap.add_argument("--lullaby2")
    ap.add_argument("--birth")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--restore-only", metavar="BACKUP_BIN")
    ap.add_argument("--port", default=PORT_DEFAULT)
    ap.add_argument("--wifi-ssid", help="explicit WiFi for an unprovisioned "
                    "board (sent to the loader as a W command)")
    ap.add_argument("--wifi-pass", default="")
    args = ap.parse_args()

    BACKUPS.mkdir(exist_ok=True)

    if args.restore_only:
        say(f"restore-only from {args.restore_only}")
        esptool(args.port, ["write_flash", "0x0", args.restore_only])
        sys.exit(0 if boot_check(args.port) else 1)

    staging = Path(tempfile.mkdtemp(prefix="sdload-"))
    plan = stage(args, staging)

    say("backup: reading full flash image")
    size = detect_flash_size(args.port)
    backup = BACKUPS / core.backup_name(datetime.now(timezone.utc))
    esptool(args.port, ["read_flash", "0", hex(size), str(backup)])
    say(f"backup saved: {backup} ({backup.stat().st_size} bytes)")

    loader_flashed = False
    try:
        say("load: flashing loader firmware")
        run([PIO, "run", "-d", LOADER, "-t", "upload",
             "--upload-port", args.port])
        loader_flashed = True

        handler = functools.partial(
            http.server.SimpleHTTPRequestHandler, directory=str(staging))
        httpd = http.server.ThreadingHTTPServer(("0.0.0.0", 0), handler)
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        ip, http_port = lan_ip(), httpd.server_address[1]
        say(f"serving staged files at http://{ip}:{http_port}/")

        clock = Clock(args.port)
        try:
            clock.reset()
            ready = None
            for text in clock.lines(25):
                p = core.parse_line(text)
                if p["kind"] == "ready":
                    ready = p
                    break
            if not ready:
                raise RuntimeError("loader never printed READY")
            if ready["sd"] != "ok":
                raise RuntimeError("loader could not mount the SD card")
            if not re.match(r"\d+\.\d+\.\d+\.\d+", ready["wifi"]):
                if not args.wifi_ssid:
                    raise RuntimeError(
                        "loader has no WiFi (NVS empty?) — pass "
                        "--wifi-ssid/--wifi-pass for an unprovisioned board")
                say(f"wifi: joining {args.wifi_ssid} via W command")
                reply = clock.command(
                    f"W {args.wifi_ssid}\t{args.wifi_pass}", timeout=30)
                if reply["kind"] != "ok":
                    raise RuntimeError(f"loader WiFi join failed: "
                                       f"{reply['detail']}")
            transfer(clock, plan, ip, http_port)
        finally:
            clock.close()
            httpd.shutdown()
    finally:
        if loader_flashed:
            say("restore: writing original flash image back")
            try:
                esptool(args.port, ["write_flash", "0x0", str(backup)])
            except Exception:
                say("RESTORE FAILED — recover with:")
                say(f"  python3 tools/sd_load.py --restore-only {backup} "
                    f"--port {args.port}")
                raise

    ok = boot_check(args.port)
    say("SUCCESS — clock restored and healthy" if ok else
        f"restore wrote OK but boot check failed; backup at {backup}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
