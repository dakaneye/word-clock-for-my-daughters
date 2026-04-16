"""Boolean-union overlapping subpaths of a glyph into clean outer outlines.

Jost Bold (and many geometric sans fonts) build letters from overlapping
primitives: E = stem rectangle + 3 bar rectangles, H = 2 stems + crossbar.
When these are extracted via fontTools.SVGPathPen, each primitive is a
separate subpath (separate M...Z). For FILLED rendering this is invisible
(overlapping fill areas blend). For LASER CUTTING each subpath becomes
an independent cut — producing the letter as fragments, not a single shape.

This module unions the overlapping subpaths into a single outline (with
holes for inner counters like O, A, B, P, R). The result is a clean cut
path: one exterior ring per connected component, plus an interior ring
per hole.
"""
from __future__ import annotations

import re
from typing import List, Tuple

from shapely.geometry import Polygon
from shapely.ops import unary_union
from shapely.validation import make_valid

# Match either a single command letter or a signed decimal number.
_TOKEN_RE = re.compile(r"[MLHVQCZmlhvqcz]|-?\d+\.?\d*(?:[eE]-?\d+)?")

# How many line segments to flatten a Bezier curve into. 16 is more than
# enough for 10mm-tall letters at laser-cut precision (<0.1mm chord error).
_CURVE_STEPS = 16


def _parse_svg_path(path_data: str) -> List[List[Tuple[float, float]]]:
    """Parse an absolute-coordinate SVG path into a list of subpaths.

    Each subpath is a list of (x, y) tuples, with the first point repeated
    at the end for closed rings (so Shapely accepts them as polygons).
    Flattens Q/C Bezier curves into line segments. Handles implicit
    continuation after M and L.
    """
    tokens = _TOKEN_RE.findall(path_data)
    subpaths: List[List[Tuple[float, float]]] = []
    current: List[Tuple[float, float]] = []
    cx = cy = 0.0
    cmd = None
    i = 0

    def flush() -> None:
        nonlocal current
        if current:
            if current[0] != current[-1]:
                current.append(current[0])
            subpaths.append(current)
            current = []

    while i < len(tokens):
        t = tokens[i]
        if t.isalpha():
            cmd = t
            i += 1
            if cmd in ("Z", "z"):
                flush()
                continue
        # Consume data pairs based on current command
        if cmd == "M":
            x = float(tokens[i]); y = float(tokens[i + 1]); i += 2
            flush()
            current.append((x, y))
            cx, cy = x, y
            cmd = "L"  # implicit continuation after M is L
        elif cmd == "L":
            x = float(tokens[i]); y = float(tokens[i + 1]); i += 2
            current.append((x, y))
            cx, cy = x, y
        elif cmd == "H":
            x = float(tokens[i]); i += 1
            current.append((x, cy))
            cx = x
        elif cmd == "V":
            y = float(tokens[i]); i += 1
            current.append((cx, y))
            cy = y
        elif cmd == "Q":
            qx = float(tokens[i]); qy = float(tokens[i + 1])
            x = float(tokens[i + 2]); y = float(tokens[i + 3])
            i += 4
            x0, y0 = cx, cy
            for step in range(1, _CURVE_STEPS + 1):
                tt = step / _CURVE_STEPS
                one_minus = 1 - tt
                bx = one_minus * one_minus * x0 + 2 * one_minus * tt * qx + tt * tt * x
                by = one_minus * one_minus * y0 + 2 * one_minus * tt * qy + tt * tt * y
                current.append((bx, by))
            cx, cy = x, y
        elif cmd == "C":
            c1x = float(tokens[i]); c1y = float(tokens[i + 1])
            c2x = float(tokens[i + 2]); c2y = float(tokens[i + 3])
            x = float(tokens[i + 4]); y = float(tokens[i + 5])
            i += 6
            x0, y0 = cx, cy
            for step in range(1, _CURVE_STEPS + 1):
                tt = step / _CURVE_STEPS
                om = 1 - tt
                bx = om**3 * x0 + 3 * om**2 * tt * c1x + 3 * om * tt**2 * c2x + tt**3 * x
                by = om**3 * y0 + 3 * om**2 * tt * c1y + 3 * om * tt**2 * c2y + tt**3 * y
                current.append((bx, by))
            cx, cy = x, y
        elif cmd == "m":
            x = float(tokens[i]); y = float(tokens[i + 1]); i += 2
            flush()
            cx += x; cy += y
            current.append((cx, cy))
            cmd = "l"
        elif cmd == "l":
            x = float(tokens[i]); y = float(tokens[i + 1]); i += 2
            cx += x; cy += y
            current.append((cx, cy))
        else:
            # Unrecognized; skip token to avoid infinite loop
            i += 1

    flush()
    return subpaths


def _ring_to_svg(coords) -> str:
    pts = list(coords)
    if not pts:
        return ""
    out = [f"M{pts[0][0]},{pts[0][1]}"]
    for x, y in pts[1:]:
        out.append(f"L{x},{y}")
    out.append("Z")
    return " ".join(out)


def _shapely_to_svg(geom) -> str:
    parts: List[str] = []
    if geom.is_empty:
        return ""
    if geom.geom_type == "Polygon":
        parts.append(_ring_to_svg(geom.exterior.coords))
        for interior in geom.interiors:
            parts.append(_ring_to_svg(interior.coords))
    elif geom.geom_type == "MultiPolygon":
        for poly in geom.geoms:
            parts.append(_ring_to_svg(poly.exterior.coords))
            for interior in poly.interiors:
                parts.append(_ring_to_svg(interior.coords))
    return " ".join(parts)


def _signed_area(points: List[Tuple[float, float]]) -> float:
    """Shoelace-formula signed area. Positive = CCW, negative = CW (for Y-up coords).

    fontTools emits glyph outlines in font-native Y-up coordinates where CCW
    marks outer rings and CW marks interior holes (counters).
    """
    n = len(points)
    s = 0.0
    for i in range(n):
        x1, y1 = points[i]
        x2, y2 = points[(i + 1) % n]
        s += x1 * y2 - x2 * y1
    return s / 2


def union_glyph_path(path_data: str) -> str:
    """Union overlapping subpaths in glyph SVG path data, preserving holes.

    CCW subpaths are treated as filled regions (letter bodies); CW subpaths
    are treated as holes (inner counters). The result is
    (union of CCW regions) minus (union of CW regions).

    Returns cleaned SVG path data with one subpath per exterior plus one per
    interior hole. Original path_data is returned unchanged if parsing yields
    no valid polygons.
    """
    if not path_data.strip():
        return path_data

    subpaths = _parse_svg_path(path_data)
    exteriors = []
    holes = []
    for sub in subpaths:
        if len(sub) < 4:  # need at least 3 unique points + closing repeat
            continue
        area = _signed_area(sub)
        try:
            poly = Polygon(sub)
            if not poly.is_valid:
                poly = make_valid(poly)
            if poly.is_empty or poly.geom_type not in ("Polygon", "MultiPolygon"):
                continue
        except Exception:
            continue
        if area > 0:
            exteriors.append(poly)
        else:
            holes.append(poly)

    if not exteriors:
        return path_data

    outer = unary_union(exteriors)
    if holes:
        hole_union = unary_union(holes)
        outer = outer.difference(hole_union)

    result = _shapely_to_svg(outer)
    return result if result else path_data
