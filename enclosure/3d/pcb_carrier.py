"""One-piece carrier plate — glues to the back panel interior and carries
every internal mount: PCB standoff posts, speaker-spacer collars, a button-cap
retention pad, and a cable cleat. Replaces the four loose standoff posts and
the glue-spacers-to-wood step; nothing needs measuring at glue-up.

Registration: four ⌀2.2 holes match the panel's speaker-vent grid corners —
drop ⌀2 drill bits through plate + vent holes at glue-up and pull them after.
The cable hole is NOT a reference (it was placed for the old ESP32 position).

Button retention: per-cap flanged wells are geometrically impossible at the
10.0 mm button pitch (⌀9 flanges need walls that would overlap — proven in
the slicer 2026-07-10), but a shared pocket under a roof with per-cap SNAP
HOLES retains all three caps AND keeps them interchangeable: each roof hole
is sized so the cap's ⌀9 flange pops through under a firm push (the flange's
45° cone cams it in; a matching removal chamfer on the flange top cams it
out). Snapped in, the flange rides captive between the panel (the pocket
floor — the plate is glued to it) and the snap lip above. To swap a cap
(board off the posts), press its neck from the panel EXTERIOR and it pops up
through the roof into your hand. Caps snap in after glue-up, so any glue
works, thin CA included. The switch bottoms out 0.6 mm before the flange
reaches the roof, so no press can self-eject a cap. Snap-hole diameter is
FDM-calibrated: print --part snap-test (holes ⌀8.6/8.9/9.2 at the real
10 mm pitch) and set SNAP_HOLE_D_MM to the size that snaps firmly but
releases under a deliberate push.

Button guidance (bench 2026-07-12 — caps too loose, hard to line up with
the switches): a GUIDE TOWER rises from the pad roof to z=23.5 with a
PER-CAP BORE — each cap is a piston in its own ⌀9 compartment, held at
three heights (neck in panel hole, flange in pocket, anvil disc in bore)
and standing rigid directly under its switch. Compartments became
printable at the 10 mm pitch (1.0 mm webs) by shrinking the cap: flange
⌀8 (still can't pass the 6.5 panel hole), anvil ⌀8.2 (still covers
±2.0 mm of plunger offset — 4× what a THT switch's pin-in-hole fit
allows; the "~3 mm switch offset" once measured came from the mis-built
early coupon, not the switch). Bore mouths are chamfered to funnel
snap-in. The tower stops 1 mm below the switches rather than wrapping
their bodies — the pins exit flush against two body faces, so socket
walls there would land on solder joints; the bore guides the anvil to
within ±0.4 of nominal, which the anvil diameter absorbs. Snap caps in
with a pencil-end; removal unchanged (press the neck from the exterior,
tip the panel, cap slides out).

The PCB then self-locates on the plate: each post carries a 3.0 mm stub that
enters the board's non-plated corner mounting holes H1–H4.

Speaker: the brass M3 spacers drop into hex bores that pass through the
plate, so each spacer stands on the WOOD under screw load (no glue, and the
hex prevents rotation when driving speaker screws).

COORDINATE FRAME / CHIRALITY (institutional lesson from the light channel):
built in the PANEL-INTERIOR view = kicad (x, y) + BORDER on both axes, NO
mirror. Proof: the outside-view panel SVG maps pcb→panel as
(192 − (x+7.1), y+7.1) (render_back_panel.py); flipping the physical panel
interior-up about its vertical axis gives x_in = 192 − x_out = x+7.1,
y_in = y+7.1. A part modeled in that frame and printed glue-face-down lands
feature-on-feature — verified by the coupon against the physical panel's
button holes before the full plate prints.

Stack (unchanged from the loose-post design):
    plate web 2.5 + post 25.9 = 28.4 panel→PCB underside; +1.6 PCB +
    17.84 channel + 0.16 film = 48.0 = the frame cavity (face on top of the
    walls, panel underneath — corrected 2026-07-09).

Parts:
    --part plate       full carrier (print PETG, glue-face down, brim,
                       supports OFF)
    --part coupon      button-corner crop: post H2 + the full retention pad
                       — snap three caps in, verify seat + press feel
    --part tower-test  calibration tile with three hex bores
    --part snap-test   calibration strip with three snap-hole sizes

Sources of truth:
    H1–H4            pcbnew dump 2026-07-07 (word-clock.kicad_pcb)
    SW1–SW3          footprint origins, stable across revs (verified)
    vent center      render_back_panel.py SPEAKER_VENT_CENTER (outside view)
    cap geometry     button_cap.py (flange ⌀8×1.9 incl. removal chamfer,
                     stem ⌀5, anvil ⌀8.2, neck ⌀6.2)
    module keep-outs daughtercards.md calipered outlines + pcbnew positions
"""
import argparse
from pathlib import Path

