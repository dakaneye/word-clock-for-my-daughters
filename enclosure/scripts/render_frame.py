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
    """Generate an SVG path-data fragment for a single short edge with finger joints.

    Path traverses from the top of the edge (at current cursor) downward
    FRAME_DEPTH_MM along Y, with fingers sticking out in POSITIVE X direction
    (use a horizontal flip for the left edge — see _left_edge_path).

    invert=False: starts as a finger sticking out
    invert=True:  starts as a slot cut in

    Uses lowercase 'l' (relative line) commands.
    """
    parts = []
    finger_depth = MATERIAL_THICKNESS_MM
    for i in range(FINGER_COUNT):
        is_finger = (i % 2 == 0) ^ invert
        if is_finger:
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            if i == 0:
                parts.append(f"l {finger_depth},0")
            parts.append(f"l 0,{pitch}")
            next_in_range = i + 1 < FINGER_COUNT
            next_is_finger = ((i + 1) % 2 == 0) ^ invert if next_in_range else None
            if next_is_finger is False:
                parts.append(f"l {-finger_depth},0")
        else:
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l 0,{pitch}")
            next_in_range = i + 1 < FINGER_COUNT
            next_is_finger = ((i + 1) % 2 == 0) ^ invert if next_in_range else None
            if next_is_finger is True:
                parts.append(f"l {finger_depth},0")

    return " ".join(parts)


def _left_edge_path(invert: bool) -> str:
    """Path along the left edge from current cursor (typically (0, FRAME_DEPTH_MM))
    back UP to (0, 0), with fingers extending in negative X direction.

    Iterates fingers in reverse order so the visual pattern matches the right edge
    when viewed in the assembled box.
    """
    parts = []
    finger_depth = MATERIAL_THICKNESS_MM
    for i in reversed(range(FINGER_COUNT)):
        is_finger = (i % 2 == 0) ^ invert
        if is_finger:
            pitch = FINGER_PITCH_MM + KERF_COMPENSATION_MM
            if i == FINGER_COUNT - 1:
                parts.append(f"l {-finger_depth},0")
            parts.append(f"l 0,{-pitch}")
            prev_in_range = i - 1 >= 0
            prev_is_finger = ((i - 1) % 2 == 0) ^ invert if prev_in_range else None
            if prev_is_finger is False:
                parts.append(f"l {finger_depth},0")
        else:
            pitch = FINGER_PITCH_MM - KERF_COMPENSATION_MM
            parts.append(f"l 0,{-pitch}")
            prev_in_range = i - 1 >= 0
            prev_is_finger = ((i - 1) % 2 == 0) ^ invert if prev_in_range else None
            if prev_is_finger is True:
                parts.append(f"l {-finger_depth},0")
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
