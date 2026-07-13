"""Export through-hole pad positions from the PCB for the light channel.

The light channel sits flat on the PCB's LED side, but every through-hole
component's pins protrude a few mm on that side. light_channel.py cuts a
relief notch in its wall undersides at each pad so the channel seats on
the board surface instead of riding up on pin stubs.

Uses KiCad's bundled pcbnew Python (the .kicad_pcb S-expression format is
not stable enough to hand-parse for geometry that gates a 7-hour print).
Run with KiCad's interpreter, from repo root:

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/\
Versions/Current/bin/python3 enclosure/3d/export_tht_pads.py

Output: hardware/word-clock-tht-pads.csv (committed, like the LED
position CSV). Re-run whenever THT footprints move on the board.

A harmless `wxApp` assert prints to stderr when running headless.
"""
import csv
import sys
from pathlib import Path

sys.path.insert(
    0,
    "/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework"
    "/Versions/Current/lib/python3.9/site-packages",
)
import pcbnew  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
BOARD = REPO / "hardware" / "word-clock.kicad_pcb"
OUT = REPO / "hardware" / "word-clock-tht-pads.csv"


LED_OUT = REPO / "hardware" / "word-clock-led-pos.csv"


def main() -> None:
    board = pcbnew.LoadBoard(str(BOARD))
    rows = []
    led_rows = []
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if "WS2812" in str(fp.GetFPID().GetLibItemName()):
            pos = fp.GetPosition()
            led_rows.append((ref, round(pos.x / 1e6, 4), round(pos.y / 1e6, 4)))
        if ref.startswith("H"):
            continue  # NPTH mounting holes carry no pins
        for pad in fp.Pads():
            if pad.GetDrillSizeX() <= 0:
                continue  # SMD
            pos = pad.GetPosition()
            if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                # Daughtercard mounting holes: empty today, but clear them
                # for screw heads/standoffs so future hardware never
                # collides with the channel. Effective size = M2.5 head
                # diameter (4.5 mm — the largest hardware these 2.6-3 mm
                # drills accept), not the drill itself.
                rows.append(
                    (ref, "npth",
                     round(pos.x / 1e6, 4), round(pos.y / 1e6, 4), 4.5)
                )
                continue
            size = pad.GetSize()
            rows.append(
                (
                    ref,
                    pad.GetNumber(),
                    round(pos.x / 1e6, 4),
                    round(pos.y / 1e6, 4),
                    round(max(size.x, size.y) / 1e6, 4),
                )
            )
    rows.sort()
    led_rows.sort(key=lambda r: int(r[0][1:]))
    with OUT.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ref", "pad", "x_mm", "y_mm", "pad_size_mm"])
        w.writerows(rows)
    print(f"wrote {OUT} ({len(rows)} through-hole pads)")
    with LED_OUT.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ref", "x_mm", "y_mm"])
        w.writerows(led_rows)
    print(f"wrote {LED_OUT} ({len(led_rows)} LEDs)")


if __name__ == "__main__":
    main()