from build123d import (Align, Box, Compound, Cone, Cylinder, Pos,
                       RegularPolygon, Rot, ShapeList, extrude, export_stl)

# ─── Frame ───────────────────────────────────────────────────────
BORDER_MM = 7.1                      # pcb frame → panel frame, both axes
PANEL_MM = 192.0


def IN(x_kicad: float, y_kicad: float):
    """kicad board coords → panel-interior coords.
    BENCH-CORRECTED 2026-07-08: the interior frame is X-MIRRORED across the
    panel centerline (the coupon proved the un-mirrored frame wrong — wells
    missed the panel's button holes). Interior coords == the outside-view
    SVG coords, so panel-native features (vent, cable hole) use their
    render_back_panel.py values directly."""
    return (PANEL_MM - (x_kicad + BORDER_MM), y_kicad + BORDER_MM)


# ─── Locked geometry ─────────────────────────────────────────────
WEB_T_MM         = 2.5               # plate thickness
POST_H_MM        = 25.9              # + web = 28.4 to PCB underside.
                                     # STACK CORRECTION 2026-07-09: the frame
                                     # cavity is the FULL 48.0 (face glues ON
                                     # TOP of the walls, panel screws UNDER
                                     # them); the old 22.0 figure came from a
                                     # stack table that wrongly counted the
                                     # face+panel (6.4) inside the 48.0.
                                     # 28.4 + PCB 1.6 + channel 17.84 +
                                     # film 0.16 = 48.0 exactly. Bench-found
                                     # by the user (~5 mm gap), twice.
POST_D_MM        = 6.0
POST_BASE_D_MM   = 12.0
STUB_D_MM        = 3.0               # enters H1–H4 (⌀3.2 NPTH)
STUB_H_MM        = 2.0

# Board corner mounting holes, kicad frame (pcbnew 2026-07-07; note H1 y=5.5)
H_HOLES = [(5.0, 5.0), (172.3, 5.5), (5.0, 172.3), (172.3, 172.3)]

# Buttons: footprint origins, kicad frame (stable across revs)
SW = {"SW3": (168.5, 19.5), "SW1": (168.5, 29.5), "SW2": (168.5, 39.5)}

# Button-cap retention pad: shared flange pocket + per-cap snap holes in
# the roof (see module docstring). Stroke check: flange section 2.5 in a
# 5.0 pocket = 2.5 mm max stroke; click needs 0.55 (0.3 at-rest gap + 0.25
# switch travel) and the switch bottoms out at 1.9, leaving the flange
# 0.6 mm shy of the roof — no press can reach the snap lip.
PAD_T_MM         = 7.0               # local plate thickness over the pad
PAD_SLOT_D_MM    = 5.0               # flange pocket depth (roof = 2.0)
PAD_SLOT_W_MM    = 11.0              # pocket width for the ⌀9 flange
PAD_WALL_MM      = 2.5               # pad wall beyond pocket, all sides
SNAP_HOLE_D_MM   = 7.9               # ⌀8 flange pops through under a firm
                                     # push; calibrate with --part snap-test
                                     # (FDM holes print ~0.35 undersize)
SNAP_TEST_DIAS   = (7.6, 7.9, 8.2)
CAP_ANVIL_D_MM   = 8.2               # keep in sync with button_cap.py
TOWER_TOP_Z_MM   = 23.5              # guides the anvil (disc rides 21.4-
                                     # 23.5 at rest); 0.9 below worst-case
                                     # clipped switch-pin tips, 2.2 below
                                     # switch bodies
