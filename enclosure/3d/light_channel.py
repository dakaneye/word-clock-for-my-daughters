"""Light channel — production part for the word clock.

Single-piece honeycomb that sits on top of the PCB, with one pocket per
word (multi-cell, one LED at center) and one pocket per filler cell
(single-cell, no LED — stays dark behind the corresponding face letter).
Walls are 2 mm thick black PLA, 17.84 mm tall — extend from PCB top to
the diffusion film bonded on the back of the face.

Per-kid: NAME pocket length differs (Emory 5 cells, Nora 4 cells); pass
--target.

Stack-up reference (from the assembly plan + 2026-04-26 decision):
    face wood        3.20 mm  (z = 44.80 to 48.00)
    diffusion film   0.16 mm  (z = 44.64 to 44.80; bonded to face back)
    light channel   17.84 mm  (z = 26.80 to 44.64; top edge seals to film)
    PCB              1.60 mm  (z = 25.20 to 26.80; LEDs on top side)
    standoff post   20.00 mm  (z =  5.20 to 25.20)
    standoff flange  2.00 mm  (z =  3.20 to  5.20; E6000'd to back panel)
    back panel       3.20 mm  (z =  0.00 to  3.20)

Outer envelope = 13 × CELL_PITCH_MM = 177.8 mm, matching the face
SVG's letter-grid extent (`enclosure/scripts/render_face.py:GRID_SIZE_MM`
divided by 13). The OUTER cells have no thick outer wall — the FRAME
wall (6.4 mm walnut/maple) right next to the channel does the outer
light blocking instead. This keeps the channel compact and avoids
fighting the frame interior dimension for that last 1-2 mm of
perimeter.

Alignment note (verified 2026-04-26 via truth-verifier review against
`hardware/word-clock-all-pos.csv`): the channel pitch (13.6769 mm =
177.8 / 13) matches the face SVG, NOT the PCB's actual LED-grid pitch
(13.68 mm = ~177.84 mm extent, derived from the LED CSV). PCB also has
its LED grid offset by approximately (-1.5 mm, +0.5 mm) from PCB
outline origin. The two are 0.04 mm out of sync end-to-end, plus a
1.5 mm origin offset. We deliberately align this script to the FACE
because that's the visible alignment surface — channel walls land at
face cell boundaries, ensuring inter-letter light isolation. The
consequence is LEDs sit ~1.5 mm off-center within their word pockets;
with 11.68 mm pocket interior vs 5 mm LED package, that's 3.34 mm of
clearance to the nearest wall — invisible at typical viewing distance.

The diffuser stack is now film-only (acrylic dropped 2026-04-26 after
bench test showed it caused 5 mm of lateral light bleed). If first-light
testing reveals visible hot spots through the film, the recovery is to
cut acrylic into per-pocket pieces and reprint this script with
WALL_HEIGHT_MM bumped to ~21 mm (so walls extend through the acrylic
level, isolating each pocket optically).

Run from repo root:
    enclosure/3d/.venv/bin/python enclosure/3d/light_channel.py --target emory
    enclosure/3d/.venv/bin/python enclosure/3d/light_channel.py --target nora

Output: enclosure/3d/out/light_channel_{target}.stl

Print: black PLA (light blocking is mandatory), open ends up so all
pockets vent properly. ~6-8 hours at default A1 speeds. Brim recommended
since the bottom face is mostly negative space (just wall edges in
contact with the bed).
"""
import argparse
from build123d import Align, Box, Compound, Pos, export_stl
from pathlib import Path

# ─── Locked parameters ──────────────────────────────────────────
GRID_SIZE_MM        = 177.8                  # matches face GRID_SIZE_MM
                                             # and PCB outline 177.8 × 177.8
CELL_PITCH_MM       = GRID_SIZE_MM / 13      # ≈ 13.6769 mm
WALL_THICKNESS_MM   = 2.0
WALL_HEIGHT_MM      = 17.84                  # PCB top → film bottom

# ─── Word spans (mirror grid.cpp set_spans_common) ──────────────
# Each entry: (row, col, length). One LED per word at word center.
# Order doesn't matter for the geometry — set semantics on word_cells.
WORD_SPANS = [
    # row 0: I T  E  I S  M  T E N  H A L F
    (0, 0, 2),    # IT
    (0, 3, 2),    # IS
    (0, 6, 3),    # TEN_MIN
    (0, 9, 4),    # HALF
    # row 1: Q U A R T E R  T W E N T Y
    (1, 0, 7),    # QUARTER
    (1, 7, 6),    # TWENTY
    # row 2: F I V E  O  M I N U T E S  R
    (2, 0, 4),    # FIVE_MIN
    (2, 5, 7),    # MINUTES
    # row 3: Y  H A P P Y  P A S T  T O  1
    (3, 1, 5),    # HAPPY
    (3, 6, 4),    # PAST
    (3, 10, 2),   # TO
    # row 4: O N E  B I R T H  T H R E E
    (4, 0, 3),    # ONE
    (4, 3, 5),    # BIRTH
    (4, 8, 5),    # THREE
    # row 5: E L E V E N  F O U R  D A Y
    (5, 0, 6),    # ELEVEN
    (5, 6, 4),    # FOUR
    (5, 10, 3),   # DAY
    # row 6: T W O  E I G H T  S E V E N
    (6, 0, 3),    # TWO
    (6, 3, 5),    # EIGHT
    (6, 8, 5),    # SEVEN
    # row 7: N I N E  S I X  T W E L V E
    (7, 0, 4),    # NINE
    (7, 4, 3),    # SIX
    (7, 7, 6),    # TWELVE
    # row 8: <NAME>  0  A  T  F I V E  6   (NAME added per-kid below)
    #   - Cells 5 ('0'/'1') and 12 ('6'/'9') are filler letters per kid.
    #   - Cells 6-7 ('A','T') are decorative-only — the actual AT word
    #     span is on row 11 (cols 10-11). These two cells become
    #     1-cell filler pockets here, matching grid.cpp's intent
    #     (set_spans_common doesn't register them).
    (8, 8, 4),    # FIVE_HR
    # row 9: T E N  O C L O C K  N O O N
    (9, 0, 3),    # TEN_HR
    (9, 3, 6),    # OCLOCK
    (9, 9, 4),    # NOON
    # row 10: 2  I N  T H E  M O R N I N G
    (10, 1, 2),   # IN
    (10, 3, 3),   # THE
    (10, 6, 7),   # MORNING
    # row 11: A F T E R N O O N  0  A T  2
    (11, 0, 9),   # AFTERNOON
    (11, 10, 2),  # AT
    # row 12: E V E N I N G  3  N I G H T
    (12, 0, 7),   # EVENING
    (12, 8, 5),   # NIGHT
]

