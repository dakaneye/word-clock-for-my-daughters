"""Back-panel SVG renderer.

Generates a per-kid 192×192 mm back panel. Same outer size as the face so
both parts share the frame's exterior footprint. Carries:

  • Outer cut rectangle (192×192 mm).
  • 4 × M3 clearance screw holes centered on each edge, 3.2 mm inset —
    aligned to the center of the 6.4 mm frame-strip thickness. Threaded
    brass inserts live in the frame; these are just pass-through holes.
  • 3 × button-access holes at the SW1/SW2/SW3 PCB positions (X=168.5 mm
    in PCB frame, translated by the 7.1 mm border into face coords).
  • USB-C access cutout — a 10 × 4 mm slot + 2 × M2 screw holes (≈28 mm
    spacing) for mounting a rigid USB-C-female / Micro-USB-male adapter
    (aluminum-alloy type with integrated screw tabs) directly to the
    inside of the panel. The adapter's Micro-USB male end points into
    the case and connects to the ESP32 module's Micro-USB via a short
    standard Micro-USB male-to-female extension cable. Dimensions are
    placeholders — verify against the specific adapter when it arrives.
  • microSD access slot aligned to the HW-125 SD slot position. Flagged
    [LOW] confidence — exact slot location depends on which direction the
    breakout is oriented when plugged in. Verify during first fit-up.
  • Speaker vent — grid of small holes centered in the lower-middle
    region. Final diameter / count TBD once the speaker driver is chosen
    (affects acoustic behavior); current pattern is a reasonable
    placeholder that will not need the speaker spec to look right.
  • Dedication raster engraving — per-kid text rendered from the same
    variable font as the face (Jost Bold for Emory, Fraunces for Nora).

Coordinate system: the panel SVG is drawn as viewed **from outside the
clock** — i.e., looking at the back of the finished clock with the
dedication the right way up and readable. That means features tied to
PCB positions (buttons, microSD) are mirrored horizontally: PCB X is
reflected across the panel's vertical centerline. Without this mirror,
the laser would cut a panel that, when flipped into place, would have
the dedication on the inside surface and the buttons on the wrong side.

PCB frame → panel frame: add BORDER (7.1 mm) on both axes, **then mirror X
across the panel centerline**.
"""
from __future__ import annotations

import csv
from pathlib import Path
from typing import Tuple

from fontTools.ttLib import TTFont

from .fonts import load_font_instance, render_glyph
from .svg_utils import (
    add_cut_circle,
    add_cut_path,
    add_cut_rect,
    add_engrave_path,
    new_svg_document,
)

REPO_ROOT = Path(__file__).resolve().parents[2]
PCB_POS_CSV = REPO_ROOT / "hardware" / "word-clock-all-pos.csv"

# Panel geometry (all mm).
PANEL_SIZE_MM = 192.0
BORDER_MM = 7.1                     # PCB-frame → panel-frame translation
FRAME_THICKNESS_MM = 6.4            # frame-strip wall thickness

# Fastener geometry.
SCREW_CLEARANCE_MM = 3.3            # M3 clearance hole
SCREW_INSET_MM = FRAME_THICKNESS_MM / 2  # 3.2 — center of frame wall

# User-pressed button access. 5 mm hole lets the 6mm-square tact switch's
# ~3 mm plunger be reached with a finger without the hole wandering outside
# the switch body. If this turns out to be awkward in practice, a 3D-printed
# actuator cap (separate task) can press through a slightly smaller hole.
BUTTON_HOLE_DIA_MM = 5.0

# USB-C access cutout — the user-facing port is actually the USB-C receptacle
# face of a rigid USB-C-female / Micro-USB-male adapter that mounts to the
# inside of the back panel via its own screw tabs. Its Micro USB male end
# points into the case and connects to the ESP32 module's Micro USB via a
# short Micro USB male-to-female extension cable (standard part). Cutout
# dimensions: USB-C receptacle face ~10 × 4 mm; tab screws are M2, typically
# ~28 mm apart for this adapter style. Re-measure against the specific
# adapter and tune if needed.
USBC_CUTOUT_W_MM = 10.0
USBC_CUTOUT_H_MM = 4.0
USBC_SCREW_SPACING_MM = 28.0
USBC_SCREW_DIA_MM = 2.2             # M2 clearance

# microSD access slot. HW-125 SD slot opening is roughly 12 × 2 mm.
SD_SLOT_W_MM = 14.0
SD_SLOT_H_MM = 3.0

