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
