# 3D Printing Setup — Bambu Lab A1

End-to-end setup for the word-clock 3D-printed parts. First-time use of the
Bambu A1; this doc captures the path from "box on desk" to "first usable
print in hand."

**Already installed on this machine (2026-04-16):**
- Bambu Studio slicer (`/Applications/BambuStudio.app`, via Homebrew)
- `build123d` Python CAD library (`enclosure/3d/.venv/`)
- `ocp-vscode` Python package + **OCP CAD Viewer** VS Code extension
  (live 3D preview of build123d parts without exporting STL)

**Scripts ready to print:**
- `enclosure/3d/pcb_standoff.py` → `out/pcb_standoff.stl` (×4 needed)
- (more parts land here as measurements unblock them)

---

## Printer onboarding (first time only, ~30 min)

When the A1 arrives, follow Bambu's official unboxing quick-start first
(the printed card in the box). It covers physical setup — remove shipping
bolts, attach the AMS lite if you ordered one, plug it in.

Then from the printer's on-screen wizard:

1. **Connect to WiFi.** 2.4 GHz only on the A1 — same band-constraint as
   the word clock itself. Use the home network you'll expect the clock to
   use.
2. **Calibrate.** The A1 runs an auto-calibration routine on first power:
   vibration compensation, flow rate, bed tilt. ~15 min. Don't skip;
   skipping shows up as layer shifts later.
3. **Create a Bambu account** in the printer UI (email + password). Needed
   to link Bambu Studio to the printer over the network.
4. **Print the test cube** from the onboard SD card to verify everything
   works before trying our custom parts.

---

## Bambu Studio first launch

1. Open `/Applications/BambuStudio.app`.
2. Sign in with the same Bambu account you used on the printer.
3. `Device` tab → the A1 should appear under "My Devices". Click to bind.
4. `Preference` → confirm:
   - Units: mm (default)
   - `Auto-Check Update`: on (keeps slicer current with printer firmware)
   - `Enable network plugin`: yes (needed to send prints to the printer)
5. Load a filament profile:
   - Click the filament slot → "Bambu PLA Basic" (generic PLA is fine for
     every internal clock part).
   - Scan the RFID spool (if Bambu branded) or tell the AMS manually what
     color/type is loaded.

---

## Printing a word-clock part

Standoff posts are the first parts needed. The same flow applies to every
future part in `enclosure/3d/`.

1. **Regenerate the STL** (always — parameters may have changed):

   ```bash
   cd ~/dev/personal/word-clock-for-my-daughters
   enclosure/3d/.venv/bin/python enclosure/3d/pcb_standoff.py
   # or build all at once:
   enclosure/3d/.venv/bin/python enclosure/3d/build_all.py
   ```

2. **Open Bambu Studio.**
3. `File → Import → Import 3MF/STL` → pick `enclosure/3d/out/pcb_standoff.stl`.
4. **Orient the part.** For the standoff: right-click the part in the 3D
   view → "Auto Orient" → Bambu Studio picks the flattest face (the
   flange bottom) to sit on the bed. This is ideal for the standoff —
   supports won't be needed.
5. **Check placement.** Part should be in the build plate's center-ish.
   The default is fine.
6. **Duplicate to print 4 at once.** Right-click → Clone → 4 copies.
   Bambu Studio auto-arranges them on the bed.
7. **Slice settings for small mechanical parts:**
   - Layer height: 0.2 mm (default — fine)
   - Infill: 30-50% (standoffs take compression; over 30 is fine)
   - Supports: off (auto-orient avoided overhangs)
   - Filament: Bambu PLA Basic, 220°C (default)
8. Click **Slice**. Preview the toolpath (right panel). Rough sanity
   check: time and filament estimates reasonable (~20 min and ~5 g for 4
   standoffs).
9. Click **Send** (or Print on network). The A1 heats up and starts.

**Expected output:** 4 standoff posts, ~20 min. Each ~24 mm tall, flange
at the bottom, small locating stub on top.

---

## VS Code live-preview workflow (optional but very nice)

Instead of exporting STL + reopening in Bambu Studio every time you tweak
a parameter, you can preview in VS Code live:

1. Open the repo in VS Code: `code ~/dev/personal/word-clock-for-my-daughters`.
2. `⇧⌘P` → "OCP CAD: Start viewer". A split-pane 3D preview opens.
3. Open `enclosure/3d/pcb_standoff.py`.
4. At the bottom of the script (before exit), add:
   ```python
   from ocp_vscode import show
   show(standoff)
   ```
5. Run the script (via the venv python). The viewer updates live.
6. Tweak a parameter, re-run, see the new geometry immediately.

Remove the `show()` call before committing; it's a dev-loop tool, not
something the STL export needs.

---

## Troubleshooting

### First layer is stringy or doesn't stick
- Re-level the bed via the A1's onboard routine (`Calibration` menu).
- Wipe the PEI bed with isopropyl alcohol; finger oils ruin adhesion.

### Parts warp at corners
- PLA at default settings shouldn't warp on an A1 (enclosed hotend is
  forgiving). If it happens: print on a brim (Bambu Studio → Supports
  → Brim: 3 mm).

### Standoff stub doesn't fit the PCB hole
- Too tight: open `pcb_standoff.py`, drop `STUB_DIAMETER` from 2.9 to
  2.85 (or 2.8), re-run, reprint one as a test.
- Too loose: go to 3.0. PLA prints 0.1-0.2 mm oversize on tight
  diameters; the 2.9 target assumes typical A1 accuracy.

### Network send to printer fails
- Check both the Mac and the printer are on the same 2.4 GHz SSID.
- If still failing: use "Export G-code to SD card" instead, carry the
  SD card to the printer.

### STL opens but looks like a mess of triangles
- Export worked but the part wasn't fused. Re-check that `standoff = base
  + post + stub` produces a single solid in build123d (the `+` operator
  unions; if you accidentally use `Compound`, parts stay separate).

---

## Filament considerations

Parts and their filament recommendations:

| Part | Filament | Why |
|---|---|---|
| PCB standoff post | PLA (any brand) | Non-structural, room-temp, no UV |
| Button actuator cap | PLA | Same |
| Speaker cradle | PLA (or PETG for vibration isolation) | PLA is fine; PETG damps vibration better if you have it |
| Light channel | **Black PLA, not white** | Optical isolator — see `docs/superpowers/specs/2026-04-15-laser-cut-face-design.md` §Light channel material override. White PLA leaks light between pockets. |

Filament not yet ordered? Bambu PLA Basic black + one of (white/cream/any
color) covers every part. 1 kg of each is more than enough for both
clocks' internals with plenty of room for calibration prints and mistakes.

**Not printed:** the diffuser stack (adhesive-backed diffusion film +
2-3 mm opal cast acrylic) is purchased, not 3D-printed. Sourcing is
tracked in `TODO.md` §Supplies still to order. An earlier PETG-print
iteration was rejected because Chelsea's 2015 clock used PETG and still
shows LED hot spots.

---

## What's blocked on physical parts arriving

These parts can't be designed until components are in hand and
caliper-measured:

- **Button actuator cap** — needs tact switch plunger height + back panel
  thickness at hole location + travel distance
- **Speaker cradle** — needs exact speaker mount-tab spacing + cone
  clearance + vent alignment
- **Light channel** — needs LED positions (from the PCB's actual CPL
  file, already in the repo), diffuser thickness decision (diffuser
  material test), PCB component envelope

Standoff is the only part whose every dimension is already locked — which
is why it's the "first print" in this flow.
