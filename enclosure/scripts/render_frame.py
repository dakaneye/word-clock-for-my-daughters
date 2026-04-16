"""Frame strip + box-joint SVG renderer.

Produces 4 frame strips per kid, each 192×48 mm × 6.4 mm material thickness.
Box-joint corners with 8 fingers × 6 mm pitch (finger width = material thickness).
Two strips have inverse finger pattern to mate with their neighbors.
"""
from .svg_utils import new_svg_document, add_cut_path

FRAME_LENGTH_MM = 192.0
FRAME_DEPTH_MM = 48.0
MATERIAL_THICKNESS_MM = 6.4

FINGER_COUNT = 8
FINGER_PITCH_MM = FRAME_DEPTH_MM / FINGER_COUNT  # = 6.0
KERF_COMPENSATION_MM = 0.1  # +0.1 finger / -0.1 slot for press-fit


def finger_path_for_edge(invert: bool) -> str:
    """SVG path fragment for the strip's RIGHT short edge, traversing DOWN
    from (FRAME_LENGTH_MM, 0) to (FRAME_LENGTH_MM, FRAME_DEPTH_MM).

    Fingers stay at the strip edge (X = FRAME_LENGTH_MM); slots cut INWARD
    (negative X by MATERIAL_THICKNESS_MM). The strip's outer extent never
    exceeds FRAME_LENGTH_MM in X.

    invert=False: starts with a finger (edge stays straight at the top)
    invert=True:  starts with a slot (cut inward at the top)

    Uses lowercase 'l' (relative line) commands.
    """
    parts = []
    for i in range(FINGER_COUNT):
        is_finger = (i % 2 == 0) ^ invert
        if is_finger:
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            parts.append(f"l 0,{pitch}")
        else:
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l -{MATERIAL_THICKNESS_MM},0")
            parts.append(f"l 0,{pitch}")
            parts.append(f"l {MATERIAL_THICKNESS_MM},0")
    return " ".join(parts)


def _left_edge_path(invert: bool) -> str:
    """SVG path fragment for the strip's LEFT short edge, traversing UP
    from (0, FRAME_DEPTH_MM) to (0, 0).

    Fingers stay at the strip edge (X = 0); slots cut INWARD (positive X
    by MATERIAL_THICKNESS_MM). The strip's outer extent never goes
    negative in X. Fingers are iterated in reverse so the assembled box
    presents a visually consistent corner pattern.
    """
    parts = []
    for i in reversed(range(FINGER_COUNT)):
        is_finger = (i % 2 == 0) ^ invert
        if is_finger:
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            parts.append(f"l 0,{-pitch}")
        else:
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l {MATERIAL_THICKNESS_MM},0")
            parts.append(f"l 0,{-pitch}")
            parts.append(f"l -{MATERIAL_THICKNESS_MM},0")
    return " ".join(parts)


def frame_strip_path(invert_left: bool, invert_right: bool) -> str:
    """Generate the full SVG path-data string for one frame strip.

    Strip is FRAME_LENGTH_MM long (X) × FRAME_DEPTH_MM tall (Y), with finger-joint
    cuts on the two short edges. Path traverses: top edge → right finger edge →
    bottom edge → left finger edge → close.
    """
    parts = ["M 0,0"]
    parts.append(f"L {FRAME_LENGTH_MM},0")
    parts.append(finger_path_for_edge(invert=invert_right))
    parts.append(f"L 0,{FRAME_DEPTH_MM}")
    parts.append(_left_edge_path(invert=invert_left))
    parts.append("Z")
    return " ".join(parts)


STRIP_GAP_MM = 5.0  # spacing between stacked strips on the Ponoko sheet


def render_frame_svg() -> str:
    """Generate the SVG containing all 4 frame strips for one clock.

    Strips stacked vertically with STRIP_GAP_MM gap. Total sheet:
    192 mm wide × (4*48 + 3*5) = 207 mm tall.

    Inversion pattern alternates so adjacent strips at a corner have mating
    (inverse) finger patterns:
      strip 0: invert_left=False, invert_right=True
      strip 1: invert_left=True,  invert_right=False
      strip 2: invert_left=False, invert_right=True
      strip 3: invert_left=True,  invert_right=False
    """
    sheet_width = FRAME_LENGTH_MM
    sheet_height = 4 * FRAME_DEPTH_MM + 3 * STRIP_GAP_MM

    dwg = new_svg_document(width_mm=sheet_width, height_mm=sheet_height)

    inversion_patterns = [
        (False, True),
        (True, False),
        (False, True),
        (True, False),
    ]
    for i, (inv_l, inv_r) in enumerate(inversion_patterns):
        path_data = frame_strip_path(invert_left=inv_l, invert_right=inv_r)
        y_offset = i * (FRAME_DEPTH_MM + STRIP_GAP_MM)
        add_cut_path(dwg, path_data, transform=f"translate(0,{y_offset})")

    return dwg.tostring()
