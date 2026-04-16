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
from typing import List, Optional, Tuple

from shapely.geometry import Polygon, box
from shapely.ops import unary_union
from shapely.validation import make_valid

# Struts (bridges) keep the inner-counter wood connected to the main face for
# letters with closed counters. Each rule is (orientation, x_frac, y_frac):
#   orientation: 'V' = vertical strut (narrow X, long Y) — crosses top/bottom of hole
#                'H' = horizontal strut (long X, narrow Y) — crosses left/right of hole
#   x_frac, y_frac: center position as fraction of hole bbox (0..1, Y-up font coords)
# Fractions < 0 or > 1 shift the strut OUTSIDE the hole center (e.g., x=0 aligns
# the strut with the left edge of the hole, which is exactly where the R/P/D/B
# struts sit — along the "left vertical line" of the letter).
BRIDGE_RULES = {
    'O': [('V', 0.5, 1.0), ('V', 0.5, 0.0)],   # 12 and 6 o'clock
    '0': [('V', 0.5, 1.0), ('V', 0.5, 0.0)],
    'Q': [('V', 0.5, 1.0), ('V', 0.5, 0.0)],
    'R': [('H', 0.0, 0.5)],                    # single strut along left vertical
    'P': [('H', 0.0, 0.5)],
    'D': [('H', 0.0, 0.5)],
    'B': [('H', 0.0, 0.5)],                    # one per counter → 2 total (B has 2 holes)
    # A's V-at-bottom strut was non-structural (upper counter island doesn't
    # connect to outside-A wood through below-crossbar area) AND caused the
    # letter to look like an arrow. Single H strut at left diagonal is
    # structurally sufficient.
    'A': [('H', 0.0, 0.5)],
    # G: previous (0.5, 0.5) was interior — strut entirely inside counter,
    # no effect on cut. Moved to left boundary to actually bridge.
    'G': [('H', 0.0, 0.5)],
    # 6 and 9: previously used V struts at bottom/top of counter. Those
    # produced a long vertical "slot" from the counter to the letter edge
    # because the counter sits close to the baseline (or cap-height for 9).
    # Switched to H strut at left-of-counter — crosses the loop's side
    # stroke, much shorter path with no visible slot.
    '6': [('H', 0.0, 0.5)],
    '9': [('H', 0.0, 0.5)],
}

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


def _extract_polygons(g):
    """Flatten any shapely geometry to a list of Polygons, discarding
    non-areal geometries (LineStrings from make_valid artifacts, etc.).
    """
    if g.is_empty:
        return []
    if g.geom_type == "Polygon":
        return [g]
    if g.geom_type == "MultiPolygon":
        return list(g.geoms)
    if g.geom_type == "GeometryCollection":
        out = []
        for sub in g.geoms:
            out.extend(_extract_polygons(sub))
        return out
    return []


def _shapely_to_svg(geom) -> str:
    parts: List[str] = []
    for poly in _extract_polygons(geom):
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


