#!/usr/bin/env python3
"""Load/update the clock's SD audio through the captive USB cable.

Stages (each a hard gate, all narrated):
  1 stage    convert inputs to spec WAVs (afconvert), validate, CRC
  2 backup   esptool read_flash of the ENTIRE flash to tools/.flash_backups/,
             then verify the image (size + bootloader magic) before trusting it
  3 load     flash the loader firmware, serve staged files over the home WiFi
             (HTTP; the cable carries flashing + serial control only),
             drive the serial protocol until every CRC matches
  4 restore  esptool write_flash the backup back (esptool 4.x verifies
             written data itself), then check boot markers

If anything fails once flashing has begun — including Ctrl-C — the tool
attempts the restore automatically and, failing that, prints the exact
--restore-only recovery command. Design:
docs/superpowers/specs/2026-07-13-sd-loader-design.md
"""
import argparse
import fcntl
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
import wave
from datetime import datetime, timezone
from pathlib import Path

import sd_load_core as core
from serial_bridge import SerialBridge

TOOLS = Path(__file__).resolve().parent
LOADER = TOOLS / "sd_loader"
BACKUPS = TOOLS / ".flash_backups"
PIO = Path.home() / "Library/Python/3.9/bin/pio"
ESPTOOL = Path.home() / ".platformio/packages/tool-esptoolpy/esptool.py"
BAUDS = (921600, 460800, 115200)
PORT_DEFAULT = "/dev/cu.usbserial-0001"

_IP_RE = re.compile(r"\d+\.\d+\.\d+\.\d+")


def say(msg: str) -> None:
    print(f"== {msg}", flush=True)


