import struct
import zlib
from datetime import datetime
from pathlib import Path

import pytest

import sd_load_core as core


def make_wav(path: Path, *, channels=1, rate=44100, bits=16,
             fmt=1, data=b"\x00\x01" * 100, magic=b"RIFF", wave=b"WAVE"):
    """Minimal RIFF/WAVE writer for tests."""
    fmt_chunk = struct.pack("<4sIHHIIHH", b"fmt ", 16, fmt, channels,
                            rate, rate * channels * bits // 8,
                            channels * bits // 8, bits)
    data_chunk = struct.pack("<4sI", b"data", len(data)) + data
    body = wave + fmt_chunk + data_chunk
    path.write_bytes(magic + struct.pack("<I", len(body)) + body)
    return path


def test_validate_wav_accepts_spec_file(tmp_path):
    p = make_wav(tmp_path / "ok.wav")
    assert core.validate_wav(p) == 200


@pytest.mark.parametrize("kwargs,needle", [
    ({"channels": 2}, "mono"),
    ({"rate": 48000}, "44100"),
    ({"bits": 24}, "16-bit"),
    ({"fmt": 3}, "PCM"),
    ({"magic": b"RIFX"}, "RIFF"),
    ({"data": b""}, "empty"),
])
def test_validate_wav_rejects_violations(tmp_path, kwargs, needle):
    p = make_wav(tmp_path / "bad.wav", **kwargs)
    with pytest.raises(ValueError, match=needle):
        core.validate_wav(p)


def test_file_crc32_matches_zlib(tmp_path):
    p = tmp_path / "f.bin"
    p.write_bytes(b"hello sd loader")
    assert core.file_crc32(p) == zlib.crc32(b"hello sd loader")


def test_afconvert_cmd_shape(tmp_path):
    cmd = core.afconvert_cmd(Path("in.m4a"), Path("out.wav"))
    assert cmd[0] == "afconvert"
    assert "-d" in cmd and "LEI16@44100" in cmd
    assert "-c" in cmd and "1" in cmd
    assert cmd[-2:] == ["in.m4a", "out.wav"]


def test_resolve_inputs_from_flags(tmp_path):
    a = make_wav(tmp_path / "a.wav")
    got = core.resolve_inputs(
        type("A", (), {"lullaby1": a, "lullaby2": None, "birth": None,
                       "directory": None})())
    assert got == {"lullaby1.wav": a}


def test_resolve_inputs_from_directory(tmp_path):
    l1 = make_wav(tmp_path / "lullaby1.m4a")
    b = make_wav(tmp_path / "birth.wav")
    got = core.resolve_inputs(
        type("A", (), {"lullaby1": None, "lullaby2": None, "birth": None,
                       "directory": tmp_path})())
    assert got == {"lullaby1.wav": l1, "birth.wav": b}


def test_resolve_inputs_empty_raises(tmp_path):
    with pytest.raises(ValueError, match="[Nn]o input"):
        core.resolve_inputs(
            type("A", (), {"lullaby1": None, "lullaby2": None,
                           "birth": None, "directory": tmp_path})())


def test_format_get_is_tab_separated():
    line = core.format_get("http://10.0.0.5:8123/birth.wav",
                           "birth.wav", 12345, 0xDEADBEEF)
    assert line == ("G http://10.0.0.5:8123/birth.wav\tbirth.wav"
                    "\t12345\tdeadbeef\n")


@pytest.mark.parametrize("line,kind,extra", [
    ("READY sd=ok wifi=192.168.1.44", "ready",
     {"sd": "ok", "wifi": "192.168.1.44"}),
    ("READY sd=fail wifi=none", "ready", {"sd": "fail", "wifi": "none"}),
    ("OK got birth.wav 12345 deadbeef", "ok",
     {"got": {"name": "birth.wav", "size": 12345, "crc": 0xDEADBEEF}}),
    ("ERR get http 404", "err", {}),
    ("FILE lullaby1.wav 10485760 0012abcd", "file",
     {"name": "lullaby1.wav", "size": 10485760, "crc": 0x0012ABCD}),
    ("PROG birth.wav 45", "prog", {"name": "birth.wav", "pct": 45}),
    ("[boot] anything else", "noise", {}),
])
def test_parse_line(line, kind, extra):
    got = core.parse_line(line)
    assert got["kind"] == kind
    for k, v in extra.items():
        assert got[k] == v


def test_parse_line_plain_ok_has_no_got_key():
    got = core.parse_line("OK list")
    assert got == {"kind": "ok", "detail": "list"}
    assert "got" not in got


def test_backup_name():
    assert core.backup_name(datetime(2026, 7, 13, 9, 5, 7)) == \
        "flash-20260713-090507.bin"
