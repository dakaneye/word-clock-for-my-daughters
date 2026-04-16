"""Back-panel SVG renderer.

Generates a per-kid 192×192 mm back panel. Same outer size as the face so
both parts share the frame's exterior footprint. Carries:

  • Outer cut rectangle (192×192 mm).
  • 4 × M3 clearance screw holes centered on each edge, 3.2 mm inset —
    aligned to the center of the 6.4 mm frame-strip thickness. Threaded
    brass inserts live in the frame; these are just pass-through holes.
  • 3 × button-access holes at the SW1/SW2/SW3 PCB positions (X=168.5 mm
    in PCB frame, translated by the 7.1 mm border into face coords).
  • One grommeted cable exit hole (~6 mm) — a single Micro-USB-to-USB-C
    cable lives permanently inside the clock, plugged into the ESP32's
    native Micro USB port, with the USB-C end sticking out through this
    hole. A rubber grommet protects the cable at the wood edge and
    provides strain relief. Simplest possible USB path: no panel-mount
    adapter, no internal pigtail, no screw alignment. Cable replaces
    via removing the 4 corner screws.
  • Speaker vent — grid of small holes centered in the lower-middle
    region. Final diameter / count TBD once the speaker driver is chosen
    (affects acoustic behavior); current pattern is a reasonable
    placeholder that will not need the speaker spec to look right.
  • Dedication raster engraving — per-kid text rendered from the same
    variable font as the face (Jost Bold for Emory, Fraunces for Nora).
  • Button labels ("Hour", "Min", "Audio") raster-engraved next to each
    tact-switch cutout so the user can tell the buttons apart by touch
    and by sight when reading the back of the clock.

  SD card slot is intentionally **not** cut — the card is inserted once
  before the back panel closes, and subsequent audio updates are rare
  enough that removing the 4 corner screws is acceptable.

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

# Fastener geometry. Machine screws go through the back panel at the 4
# corners, into M3 brass hex spacers (5 mm across flats × 10 mm long,
# female-female threaded) that are epoxied into each interior corner of
# the frame. The spacer provides the machine thread, independent of the
# frame's 6.4 mm wall thickness — no threaded insert needed.
SCREW_CLEARANCE_MM = 3.3            # M3 clearance hole
SCREW_CORNER_INSET_MM = 4.0         # center of each corner screw hole,
                                     # measured in from BOTH adjacent edges

# User-pressed button access. QTEATAK tact switch spec:
#   body 6 × 6 × 5 mm + plunger 1.1 mm above body = 6.1 mm above PCB.
# With a 20 mm standoff there's ~14 mm of air above the plunger — too far
# for a finger to press directly through a small panel hole. A 3D-printed
# button actuator cap slides through this hole (~6 mm shaft OD) and
# bridges the gap to the plunger.
BUTTON_HOLE_DIA_MM = 6.5

# USB cable exit. Single Micro-USB-to-USB-C cable permanently resides
# inside the clock, plugged into the ESP32 module's native Micro USB port,
# with its USB-C end routed out through this grommeted hole. A standard
# rubber grommet (~3 mm ID, 6 mm OD, for a ~5 mm panel hole) protects
# the cable at the wood edge.
USB_CABLE_EXIT_DIA_MM = 6.0

# Speaker vent — 5×5 grid of 2 mm holes at 4 mm pitch ≈ 16 mm square vent.
SPEAKER_VENT_ROWS = 5
SPEAKER_VENT_COLS = 5
SPEAKER_VENT_HOLE_DIA_MM = 2.0
SPEAKER_VENT_PITCH_MM = 4.0

# Dedication engraving.
DEDICATION_CAP_HEIGHT_MM = 5.0
DEDICATION_LINE_SPACING_MM = 8.0

# Button labels — raster-engraved next to each switch hole.
BUTTON_LABEL_CAP_HEIGHT_MM = 3.0
BUTTON_LABEL_X_OFFSET_MM = 8.0   # label leading edge, right of button hole center
BUTTON_LABEL_Y_OFFSET_MM = 1.0   # baseline nudge so text visually centers on hole

BUTTON_LABELS = {
    "SW3": "Hour",
    "SW1": "Min",
    "SW2": "Audio",
}

# Placement centers (panel-frame mm, outside-view convention).
#
# Button column: upper-left (mirrored from PCB X=168.5).
#
# USB cable exit: upper-center above the ESP32 module. The module is at PCB
# (73, 72.5); its pin-1 end (USB end per the 38-pin dev-board convention)
# sits at PCB (84, 73.5) with the micro-USB receptacle pointing toward
# Y=0 (top of PCB). In outside-view panel coords that's X = 192 − (73+7.1)
# ≈ 112. Grommet at Y=20 keeps the cable run short and lets the cable
# flex 90° from horizontal-off-the-port up to the back panel.
#
# Speaker vent: lower-left, close to the MAX98357A amp header J_AMP1
# (PCB 102.75, −87.25 → outside-view panel ≈ (82, 94)). Placing the
# speaker driver toward the lower-left minimizes the 2-wire speaker cable
# run to the amp. Speaker body is 28.2 × 31 mm + mounting tabs, so the
# vent grid's center stays a comfortable distance from both the left
# edge (at ~50) and the bottom edge (at ~150).
USB_CABLE_EXIT_CENTER = (112.0, 20.0)
SPEAKER_VENT_CENTER = (55.0, 150.0)
DEDICATION_CENTER = (96.0, 100.0)

KID_CONFIG = {
    "emory": {
        "font_filename": "Jost-Variable.ttf",
        "font_weight": 700,
        "font_opsz": None,
        "dedication_lines": [
            "For Emory",
            "love Dad",
        ],
    },
    "nora": {
        "font_filename": "Fraunces-Variable.ttf",
        "font_weight": 500,
        "font_opsz": 72,
        "dedication_lines": [
            "For Nora",
            "love Dad",
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


def _render_engraved_text(
    dwg, font: TTFont, text: str, baseline_mm: Tuple[float, float],
    cap_height_mm: float, *, align: str = "center",
) -> None:
    """Render a single line of text as raster-engrave paths.

    ``align`` is one of "center" (text centered on baseline_mm X), "left"
    (baseline_mm X is the left edge), "right" (baseline_mm X is the right edge).
    Y is always the baseline.
    """
    cap_height_units = font["OS/2"].sCapHeight
    scale = cap_height_mm / cap_height_units

    line_width_units = 0.0
    glyph_infos = []
    for ch in text:
        if ch == " ":
            advance_units = cap_height_units * 0.3
            glyph_infos.append((ch, None, None, advance_units))
            line_width_units += advance_units
            continue
        path_data, bbox = render_glyph(font, ch)
        if bbox is None:
            continue
        gxmin, _, gxmax, _ = bbox
        advance_units = (gxmax - gxmin) + cap_height_units * 0.05
        glyph_infos.append((ch, path_data, bbox, advance_units))
        line_width_units += advance_units

    line_width_mm = line_width_units * scale
    if align == "center":
        x = baseline_mm[0] - line_width_mm / 2
    elif align == "left":
        x = baseline_mm[0]
    elif align == "right":
        x = baseline_mm[0] - line_width_mm
    else:
        raise ValueError(f"invalid align: {align!r}")
    line_y = baseline_mm[1]

    for ch, path_data, bbox, advance_units in glyph_infos:
        if path_data is None:
            x += advance_units * scale
            continue
        gxmin, _, _, _ = bbox
        inner_dx = -gxmin
        transform = (
            f"translate({x},{line_y}) "
            f"scale({scale},{-scale}) "
            f"translate({inner_dx},0)"
        )
        add_engrave_path(dwg, path_data, transform=transform)
        x += advance_units * scale


def _render_dedication(
    dwg, font: TTFont, lines: list[str], center: Tuple[float, float]
) -> None:
    """Render the dedication as raster-engrave paths centered at `center`."""
    total_h = DEDICATION_LINE_SPACING_MM * (len(lines) - 1)
    y0 = center[1] - total_h / 2
    for i, text in enumerate(lines):
        line_y = y0 + i * DEDICATION_LINE_SPACING_MM
        _render_engraved_text(
            dwg, font, text, (center[0], line_y),
            DEDICATION_CAP_HEIGHT_MM, align="center",
        )


def _render_button_labels(
    dwg, font: TTFont, switches: list[Tuple[str, float, float]]
) -> None:
    """Engrave "Hour" / "Min" / "Audio" next to each tact-switch cutout."""
    for ref, bx, by in switches:
        label = BUTTON_LABELS.get(ref)
        if not label:
            continue
        label_x = bx + BUTTON_LABEL_X_OFFSET_MM
        label_y = by + BUTTON_LABEL_Y_OFFSET_MM
        _render_engraved_text(
            dwg, font, label, (label_x, label_y),
            BUTTON_LABEL_CAP_HEIGHT_MM, align="left",
        )


def render_back_panel_svg(kid: str) -> str:
    if kid not in KID_CONFIG:
        raise ValueError(f"unknown kid: {kid!r}")
    cfg = KID_CONFIG[kid]

    dwg = new_svg_document(width_mm=PANEL_SIZE_MM, height_mm=PANEL_SIZE_MM)

    # 1. Outer cut.
    add_cut_rect(dwg, x=0, y=0, width=PANEL_SIZE_MM, height=PANEL_SIZE_MM)

    # 2. 4 × M3 corner screw-clearance holes, matching the corner-epoxied
    # brass hex spacers inside the frame.
    inset = SCREW_CORNER_INSET_MM
    corner_holes = [
        (inset, inset),                               # top-left
        (PANEL_SIZE_MM - inset, inset),               # top-right
        (inset, PANEL_SIZE_MM - inset),               # bottom-left
        (PANEL_SIZE_MM - inset, PANEL_SIZE_MM - inset),  # bottom-right
    ]
    for cx, cy in corner_holes:
        add_cut_circle(dwg, cx, cy, SCREW_CLEARANCE_MM)

    # 3. Button access holes (aligned to tact switches on PCB bottom side).
    switches = _read_switch_positions()
    for _ref, x, y in switches:
        add_cut_circle(dwg, x, y, BUTTON_HOLE_DIA_MM)

    # 4. USB cable exit — single grommeted hole. Cable lives inside the case
    #    plugged into the ESP32's micro-USB port; USB-C end exits here.
    ux, uy = USB_CABLE_EXIT_CENTER
    add_cut_circle(dwg, ux, uy, USB_CABLE_EXIT_DIA_MM)

    # 5. Speaker vent grid.
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

    # 6. Dedication + button labels (both raster-engraved in the kid's font).
    font = load_font_instance(
        filename=cfg["font_filename"],
        weight=cfg["font_weight"],
        opsz=cfg["font_opsz"],
    )
    _render_dedication(dwg, font, cfg["dedication_lines"], DEDICATION_CENTER)
    _render_button_labels(dwg, font, switches)

    return dwg.tostring()