# NAME varies per-kid (length differs). Always at row 8 col 0.
NAME_SPANS = {
    "emory": (8, 0, 5),  # E M O R Y
    "nora":  (8, 0, 4),  # N O R A
}


def build_channel(target: str) -> tuple[Compound, int, int]:
    """Construct the channel as outer-envelope minus all pockets.

    Returns (channel_compound, n_word_pockets, n_filler_pockets). The
    target validation is here (in addition to argparse choices=) so this
    function is safe to call directly from tests or other scripts
    without going through the CLI.
    """
    if target not in NAME_SPANS:
        raise ValueError(f"unknown target: {target!r} (expected emory or nora)")

    # All word pockets for this target (NAME varies)
    spans = WORD_SPANS + [NAME_SPANS[target]]

    # Cells covered by some word — used to derive filler cells.
    # Also catches overlapping spans (would surface as |word_cells| < sum of lengths).
    word_cells = set()
    for row, col, length in spans:
        for c in range(col, col + length):
            word_cells.add((row, c))
    total_word_lengths = sum(length for _, _, length in spans)
    assert len(word_cells) == total_word_lengths, (
        f"WORD_SPANS contain overlapping cells: "
        f"unique cells {len(word_cells)} != sum of lengths {total_word_lengths}"
    )

    # Filler cells = grid cells NOT in any word. Each gets a 1-cell pocket.
    filler_cells = [
        (row, col)
        for row in range(13)
        for col in range(13)
        if (row, col) not in word_cells
    ]
    # Every cell in the 13×13 grid must be covered by exactly one pocket
    # (word or filler). Catches drift between WORD_SPANS and grid.cpp.
    assert len(word_cells) + len(filler_cells) == 13 * 13, (
        f"cells not fully covered: "
        f"{len(word_cells)} word + {len(filler_cells)} filler != {13 * 13}"
    )

    BOTTOM = (Align.MIN, Align.MIN, Align.MIN)
    half_wall = WALL_THICKNESS_MM / 2

    # Outer envelope = grid extent. Outer cells touch the envelope
    # boundary; outer light blocking comes from the frame wood, not from
    # a thick perimeter wall on the channel itself.
    channel = Box(GRID_SIZE_MM, GRID_SIZE_MM, WALL_HEIGHT_MM, align=BOTTOM)

    # Subtract a pocket for each word. Pocket extends from
    # (col*pitch + half_wall) to ((col+length)*pitch − half_wall) so
    # adjacent pockets share a 2 mm wall.
    for row, col, length in spans:
        x = col * CELL_PITCH_MM + half_wall
        y = row * CELL_PITCH_MM + half_wall
        w = length * CELL_PITCH_MM - WALL_THICKNESS_MM
        h = CELL_PITCH_MM - WALL_THICKNESS_MM
        pocket = Box(w, h, WALL_HEIGHT_MM + 0.2, align=BOTTOM)
        channel = channel - Pos(x, y, -0.1) * pocket

    # Subtract a 1-cell pocket for each filler. Walls around fillers are
    # shared with adjacent word pockets — also 2 mm thick.
    for row, col in filler_cells:
        x = col * CELL_PITCH_MM + half_wall
        y = row * CELL_PITCH_MM + half_wall
        s = CELL_PITCH_MM - WALL_THICKNESS_MM
        pocket = Box(s, s, WALL_HEIGHT_MM + 0.2, align=BOTTOM)
        channel = channel - Pos(x, y, -0.1) * pocket

    return channel, len(spans), len(filler_cells)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["emory", "nora"], required=True)
    args = parser.parse_args()

    channel, n_words, n_fillers = build_channel(args.target)

    out_dir = Path(__file__).parent / "out"
    out_dir.mkdir(exist_ok=True)
    stl_path = out_dir / f"light_channel_{args.target}.stl"
    export_stl(channel, str(stl_path))

    print(f"wrote {stl_path}")
    print(f"  target:           {args.target}")
    print(f"  envelope:         {GRID_SIZE_MM:.2f} × {GRID_SIZE_MM:.2f} × {WALL_HEIGHT_MM:.2f} mm")
    print(f"  cell pitch:       {CELL_PITCH_MM:.4f} mm")
    print(f"  wall thickness:   {WALL_THICKNESS_MM:.1f} mm")
    print(f"  word pockets:     {n_words}")
    print(f"  filler pockets:   {n_fillers}")
    print(f"  total pockets:    {n_words + n_fillers}")


if __name__ == "__main__":
    main()
