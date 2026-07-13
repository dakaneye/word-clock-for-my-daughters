import argparse
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


def _args(**kw):
    base = {"lullaby1": None, "lullaby2": None, "birth": None,
            "directory": None}
    base.update(kw)
    return argparse.Namespace(**base)


def test_resolve_inputs_from_flags(tmp_path):
    a = make_wav(tmp_path / "a.wav")
    b = make_wav(tmp_path / "b.wav")
    got = core.resolve_inputs(_args(lullaby1=a, lullaby2=b))
    assert got == {"lullaby1.wav": a, "lullaby2.wav": b}


def test_resolve_inputs_flags_win_over_directory(tmp_path):
    flag = make_wav(tmp_path / "flag.wav")
    make_wav(tmp_path / "lullaby2.wav")   # would match a directory scan
    got = core.resolve_inputs(_args(birth=flag, directory=tmp_path))
    assert got == {"birth.wav": flag}     # any flag disables the dir scan


def test_resolve_inputs_from_directory(tmp_path):
    l1 = make_wav(tmp_path / "lullaby1.m4a")
    b = make_wav(tmp_path / "birth.wav")
    got = core.resolve_inputs(_args(directory=tmp_path))
    assert got == {"lullaby1.wav": l1, "birth.wav": b}


def test_resolve_inputs_empty_raises(tmp_path):
    with pytest.raises(ValueError, match="[Nn]o input"):
        core.resolve_inputs(_args(directory=tmp_path))


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


def test_validate_wav_truncated_fmt_chunk(tmp_path):
    fmt_chunk = struct.pack("<4sI", b"fmt ", 8) + b"\x01\x00\x01\x00PADD"
    data_chunk = struct.pack("<4sI", b"data", 16) + b"\x00\x01" * 8
    body = b"WAVE" + fmt_chunk + data_chunk
    p = tmp_path / "trunc.wav"
    p.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
    with pytest.raises(ValueError, match="truncated fmt"):
        core.validate_wav(p)


def test_validate_wav_missing_fmt_chunk(tmp_path):
    data_chunk = struct.pack("<4sI", b"data", 4) + b"\x00\x01\x00\x01"
    body = b"WAVE" + data_chunk + b"\x00" * 24   # pad past the 44-byte floor
    p = tmp_path / "nofmt.wav"
    p.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
    with pytest.raises(ValueError, match="missing fmt"):
        core.validate_wav(p)


def test_validate_wav_rejects_truncated_file(tmp_path):
    p = tmp_path / "short.wav"
    p.write_bytes(b"RIFF\x00\x00")
    with pytest.raises(ValueError, match="RIFF"):
        core.validate_wav(p)


def test_file_crc32_known_vector(tmp_path):
    # The canonical CRC32 check value. The loader firmware's hand-rolled
    # table (sd_loader/src/main.cpp) must agree with zlib on this vector;
    # pinning it here guards the Python side of that cross-language pact.
    p = tmp_path / "vec.bin"
    p.write_bytes(b"123456789")
    assert core.file_crc32(p) == 0xCBF43926


def test_got_matches_accepts_exact_payload():
    reply = core.parse_line("OK got birth.wav 12345 deadbeef")
    assert core.got_matches(reply, "birth.wav", 12345, 0xDEADBEEF)


@pytest.mark.parametrize("line", [
    "OK got lullaby1.wav 12345 deadbeef",   # replayed: wrong file
    "OK got birth.wav 99 deadbeef",          # wrong size
    "OK got birth.wav 12345 00000000",       # wrong crc
    "OK list",                               # no got payload at all
])
def test_got_matches_rejects_mismatches(line):
    reply = core.parse_line(line)
    assert not core.got_matches(reply, "birth.wav", 12345, 0xDEADBEEF)


def _fake_flash_image(path, size=4 * 1024 * 1024, magic=b"\xe9"):
    data = bytearray(b"\xff" * size)
    data[0x1000:0x1001] = magic
    path.write_bytes(bytes(data))
    return path


def test_validate_backup_image_accepts_plausible_dump(tmp_path):
    p = _fake_flash_image(tmp_path / "good.bin")
    core.validate_backup_image(p, 4 * 1024 * 1024)   # no raise
    core.validate_backup_image(p, None)              # size unknown is ok


def test_validate_backup_image_rejects_truncated(tmp_path):
    p = _fake_flash_image(tmp_path / "short.bin", size=1024 * 1024)
    with pytest.raises(ValueError, match="truncated or wrong"):
        core.validate_backup_image(p, 4 * 1024 * 1024)


def test_validate_backup_image_rejects_garbage(tmp_path):
    p = _fake_flash_image(tmp_path / "junk.bin", magic=b"\x00")
    with pytest.raises(ValueError, match="bootloader magic"):
        core.validate_backup_image(p, 4 * 1024 * 1024)