# Speaker vent — 5×5 grid of 2 mm holes at 4 mm pitch ≈ 16 mm square vent.
SPEAKER_VENT_ROWS = 5
SPEAKER_VENT_COLS = 5
SPEAKER_VENT_HOLE_DIA_MM = 2.0
SPEAKER_VENT_PITCH_MM = 4.0

# Dedication engraving.
DEDICATION_CAP_HEIGHT_MM = 5.0
DEDICATION_LINE_SPACING_MM = 8.0

# Placement centers (panel-frame mm, outside-view convention).
# Button column lives upper-left (mirrored from PCB X=168.5), so the USB-C
# goes upper-right to keep them far apart. Speaker vent is on the vertical
# centerline because the speaker driver's mount location is a free design
# choice — it attaches to the MAX98357A via a 2-wire cable, independent of
# the amp's PCB position.
USBC_CENTER = (164.0, 30.0)
SPEAKER_VENT_CENTER = (96.0, 152.0)
DEDICATION_CENTER = (96.0, 100.0)

KID_CONFIG = {
    "emory": {
        "font_filename": "Jost-Variable.ttf",
        "font_weight": 700,
        "font_opsz": None,
        "dedication_lines": [
            "For Emory",
            "love Dad",
            "2030",
        ],
    },
    "nora": {
        "font_filename": "Fraunces-Variable.ttf",
        "font_weight": 500,
        "font_opsz": 72,
        "dedication_lines": [
            "For Nora",
            "love Dad",
            "2032",
        ],
    },
}


def _pcb_to_panel_outside(pcb_x: float, pcb_y: float) -> Tuple[float, float]:
    """Translate a PCB-frame position into panel-frame mm, **as viewed from
    outside the clock** (mirror X across the panel centerline).

    Without the X mirror, the laser-cut panel would — after flipping to
    attach — have the dedication reading backwards and the buttons on the
    wrong side.
    """
    face_x = pcb_x + BORDER_MM
    face_y = pcb_y + BORDER_MM
    return PANEL_SIZE_MM - face_x, face_y


def _read_switch_positions(csv_path: Path = PCB_POS_CSV) -> list[Tuple[str, float, float]]:
    """Return [(ref, x_panel_mm, y_panel_mm), ...] for SW1, SW2, SW3 in the
    outside-view panel frame.

    CSV Y is signed negative per KiCad CPL convention; normalize first, then
    translate + X-mirror.
    """
    out: list[Tuple[str, float, float]] = []
    with csv_path.open() as f:
        for row in csv.DictReader(f):
            if row["Ref"].strip('"') in ("SW1", "SW2", "SW3"):
                pcb_x = float(row["PosX"])
                pcb_y = -float(row["PosY"])
                px, py = _pcb_to_panel_outside(pcb_x, pcb_y)
                out.append((row["Ref"].strip('"'), px, py))
    return out


def _read_sd_header_position(csv_path: Path = PCB_POS_CSV) -> Tuple[float, float]:
    """Return (x, y) of the HW-125 microSD header in outside-view panel mm."""
    with csv_path.open() as f:
        for row in csv.DictReader(f):
            if "HW-125" in row["Val"] or row["Ref"].strip('"') == "J_SD1":
                pcb_x = float(row["PosX"])
                pcb_y = -float(row["PosY"])
                return _pcb_to_panel_outside(pcb_x, pcb_y)
    raise RuntimeError("Could not find J_SD1 / HW-125 header in CPL CSV")


def _render_dedication(
    dwg, font: TTFont, lines: list[str], center: Tuple[float, float]
) -> None:
    """Render the dedication as raster-engrave paths centered at `center`."""
    cap_height_units = font["OS/2"].sCapHeight
    scale = DEDICATION_CAP_HEIGHT_MM / cap_height_units

    total_h = DEDICATION_LINE_SPACING_MM * (len(lines) - 1)
    y0 = center[1] - total_h / 2

    for i, text in enumerate(lines):
        line_y = y0 + i * DEDICATION_LINE_SPACING_MM

        # Width pass first (to center the line horizontally).
        line_width_units = 0.0
        glyph_infos = []
        for ch in text:
            if ch == " ":
                # Rough inter-word space — no glyph path, just advance by
                # 0.3 × cap height which is a reasonable spacing for most
                # Latin fonts.
                advance_units = cap_height_units * 0.3
                glyph_infos.append((ch, None, None, advance_units))
                line_width_units += advance_units
                continue
            path_data, bbox = render_glyph(font, ch)
            if bbox is None:
                continue
            gxmin, _, gxmax, _ = bbox
            glyph_width = gxmax - gxmin
            # Side-bearing gap matches the character's own left side-bearing
            # for a basic fitted layout; skip kerning as over-engineering.
            advance_units = glyph_width + cap_height_units * 0.05
            glyph_infos.append((ch, path_data, bbox, advance_units))
            line_width_units += advance_units

        line_width_mm = line_width_units * scale
        x = center[0] - line_width_mm / 2

        for ch, path_data, bbox, advance_units in glyph_infos:
            if path_data is None:
                x += advance_units * scale
                continue
            gxmin, _, _, _ = bbox
            # Each glyph is transformed from font-unit space to mm, with the
            # baseline at line_y. Font Y-up → SVG Y-down via negative scale.
            inner_dx = -gxmin
            transform = (
                f"translate({x},{line_y}) "
                f"scale({scale},{-scale}) "
                f"translate({inner_dx},0)"
            )
            add_engrave_path(dwg, path_data, transform=transform)
            x += advance_units * scale