TOWER_BORE_D_MM  = 9.0               # per-cap compartment: anvil ⌀8.2 →
                                     # ±0.4 nominal (~±0.2 printed); also
                                     # passes the ⌀8 flange at snap-in.
                                     # 1.0 mm webs between bores at the
                                     # 10 mm pitch
TOWER_LEADIN_D_MM = 9.8              # bore-mouth chamfer top diameter
TOWER_LEADIN_H_MM = 0.4
TOWER_WALL_MM    = 2.4

# Speaker: vent center from render_back_panel.py, outside view (55,150)
# → interior view x = 192−55 = 137.  Spacers 37.0 apart, horizontal.
VENT_CENTER_IN   = (55.0, 150.0)
SPACER_SPAN_MM   = 37.0
SPACER_AF_MM     = 4.5               # measured across-flats 2026-07-09
COLLAR_BORE_AF   = 4.90              # tile-calibrated 2026-07-09 (4.5 AF
                                     # spacer fit the 4.90 bore)
COLLAR_D_MM      = 12.0
COLLAR_H_MM      = 5.0               # above web; bore passes through web too
VENT_CLEAR_D_MM  = 17.0              # open window over the 16 mm vent grid
REG_HOLE_D_MM    = 2.2               # glue-up registration: drop 2 mm drill
                                     # bits through these into the panel's
                                     # vent holes; pull them after

# Cable cleat: on the bottom web along the ESP32→exit run (interior view).
CLEAT_POS_IN     = (129.0, 158.0)
CLEAT_GAP_MM     = 4.4               # jacket measured ⌀4.5 (2026-07-12);
                                     # light grip. The original 3.6 was a
                                     # guess and too small on the bench.
CLEAT_NUB_MM     = 0.6               # per-side snap nub; top opening =
                                     # gap − 2×nub = 3.2 — the 4.5 jacket
                                     # squeezes past and seats
CLEAT_H_MM       = 8.0

# Grommet + cable clearance at the panel's USB exit hole. The grommet's
# wide flange sits on the panel EXTERIOR; the interior side still sees its
# sleeve + retaining lip through the ⌀16 hole, and the plate's bottom ring
# band (y ≥ 170.9) overlaps that footprint — Emory's plate was hand-cut on
# the bench (2026-07-12) to clear it. This puts the clearance in CAD.
CABLE_EXIT_IN    = (165.0, 170.0)    # render_back_panel.py
                                     # USB_CABLE_EXIT_CENTER (panel-native:
                                     # interior == outside-view values)
GROMMET_CLEAR_D_MM = 22.0            # ⌀16 hole + lip + margin

# Keep-out windows (kicad frame, conservative, from daughtercards.md +
# pcbnew): the skeleton must not put material under these.
KEEPOUTS = [
    (19.0, 40.0, 48.0, 87.0,  "SD card + socket"),
    (62.0, 105.0, 89.0, 151.0, "RTC card + battery"),
    (126.0, 60.0, 152.0, 91.0, "AMP card"),
    (74.0, 36.0, 106.0, 106.0, "ESP32 + USB plug"),
    (103.0, 82.0, 122.0, 98.0, "C2 electrolytic"),
    (160.0, 14.0, 177.8, 45.0, "SW bodies (retention pad only, 6 mm tall)"),
]

BOTTOM = (Align.MIN, Align.MIN, Align.MIN)
CENTER_B = (Align.CENTER, Align.CENTER, Align.MIN)


def _fuse(shape):
    if isinstance(shape, ShapeList):
        return Compound(list(shape))
    return shape


def _rect(x0, y0, x1, y1, h, z0=0.0):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    return Pos(x0, y0, z0) * Box(x1 - x0, y1 - y0, h, align=BOTTOM)


