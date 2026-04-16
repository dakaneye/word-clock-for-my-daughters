"""Tests for parse_grid: extracts the 13×13 letter layout from firmware/lib/core/src/grid.cpp."""
from pathlib import Path
import pytest

from enclosure.scripts.parse_grid import parse_grid_cpp

REPO_ROOT = Path(__file__).resolve().parents[3]
GRID_CPP = REPO_ROOT / "firmware" / "lib" / "core" / "src" / "grid.cpp"


def test_parse_grid_returns_emory_and_nora():
    grids = parse_grid_cpp(GRID_CPP)
    assert "emory" in grids
    assert "nora" in grids


def test_emory_grid_is_13x13():
    grids = parse_grid_cpp(GRID_CPP)
    emory = grids["emory"]
    assert len(emory) == 13
    for row in emory:
        assert len(row) == 13


def test_emory_row_0_matches_spec():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 0: I T E I S M T E N H A L F
    assert grids["emory"][0] == list("ITEISMTENHALF")


def test_emory_row_8_is_emory_name():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 8: E M O R Y 0 A T F I V E 6
    assert grids["emory"][8] == list("EMORY0ATFIVE6")


def test_nora_grid_is_13x13():
    grids = parse_grid_cpp(GRID_CPP)
    nora = grids["nora"]
    assert len(nora) == 13
    for row in nora:
        assert len(row) == 13


def test_nora_row_8_is_nora_name():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 8 entirely replaced: N O R A R 1 A T F I V E 9
    assert grids["nora"][8] == list("NORAR1ATFIVE9")


def test_nora_row_0_has_filler_patches():
    grids = parse_grid_cpp(GRID_CPP)
    # Row 0 fillers patched: (0,2)='N', (0,5)='O'
    # Emory row 0: I T E I S M T E N H A L F
    # After Nora patches: I T N I S O T E N H A L F
    assert grids["nora"][0] == list("ITNISOTENHALF")


def test_emory_acrostic_reading_order_spells_birthday():
    """Emory filler cells in row-major reading order spell 'EMORY1062023' (12 chars)."""
    grids = parse_grid_cpp(GRID_CPP)
    emory = grids["emory"]
    filler_positions = [
        (0, 2), (0, 5),
        (2, 4), (2, 12),
        (3, 0), (3, 12),
        (8, 5), (8, 12),
        (10, 0),
        (11, 9), (11, 12),
        (12, 7),
    ]
    fillers = "".join(emory[r][c] for r, c in filler_positions)
    assert fillers == "EMORY1062023", f"Got: {fillers!r}"


def test_nora_acrostic_reading_order_spells_birthday():
    """Nora filler cells in row-major reading order spell 'NORAMAR192025' (13 chars)."""
    grids = parse_grid_cpp(GRID_CPP)
    nora = grids["nora"]
    filler_positions = [
        (0, 2), (0, 5),
        (2, 4), (2, 12),
        (3, 0), (3, 12),
        (8, 4), (8, 5), (8, 12),
        (10, 0),
        (11, 9), (11, 12),
        (12, 7),
    ]
    fillers = "".join(nora[r][c] for r, c in filler_positions)
    assert fillers == "NORAMAR192025", f"Got: {fillers!r}"