def render_back_panel_svg(kid: str) -> str:
    if kid not in KID_CONFIG:
        raise ValueError(f"unknown kid: {kid!r}")
    cfg = KID_CONFIG[kid]

    dwg = new_svg_document(width_mm=PANEL_SIZE_MM, height_mm=PANEL_SIZE_MM)

    # 1. Outer cut.
    add_cut_rect(dwg, x=0, y=0, width=PANEL_SIZE_MM, height=PANEL_SIZE_MM)

    # 2. 4 × M3 screw-clearance holes (one per edge, centered).
    mid = PANEL_SIZE_MM / 2
    edge_holes = [
        (mid, SCREW_INSET_MM),                        # top
        (PANEL_SIZE_MM - SCREW_INSET_MM, mid),        # right
        (mid, PANEL_SIZE_MM - SCREW_INSET_MM),        # bottom
        (SCREW_INSET_MM, mid),                        # left
    ]
    for cx, cy in edge_holes:
        add_cut_circle(dwg, cx, cy, SCREW_CLEARANCE_MM)

    # 3. Button access holes (aligned to tact switches on PCB bottom side).
    for _ref, x, y in _read_switch_positions():
        add_cut_circle(dwg, x, y, BUTTON_HOLE_DIA_MM)

    # 4. USB-C panel-mount cutout.
    ux, uy = USBC_CENTER
    add_cut_rect(
        dwg,
        x=ux - USBC_CUTOUT_W_MM / 2,
        y=uy - USBC_CUTOUT_H_MM / 2,
        width=USBC_CUTOUT_W_MM,
        height=USBC_CUTOUT_H_MM,
    )
    # Screw holes on either side of the USB-C cutout.
    half_span = USBC_SCREW_SPACING_MM / 2
    add_cut_circle(dwg, ux - half_span, uy, USBC_SCREW_DIA_MM)
    add_cut_circle(dwg, ux + half_span, uy, USBC_SCREW_DIA_MM)

    # 5. microSD access slot — centered on the J_SD1 header X, placed at a
    #    Y that assumes the HW-125 breakout orients with the SD slot pointing
    #    away from the PCB top edge. Verify during Emory assembly.
    sd_x, _sd_header_y = _read_sd_header_position()
    # The SD slot opening sits ~20 mm from the header on a typical HW-125.
    # Place the cutout at that offset in the +Y direction (toward PCB center).
    sd_slot_y = _sd_header_y + 20.0
    add_cut_rect(
        dwg,
        x=sd_x - SD_SLOT_W_MM / 2,
        y=sd_slot_y - SD_SLOT_H_MM / 2,
        width=SD_SLOT_W_MM,
        height=SD_SLOT_H_MM,
    )

    # 6. Speaker vent grid.
    svx, svy = SPEAKER_VENT_CENTER
    total_w = (SPEAKER_VENT_COLS - 1) * SPEAKER_VENT_PITCH_MM
    total_h = (SPEAKER_VENT_ROWS - 1) * SPEAKER_VENT_PITCH_MM
    x0 = svx - total_w / 2
    y0 = svy - total_h / 2
    for r in range(SPEAKER_VENT_ROWS):
        for c in range(SPEAKER_VENT_COLS):
            add_cut_circle(
                dwg,
                x0 + c * SPEAKER_VENT_PITCH_MM,
                y0 + r * SPEAKER_VENT_PITCH_MM,
                SPEAKER_VENT_HOLE_DIA_MM,
            )

    # 7. Dedication engraving.
    font = load_font_instance(
        filename=cfg["font_filename"],
        weight=cfg["font_weight"],
        opsz=cfg["font_opsz"],
    )
    _render_dedication(dwg, font, cfg["dedication_lines"], DEDICATION_CENTER)

    return dwg.tostring()
