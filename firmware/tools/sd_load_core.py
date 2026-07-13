"""Pure logic for sd_load.py — importable without pyserial/esptool.

WAV validation, CRC, input resolution, and the serial protocol's line
formats. The wire protocol is defined in
docs/superpowers/specs/2026-07-13-sd-loader-design.md.
"""
import re
import struct
import zlib
from datetime import datetime
from pathlib import Path

SLOTS = ("lullaby1.wav", "lullaby2.wav", "birth.wav")


def validate_wav(path: Path) -> int:
    """Return the data-chunk size of a spec-compliant WAV; raise
    ValueError naming the first violation. Spec: RIFF/WAVE, PCM,
    mono, 44100 Hz, 16-bit, non-empty data."""
    raw = Path(path).read_bytes()
    if len(raw) < 44 or raw[0:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(f"{path.name}: not a RIFF/WAVE file")
    pos, fmt_seen, data_size = 12, False, None
    while pos + 8 <= len(raw):
        cid, csize = raw[pos:pos + 4], struct.unpack_from("<I", raw, pos + 4)[0]
        body = raw[pos + 8:pos + 8 + csize]
        if cid == b"fmt ":
            fmt_seen = True
            fmt, ch, rate, _, _, bits = struct.unpack_from("<HHIIHH", body)
            if fmt != 1:
                raise ValueError(f"{path.name}: not PCM (format {fmt})")
            if ch != 1:
                raise ValueError(f"{path.name}: {ch} channels — must be mono")
            if rate != 44100:
                raise ValueError(f"{path.name}: {rate} Hz — must be 44100")
            if bits != 16:
                raise ValueError(f"{path.name}: {bits}-bit — must be 16-bit")
        elif cid == b"data":
            data_size = csize
        pos += 8 + csize + (csize & 1)   # chunks are word-aligned
    if not fmt_seen:
        raise ValueError(f"{path.name}: missing fmt chunk")
    if not data_size:
        raise ValueError(f"{path.name}: empty data chunk")
    return data_size


def file_crc32(path: Path) -> int:
    crc = 0
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            crc = zlib.crc32(chunk, crc)
    return crc


def afconvert_cmd(src: Path, dst: Path) -> list:
    return ["afconvert", "-f", "WAVE", "-d", "LEI16@44100", "-c", "1",
            str(src), str(dst)]


def resolve_inputs(args) -> dict:
    """Map canonical slot names to source paths from --lullaby1/2/--birth
    flags or a directory of files named <slot-stem>.*  (≥1 required)."""
    out = {}
    for slot in SLOTS:
        stem = slot.split(".")[0]           # lullaby1 / lullaby2 / birth
        flag = getattr(args, stem, None)
        if flag:
            out[slot] = Path(flag)
    if not out and getattr(args, "directory", None):
        d = Path(args.directory)
        for slot in SLOTS:
            stem = slot.split(".")[0]
            hits = sorted(d.glob(f"{stem}.*"))
            if hits:
                out[slot] = hits[0]
    if not out:
        raise ValueError(
            "no inputs: pass --lullaby1/--lullaby2/--birth or a directory "
            f"containing files named {', '.join(s.split('.')[0] + '.*' for s in SLOTS)}")
    return out


def format_get(url: str, name: str, size: int, crc: int) -> str:
    return f"G {url}\t{name}\t{size}\t{crc:08x}\n"


_READY = re.compile(r"^READY sd=(\w+) wifi=(\S+)$")
_FILE = re.compile(r"^FILE (\S+) (\d+) ([0-9a-fA-F]{8})$")
_PROG = re.compile(r"^PROG (\S+) (\d+)$")
_OK_GOT = re.compile(r"^OK got (\S+) (\d+) ([0-9a-fA-F]{8})$")


def parse_line(line: str) -> dict:
    line = line.strip()
    if m := _READY.match(line):
        return {"kind": "ready", "sd": m.group(1), "wifi": m.group(2)}
    if m := _OK_GOT.match(line):
        return {"kind": "ok", "detail": line[3:],
                "got": {"name": m.group(1), "size": int(m.group(2)),
                        "crc": int(m.group(3), 16)}}
    if line.startswith("OK "):
        return {"kind": "ok", "detail": line[3:]}
    if line.startswith("ERR "):
        return {"kind": "err", "detail": line[4:]}
    if m := _FILE.match(line):
        return {"kind": "file", "name": m.group(1),
                "size": int(m.group(2)), "crc": int(m.group(3), 16)}
    if m := _PROG.match(line):
        return {"kind": "prog", "name": m.group(1), "pct": int(m.group(2))}
    return {"kind": "noise", "detail": line}


def backup_name(now: datetime) -> str:
    return now.strftime("flash-%Y%m%d-%H%M%S.bin")