def build_plate(crop=None):
    """Full carrier skeleton. crop: (x0,y0,x1,y1) interior-frame window
    for coupons."""
    a, b = IN(0, 0), IN(177.8, 177.8)
    pcb0 = (min(a[0], b[0]), min(a[1], b[1]))
    pcb1 = (max(a[0], b[0]), max(a[1], b[1]))
    ring_w = 14.0

    webs = []
    # perimeter ring (matches the PCB footprint; frame interior clears it)
    webs.append(_rect(pcb0[0], pcb0[1], pcb1[0], pcb0[1] + ring_w, WEB_T_MM))
    webs.append(_rect(pcb0[0], pcb1[1] - ring_w, pcb1[0], pcb1[1], WEB_T_MM))
    webs.append(_rect(pcb0[0], pcb0[1], pcb0[0] + ring_w, pcb1[1], WEB_T_MM))
    webs.append(_rect(pcb1[0] - ring_w, pcb0[1], pcb1[0], pcb1[1], WEB_T_MM))
    # speaker web: horizontal band carrying the two collars + cleat
    webs.append(_rect(pcb0[0], 139.0, pcb1[0], 162.0, WEB_T_MM))
    # stiffening web in the free lane between RTC and AMP (kicad 111.9-124.9)
    webs.append(_rect(IN(111.9, 0)[0], pcb0[1], IN(124.9, 0)[0], pcb1[1],
                      WEB_T_MM))

    plate = webs[0]
    for w in webs[1:]:
        plate = _fuse(plate + w)

    # keep-out enforcement: subtract every window (belt & suspenders — the
    # layout above avoids them; this guarantees it)
    for (kx0, ky0, kx1, ky1, _n) in KEEPOUTS:
        x0, y0 = IN(kx0, ky0)
        x1, y1 = IN(kx1, ky1)
        plate = _fuse(plate - _rect(x0, y0, x1, y1, WEB_T_MM + 20.2, -0.1))

    # posts at H1–H4
    for (hx, hy) in H_HOLES:
        cx, cy = IN(hx, hy)
        base = Pos(cx, cy, 0) * Cylinder(POST_BASE_D_MM / 2, WEB_T_MM + 2.0,
                                         align=CENTER_B)
        post = Pos(cx, cy, 0) * Cylinder(POST_D_MM / 2, WEB_T_MM + POST_H_MM,
                                         align=CENTER_B)
        stub = Pos(cx, cy, WEB_T_MM + POST_H_MM) * Cylinder(
            STUB_D_MM / 2, STUB_H_MM, align=CENTER_B)
        plate = _fuse(plate + base + post + stub)

    # Button-cap retention pad: one shared flange pocket (closed all
    # around; the glued panel is its floor) under a roof with a snap hole
    # per cap (module docstring).
    sw_ys = sorted(IN(*p)[1] for p in SW.values())     # 26.6, 36.6, 46.6
    bx = IN(*SW["SW1"])[0]                             # 16.4, shared x
    slot_y0 = sw_ys[0] - PAD_SLOT_W_MM / 2             # 21.1
    slot_y1 = sw_ys[-1] + PAD_SLOT_W_MM / 2            # 52.1
    pad = _rect(bx - PAD_SLOT_W_MM / 2 - PAD_WALL_MM,
                slot_y0 - PAD_WALL_MM,
                bx + PAD_SLOT_W_MM / 2 + PAD_WALL_MM,
                slot_y1 + PAD_WALL_MM, PAD_T_MM)
    plate = _fuse(plate + pad)
    pocket = _rect(bx - PAD_SLOT_W_MM / 2, slot_y0,
                   bx + PAD_SLOT_W_MM / 2, slot_y1,
                   PAD_SLOT_D_MM + 0.1, z0=-0.1)
    plate = _fuse(plate - pocket)
    for sy in sw_ys:
        snap = Pos(bx, sy, PAD_SLOT_D_MM - 0.1) * Cylinder(
            SNAP_HOLE_D_MM / 2, PAD_T_MM - PAD_SLOT_D_MM + 0.2,
            align=CENTER_B)
        plate = _fuse(plate - snap)
    # guide tower on the roof: a per-cap bore compartment for each anvil
    tower = _rect(bx - TOWER_BORE_D_MM / 2 - TOWER_WALL_MM,
                  sw_ys[0] - TOWER_BORE_D_MM / 2 - TOWER_WALL_MM,
                  bx + TOWER_BORE_D_MM / 2 + TOWER_WALL_MM,
                  sw_ys[-1] + TOWER_BORE_D_MM / 2 + TOWER_WALL_MM,
                  TOWER_TOP_Z_MM - PAD_T_MM, z0=PAD_T_MM)
    plate = _fuse(plate + tower)
    for sy in sw_ys:
        bore = Pos(bx, sy, PAD_T_MM - 0.1) * Cylinder(
            TOWER_BORE_D_MM / 2, TOWER_TOP_Z_MM - PAD_T_MM + 0.2,
            align=CENTER_B)
        plate = _fuse(plate - bore)
        leadin = Pos(bx, sy, TOWER_TOP_Z_MM - TOWER_LEADIN_H_MM) * Cone(
            TOWER_BORE_D_MM / 2, TOWER_LEADIN_D_MM / 2,
            TOWER_LEADIN_H_MM + 0.1, align=CENTER_B)
        plate = _fuse(plate - leadin)

    # speaker collars: hex bore through web + collar; spacer stands on wood
    for sgn in (-1, 1):
        cx = VENT_CENTER_IN[0] + sgn * SPACER_SPAN_MM / 2
        cy = VENT_CENTER_IN[1]
        collar = Pos(cx, cy, 0) * Cylinder(
            COLLAR_D_MM / 2, WEB_T_MM + COLLAR_H_MM, align=CENTER_B)
        plate = _fuse(plate + collar)
        hexbore = Pos(cx, cy, -0.1) * extrude(
            RegularPolygon(COLLAR_BORE_AF / 2, 6, major_radius=False),
            WEB_T_MM + COLLAR_H_MM + 0.2)
        plate = _fuse(plate - hexbore)
    # open window over the vent grid
    vent = Pos(*VENT_CENTER_IN, -0.1) * Cylinder(
        VENT_CLEAR_D_MM / 2, WEB_T_MM + 0.2, align=CENTER_B)
    plate = _fuse(plate - vent)
    # registration holes at the four vent-grid corner holes (±8, ±8)
    for dx in (-8.0, 8.0):
        for dy in (-8.0, 8.0):
            rh = Pos(VENT_CENTER_IN[0] + dx, VENT_CENTER_IN[1] + dy, -0.1) \
                * Cylinder(REG_HOLE_D_MM / 2, WEB_T_MM + 0.2, align=CENTER_B)
            plate = _fuse(plate - rh)

    # cable cleat: two fingers with inward snap nubs on the speaker web
    ccx, ccy = CLEAT_POS_IN
    for sgn in (-1, 1):
        fx = ccx + sgn * (CLEAT_GAP_MM / 2)          # inner face of finger
        finger = Pos(min(fx, fx + sgn * 2.5), ccy - 5.0, WEB_T_MM) * Box(
            2.5, 10.0, CLEAT_H_MM, align=BOTTOM)
        nub = Pos(min(fx, fx - sgn * CLEAT_NUB_MM), ccy - 5.0,
                  WEB_T_MM + CLEAT_H_MM - 2.0) * Box(
            CLEAT_NUB_MM, 10.0, 2.0, align=BOTTOM)
        plate = _fuse(plate + finger + nub)

    # grommet + cable clearance at the panel's USB exit hole
    gx, gy = CABLE_EXIT_IN
    grommet = Pos(gx, gy, -0.1) * Cylinder(
        GROMMET_CLEAR_D_MM / 2, WEB_T_MM + 0.2, align=CENTER_B)
    plate = _fuse(plate - grommet)

    # orientation chamfer at the H1 (kicad 0,0) corner of the ring
    cc = IN(0, 0)
    ch = Pos(cc[0], cc[1], -0.1) * Rot(Z=45) * Box(
        6.0, 6.0, WEB_T_MM + POST_H_MM + 5,
        align=(Align.CENTER, Align.CENTER, Align.MIN))
    plate = _fuse(plate - ch)

    if crop:
        x0, y0, x1, y1 = crop
        window = _rect(x0, y0, x1, y1, 60.0, -20.0)
        plate = _fuse(plate & window)
    return plate