def _apply_bridges(geom, char: str, bridge_width_units: float):
    """Subtract strut rectangles from the letter polygon so the inner counter
    stays connected to the main face after cutting.

    Bridge geometry is ASYMMETRIC at boundary positions (y_frac or x_frac
    near 0 or 1): extends mostly INTO the counter, with just a small
    crossing margin outward. A symmetric bridge at the crossbar of A would
    extend equally above and below, creating a vertical stem below the A's
    crossbar that makes the letter look like an arrow. The asymmetric
    version keeps the strut notch confined to the ring it's meant to cross.

    Each hole gets each rule applied to it, so letters with multiple
    counters (B) get 1 strut per counter with the single rule.
    """
    if char not in BRIDGE_RULES or geom.is_empty:
        return geom
    polys = _extract_polygons(geom)
    interiors = [i for p in polys for i in p.interiors]
    if not interiors:
        return geom

    rules = BRIDGE_RULES[char]
    result = geom
    # Bridge has two asymmetric extents from its center point:
    #   body_margin:    how far the strut extends INTO the letter stroke/body.
    #                   Must punch all the way through so the preserved wood
    #                   reaches outside-letter face. Sized to exceed the
    #                   thickest stroke (Jost Bold stems ≈ 1.5-2mm).
    #   counter_margin: how far it extends INTO the counter. Just enough to
    #                   reach the inner island wood — going deeper creates
    #                   extra notches where the strut hits interior features
    #                   (e.g., G's inward crossbar).
    body_margin = bridge_width_units * 3     # 3mm at 1mm strut width
    counter_margin = bridge_width_units * 1.5  # 1.5mm

    for interior in interiors:
        x0, y0, x1, y1 = interior.bounds
        counter_w = x1 - x0
        counter_h = y1 - y0
        # Skip tiny slivers (CFF polygon-op artifacts) that aren't real counters.
        if min(counter_w, counter_h) < bridge_width_units:
            continue
        for orientation, x_frac, y_frac in rules:
            cx = x0 + x_frac * counter_w
            cy = y0 + y_frac * counter_h
            half_w = bridge_width_units / 2
            if orientation == "V":
                if y_frac <= 0.15:
                    # Bottom boundary: body is BELOW (outward), counter ABOVE
                    y_lo = cy - body_margin
                    y_hi = cy + counter_margin
                elif y_frac >= 0.85:
                    # Top boundary: body is ABOVE, counter BELOW
                    y_lo = cy - counter_margin
                    y_hi = cy + body_margin
                else:
                    y_lo = cy - counter_h / 4
                    y_hi = cy + counter_h / 4
                bridge = box(cx - half_w, y_lo, cx + half_w, y_hi)
            else:  # 'H'
                if x_frac <= 0.15:
                    # Left boundary: body LEFT, counter RIGHT
                    x_lo = cx - body_margin
                    x_hi = cx + counter_margin
                elif x_frac >= 0.85:
                    x_lo = cx - counter_margin
                    x_hi = cx + body_margin
                else:
                    x_lo = cx - counter_w / 4
                    x_hi = cx + counter_w / 4
                bridge = box(x_lo, cy - half_w, x_hi, cy + half_w)
            result = result.difference(bridge)
    return result


def union_glyph_path(
    path_data: str,
    char: Optional[str] = None,
    bridge_width_units: float = 0,
) -> str:
    """Union overlapping subpaths in glyph SVG path data, preserving holes.

    Winding direction (CCW vs CW) that marks outer rings vs inner counters
    differs between font formats: TrueType (e.g. Jost) uses CCW outer,
    CFF/PostScript (e.g. Fraunces) uses CW outer. We detect the convention
    per-glyph by looking at the LARGEST polygon's winding: it's always the
    outer in our use case (no glyph has an inner counter larger than its own
    outer outline). Polygons with the same winding as the largest are treated
    as outers (unioned together); the others are holes (subtracted).

    If char matches a rule in BRIDGE_RULES, strut rectangles are subtracted
    from each inner counter so counter wood stays attached after cutting.
    """
    if not path_data.strip():
        return path_data

    subpaths = _parse_svg_path(path_data)
    # Filter to valid polygons + record signed area. Self-intersecting
    # subpaths (e.g. Jost Bold B's single combined path) turn into
    # GeometryCollection after make_valid — extract the Polygon(s) from it.
    raw_polys = []
    for sub in subpaths:
        if len(sub) < 4:
            continue
        area = _signed_area(sub)
        try:
            poly = Polygon(sub)
            if not poly.is_valid:
                poly = make_valid(poly)
            for p in _extract_polygons(poly):
                if not p.is_empty:
                    raw_polys.append((area, p))
        except Exception:
            continue

    if not raw_polys:
        return path_data

    # Determine this glyph's outer-winding convention from the largest polygon
    largest = max(raw_polys, key=lambda ap: abs(ap[0]))
    outer_sign = 1 if largest[0] > 0 else -1

    exteriors = []
    holes = []
    for area, poly in raw_polys:
        if (area > 0) == (outer_sign > 0):
            exteriors.append(poly)
        else:
            holes.append(poly)

    outer = unary_union(exteriors)
    if holes:
        outer = outer.difference(unary_union(holes))

    # Normalize to pure Polygon / MultiPolygon — discards MultiLineString
    # cleanup artifacts that make_valid can leave behind (Jost B does this).
    polys = _extract_polygons(outer)
    if polys:
        outer = unary_union(polys)

    if char and bridge_width_units > 0:
        outer = _apply_bridges(outer, char, bridge_width_units)

    result = _shapely_to_svg(outer)
    return result if result else path_data