def run(cmd, **kw) -> subprocess.CompletedProcess:
    print(f"   $ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def esptool(port: str, args: list, capture: bool = False):
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


def acquire_port_lock(port: str):
    """One sd_load per serial port. Two concurrent runs would interleave
    backup/flash/restore and could restore the wrong image over a
    correctly-restored clock. Returns the held file handle — keep it
    referenced for the process lifetime."""
    BACKUPS.mkdir(exist_ok=True)
    lock_path = BACKUPS / (re.sub(r"\W", "_", port) + ".lock")
    fh = lock_path.open("w")
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit(
            f"another sd_load.py already holds {port} — wait for it")
    return fh


def detect_flash_size(port: str) -> int:
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


class Clock(SerialBridge):
    """SerialBridge plus the loader's command/reply protocol."""

    def command(self, line: str, timeout: float = 120) -> dict:
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


def make_test_wav(path: Path) -> None:
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
            core.canonicalize_wav(dst)
            gain = core.normalize_wav_peak(dst)
            if gain != 1.0:
                say(f"stage: normalized {slot} (+{20 * math.log10(gain):.1f} dB)")
            core.validate_wav(dst)
            plan[slot] = dst
    for name, p in plan.items():
        print(f"   {name:<16} {p.stat().st_size:>9} bytes  "
              f"crc {core.file_crc32(p):08x}")
    return plan


def transfer(clock: Clock, plan: dict, ip: str, port: int) -> None:
    for name, path in plan.items():
        size, crc = path.stat().st_size, core.file_crc32(path)
        url = f"http://{ip}:{port}/{path.name}"
        say(f"load: {name} ({size} bytes)")
        for attempt in range(3):
            reply = clock.command(core.format_get(url, name, size, crc))
            if reply["kind"] == "ok":
                if core.got_matches(reply, name, size, crc):
                    break
                say(f"retry {attempt + 1}/3: stale/mismatched OK reply "
                    f"({reply['detail']!r}) for {name}")
                continue
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


def boot_check(port: str) -> bool:
    """PASS = boot banner + setup-complete seen and no RTC error within
    30 s. A rendered frame / WiFi-Online transition ends the capture
    early when it appears but is NOT required — a production build on
    slow WiFi may take longer than the window to print either."""
    say("boot check: capturing restored firmware boot")
    with Clock(port) as clock:
        clock.reset()
        seen = set()
        for text in clock.lines(30):
            print(f"   | {text}")
            if "word-clock booting" in text:
                seen.add("banner")
            if "[boot] audio::begin done" in text:
                seen.add("setup")
            if "[rtc] ERROR" in text:
                seen.add("rtc_error")
            if "[bench] frame" in text or "-> Online" in text:
                break
        return {"banner", "setup"} <= seen and "rtc_error" not in seen


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("directory", nargs="?",
                    help="dir with lullaby1.*/lullaby2.*/birth.*")
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

    port_lock = acquire_port_lock(args.port)   # held until process exit

    if args.restore_only:
        say(f"restore-only from {args.restore_only}")
        try:
            core.validate_backup_image(Path(args.restore_only), None)
            esptool(args.port, ["write_flash", "0x0", args.restore_only])
            ok = boot_check(args.port)
        except Exception as e:
            say(f"restore-only failed: {e}")
            sys.exit(2)
        sys.exit(0 if ok else 1)

    with tempfile.TemporaryDirectory(prefix="sdload-") as staging_dir:
        staging = Path(staging_dir)
        plan = stage(args, staging)

        say("backup: reading full flash image")
        size = detect_flash_size(args.port)
        backup = BACKUPS / core.backup_name(datetime.now(timezone.utc))
        esptool(args.port, ["read_flash", "0", hex(size), str(backup)])
        core.validate_backup_image(backup, size)
        say(f"backup saved + verified: {backup} ({size} bytes)")

        loader_flashed = False
        failure = None
        try:
            say("load: flashing loader firmware")
            # Mark the flash dirty BEFORE the upload starts: a failed or
            # interrupted upload can leave the chip partially written, so
            # every exception from here on must route through the restore.
            loader_flashed = True
            run([PIO, "run", "-d", LOADER, "-t", "upload",
                 "--upload-port", args.port])

            ip = lan_ip()
            handler = functools.partial(
                http.server.SimpleHTTPRequestHandler, directory=str(staging))
            httpd = http.server.ThreadingHTTPServer((ip, 0), handler)
            threading.Thread(target=httpd.serve_forever, daemon=True).start()
            try:
                http_port = httpd.server_address[1]
                say(f"serving staged files at http://{ip}:{http_port}/")

                with Clock(args.port) as clock:
                    clock.reset()
                    ready = None
                    for text in clock.lines(35):
                        p = core.parse_line(text)
                        if p["kind"] == "ready":
                            ready = p
                            break
                    if not ready:
                        raise RuntimeError("loader never printed READY")
                    if ready["sd"] != "ok":
                        raise RuntimeError("loader could not mount the SD card")
                    if not _IP_RE.match(ready["wifi"]):
                        if not args.wifi_ssid:
                            why = ("stored WiFi credentials failed to join "
                                   "(router down? clock moved?)"
                                   if ready["wifi"] == "failed"
                                   else "no WiFi credentials in NVS")
                            raise RuntimeError(
                                f"loader has no WiFi: {why} — pass "
                                "--wifi-ssid/--wifi-pass to supply a network")
                        say(f"wifi: joining {args.wifi_ssid} via W command")
                        reply = clock.command(
                            f"W {args.wifi_ssid}\t{args.wifi_pass}", timeout=30)
                        if reply["kind"] != "ok":
                            raise RuntimeError(f"loader WiFi join failed: "
                                               f"{reply['detail']}")
                    transfer(clock, plan, ip, http_port)
            finally:
                httpd.shutdown()
        except BaseException as e:
            if not loader_flashed:
                raise
            if isinstance(e, KeyboardInterrupt):
                say("load: interrupted — restoring original flash before exiting")
            failure = e

        say("restore: writing original flash image back")
        try:
            esptool(args.port, ["write_flash", "0x0", str(backup)])
        except Exception as restore_exc:
            if failure is not None:
                say(f"load failed: {failure}")
            say(f"RESTORE FAILED: {restore_exc}")
            say("recover with:")
            say(f"  python3 tools/sd_load.py --restore-only {backup} "
                f"--port {args.port}")
            sys.exit(2)

    ok = boot_check(args.port)
    if failure is not None:
        say(f"load failed: {failure}")
        say("restore: OK, boot check passed" if ok else
            "restore: OK, boot check FAILED")
        say(f"backup at {backup}")
        sys.exit(1)

    say("SUCCESS — clock restored and healthy" if ok else
        f"restore wrote OK but boot check failed; backup at {backup}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