def export_main_solid(shape, path: str) -> None:
    items = list(shape) if isinstance(shape, (list, tuple)) else [shape]
    solids = sorted((s for it in items for s in it.solids()),
                    key=lambda s: s.volume, reverse=True)
    dropped = sum(s.volume for s in solids[1:])
    if dropped > 100.0:
        raise AssertionError(
            f"part split into large disconnected bodies ({dropped:.0f} mm³)")
    if len(solids) > 1:
        print(f"  note: dropped {len(solids)-1} sliver(s), {dropped:.1f} mm³")
    export_stl(solids[0], path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--part",
                    choices=["plate", "coupon", "tower-test", "snap-test"],
                    required=True)
    args = ap.parse_args()
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)

    if args.part == "plate":
        part = build_plate()
        path = out / "pcb_carrier_plate.stl"
    elif args.part == "coupon":
        # button-corner strip: post H2 + the complete retention pad
        xa, xb = IN(150.0, 0)[0], IN(177.8, 0)[0]
        x0, x1 = min(xa, xb), max(xa, xb)
        y0, y1 = IN(0, 0)[1], IN(0, 177.8)[1]
        # The pad bridges the SW keep-out gap, so the strip stays one body.
        part = build_plate(crop=(x0 - 0.1, y0 - 0.1, x1 + 0.1, y0 + 50.0))
        # Orientation tab (additive — cuts sever cropped strips): points at
        # the panel corner NEAREST THE BUTTONS (interior view: top-right,
        # with the cable hole at bottom-left).
        bx = IN(*SW["SW3"])[0]
        tab_x = x0 - 6.0 if abs(bx - x0) < abs(bx - x1) else x1 - 2.0
        tab = Pos(tab_x, y0 - 5.0, 0) * Box(8.0, 8.0, 3.0, align=BOTTOM)
        part = _fuse(part + tab)

        path = out / "pcb_carrier_coupon.stl"
    elif args.part == "tower-test":
        # calibration tile: three hex bores around the nominal M2 AF
        tile = Box(48, 16, WEB_T_MM + COLLAR_H_MM, align=BOTTOM)
        part = tile
        for i, af in enumerate((4.60, 4.75, 4.90)):
            bore = Pos(8 + i * 16, 8, -0.1) * extrude(
                RegularPolygon(af / 2, 6, major_radius=False),
                WEB_T_MM + COLLAR_H_MM + 0.2)
            part = _fuse(part - bore)
        path = out / "pcb_carrier_tower_test.stl"
    else:
        # snap calibration strip: the real pad section (pocket + roof) with
        # one snap hole of each candidate size at the REAL 10 mm pitch, so
        # the web compliance between holes matches the plate. Snap a cap
        # into each; pick the one that clicks in firmly and pops out under
        # a deliberate push on the neck. Print like the plate: flat face
        # down, no supports (the roof bridges the 11 mm pocket — fine).
        n = len(SNAP_TEST_DIAS)
        half_w = PAD_SLOT_W_MM / 2
        y_last = (n - 1) * 10.0
        part = _rect(-half_w - PAD_WALL_MM, -half_w - PAD_WALL_MM,
                     half_w + PAD_WALL_MM, y_last + half_w + PAD_WALL_MM,
                     PAD_T_MM)
        part = _fuse(part - _rect(-half_w, -half_w, half_w, y_last + half_w,
                                  PAD_SLOT_D_MM + 0.1, z0=-0.1))
        for i, d in enumerate(SNAP_TEST_DIAS):
            hole = Pos(0, i * 10.0, PAD_SLOT_D_MM - 0.1) * Cylinder(
                d / 2, PAD_T_MM - PAD_SLOT_D_MM + 0.2, align=CENTER_B)
            part = _fuse(part - hole)
        # one corner chamfer marks the SMALLEST hole's end
        mark = Pos(-half_w - PAD_WALL_MM, -half_w - PAD_WALL_MM, -0.1) \
            * Rot(Z=45) * Box(5.0, 5.0, PAD_T_MM + 0.2,
                              align=(Align.CENTER, Align.CENTER, Align.MIN))
        part = _fuse(part - mark)
        path = out / "pcb_carrier_snap_test.stl"

    export_main_solid(part, str(path))
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
