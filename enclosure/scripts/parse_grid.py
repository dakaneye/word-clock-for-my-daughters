"""Parse firmware/lib/core/src/grid.cpp into Python dict of letter grids.

The C++ source defines:
  - constexpr char EMORY_LETTERS[13*13] = { 'I','T',... };
  - constexpr char NORA_ROW_8[13] = { 'N','O','R','A','R','1','A','T','F','I','V','E','9' };
  - Nora is built from Emory + per-cell patches in build_nora() (cells (0,2)='N', (0,5)='O', etc.)

This parser extracts both grids and returns dict {"emory": grid, "nora": grid},
where each grid is a list of 13 lists of 13 single-char strings.
"""
from pathlib import Path
import re
from typing import Dict, List


def parse_grid_cpp(cpp_path: Path) -> Dict[str, List[List[str]]]:
    text = Path(cpp_path).read_text()

    # Extract EMORY_LETTERS array body (everything between { and };)
    emory_match = re.search(r"EMORY_LETTERS\[13\s*\*\s*13\]\s*=\s*\{([^}]+)\}", text, re.DOTALL)
    if not emory_match:
        raise ValueError(f"Could not find EMORY_LETTERS in {cpp_path}")
    emory_chars = re.findall(r"'(.)'", emory_match.group(1))
    if len(emory_chars) != 169:
        raise ValueError(f"EMORY_LETTERS yielded {len(emory_chars)} chars, expected 169")
    emory = [emory_chars[r * 13:(r + 1) * 13] for r in range(13)]

    # Extract NORA_ROW_8
    nora_row_match = re.search(r"NORA_ROW_8\[13\]\s*=\s*\{([^}]+)\}", text, re.DOTALL)
    if not nora_row_match:
        raise ValueError(f"Could not find NORA_ROW_8 in {cpp_path}")
    nora_row_8 = re.findall(r"'(.)'", nora_row_match.group(1))
    if len(nora_row_8) != 13:
        raise ValueError(f"NORA_ROW_8 yielded {len(nora_row_8)} chars, expected 13")

    # Build Nora from Emory + patches per build_nora() in grid.cpp:
    #   row 8 entirely replaced with NORA_ROW_8
    #   (0,2)=N (0,5)=O (2,4)=R (2,12)=A (3,0)=M (3,12)=A (12,7)=5
    nora = [row[:] for row in emory]
    nora[8] = nora_row_8
    nora_patches = {
        (0, 2): "N", (0, 5): "O",
        (2, 4): "R", (2, 12): "A",
        (3, 0): "M", (3, 12): "A",
        (12, 7): "5",
    }
    for (r, c), ch in nora_patches.items():
        nora[r][c] = ch

    return {"emory": emory, "nora": nora}
