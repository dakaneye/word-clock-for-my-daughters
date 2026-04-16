# KiCad Rework Guide — Fix LED ordering + AT position + finish PCB

**Starting state:** Schematic has 35 WS2812B LEDs in enum order (D1=IT...D35=NAME) with a partial data chain wired. PCB has all 35 LEDs placed at correct grid positions, power rails partially routed, and a half-done data chain in the wrong order. AT LED (D27) is at the wrong physical position (95.76, 116.28) — needs to move to (150.48, 157.32).

**End state:** Schematic and PCB both use row-major D-numbering (D1=IT top-left through D35=NIGHT bottom-right). AT is on row 11. Data chain flows left-to-right per row. Power rails complete. All traces routed. DRC clean. Gerbers exportable.

---

# Part 1: Fix the schematic

## 1.1: Delete ALL data-chain wiring

Every wire or label connecting any LED's DOUT to any LED's DIN. Also the wire from R1 to the first LED's DIN. Delete all of it.

How to identify data-chain wires: they connect DOUT of one WS2812B symbol to DIN of another. They do NOT touch VDD or VSS pins. If a wire goes between VDD and a +5V port, or VSS and a GND port — leave it alone. Only delete DOUT↔DIN connections.

Click each chain wire → **Delete**. Work through systematically. When done, every LED should have:
- VDD still connected to `+5V` ✓
- VSS still connected to `GND` ✓  
- DIN floating (no connection) ✓
- DOUT floating (no connection) ✓

## 1.2: Rearrange LED symbols on the schematic sheet

Drag each LED symbol (**M** to move) into a grid matching the physical clock face. Identify each LED by its **Value** field (LED_IT, LED_IS, etc.).

Target layout on the schematic sheet (left-to-right within each row, rows top-to-bottom):

```
Sheet row 1:   LED_IT       LED_IS       LED_TEN_MIN   LED_HALF
Sheet row 2:   LED_QUARTER  LED_TWENTY
Sheet row 3:   LED_FIVE_MIN LED_MINUTES
Sheet row 4:   LED_HAPPY    LED_PAST     LED_TO
Sheet row 5:   LED_ONE      LED_BIRTH    LED_THREE
Sheet row 6:   LED_ELEVEN   LED_FOUR     LED_DAY
Sheet row 7:   LED_TWO      LED_EIGHT    LED_SEVEN
Sheet row 8:   LED_NINE     LED_SIX      LED_TWELVE
Sheet row 9:   LED_NAME     LED_FIVE_HR
Sheet row 10:  LED_TEN_HR   LED_OCLOCK   LED_NOON
Sheet row 11:  LED_IN       LED_THE      LED_MORNING
Sheet row 12:  LED_AFTERNOON LED_AT
Sheet row 13:  LED_EVENING  LED_NIGHT
```

Doesn't need to be pixel-perfect. Just needs to be clearly in this order so the auto-annotator sorts correctly.

## 1.3: Re-annotate

**Tools → Annotate Schematic**. In the dialog:
- ✅ **Reset existing annotations** (wipes old D1-D35)
- Sort order: **Sort symbols by Y position, then X position**
- Click **Annotate**

After this: D1 = top-left (IT), D35 = bottom-right (NIGHT). The mapping from D# to WordId is now:

| D# | WordId | Grid position |
|---|---|---|
| D1 | IT | row 0, col 0-1 |
| D2 | IS | row 0, col 3-4 |
| D3 | TEN_MIN | row 0, col 6-8 |
| D4 | HALF | row 0, col 9-12 |
| D5 | QUARTER | row 1, col 0-6 |
| D6 | TWENTY | row 1, col 7-12 |
| D7 | FIVE_MIN | row 2, col 0-3 |
| D8 | MINUTES | row 2, col 5-11 |
| D9 | HAPPY | row 3, col 1-5 |
| D10 | PAST | row 3, col 6-9 |
| D11 | TO | row 3, col 10-11 |
| D12 | ONE | row 4, col 0-2 |
| D13 | BIRTH | row 4, col 3-7 |
| D14 | THREE | row 4, col 8-12 |
| D15 | ELEVEN | row 5, col 0-5 |
| D16 | FOUR | row 5, col 6-9 |
| D17 | DAY | row 5, col 10-12 |
| D18 | TWO | row 6, col 0-2 |
| D19 | EIGHT | row 6, col 3-7 |
| D20 | SEVEN | row 6, col 8-12 |
| D21 | NINE | row 7, col 0-3 |
| D22 | SIX | row 7, col 4-6 |
| D23 | TWELVE | row 7, col 7-12 |
| D24 | NAME | row 8, col 0-4 |
| D25 | FIVE_HR | row 8, col 8-11 |
| D26 | TEN_HR | row 9, col 0-2 |
| D27 | OCLOCK | row 9, col 3-8 |
| D28 | NOON | row 9, col 9-12 |
| D29 | IN | row 10, col 1-2 |
| D30 | THE | row 10, col 3-5 |
| D31 | MORNING | row 10, col 6-12 |
| D32 | AFTERNOON | row 11, col 0-8 |
| D33 | AT | row 11, col 10-11 |
| D34 | EVENING | row 12, col 0-6 |
| D35 | NIGHT | row 12, col 8-12 |

Verify: click a few LEDs and check their Reference (D#) matches the table above based on their Value (LED_xxx).

## 1.4: Wire the data chain

Wire left-to-right, row by row. Because the symbols are now in row-major order on the sheet, each connection is a short neighbor wire.

1. Wire R1's output terminal → D1's DIN pin.
2. D1 DOUT → D2 DIN.
3. D2 DOUT → D3 DIN.
4. D3 DOUT → D4 DIN.
5. D4 DOUT → D5 DIN (end of row 0 → start of row 1).
6. Continue: D5→D6, D6→D7, D7→D8, D8→D9, D9→D10, D10→D11, D11→D12, D12→D13, D13→D14, D14→D15, D15→D16, D16→D17, D17→D18, D18→D19, D19→D20, D20→D21, D21→D22, D22→D23, D23→D24, D24→D25, D25→D26, D26→D27, D27→D28, D28→D29, D29→D30, D30→D31, D31→D32, D32→D33, D33→D34, D34→D35.
7. D35 DOUT → press **Q** to add a no-connect marker.

34 wires. Each one connects the right side of one LED to the left side of the next.

## 1.5: Verify R1 is still connected correctly

R1 (300Ω resistor) sits between the 74HC245 B0 output and the first LED. Check:
- One terminal of R1 has the `LED_DATA_5V` label (from 74HC245 B0)
- Other terminal connects to D1's DIN pin (wired in step 1.4.1)

If R1 got disconnected during cleanup, re-wire it.

## 1.6: Run ERC

**Inspect → Electrical Rules Checker → Run ERC.** Same ignorable warnings as before (tri-state/power, lib_symbol_mismatch). No new errors should appear.

## 1.7: Save and plot

- **Cmd-S** to save the schematic.
- **File → Plot → PDF** to regenerate `word-clock.pdf`.

---

# Part 2: Fix the PCB

## 2.1: Update PCB from Schematic

Open the PCB Editor. **Tools → Update PCB from Schematic (F8).**

What this does:
- Renames all 35 LED footprints from their old D-numbers to the new row-major D-numbers
- Updates net names to match the new chain order

What it does NOT do:
- Move any physical footprint (all stay at their current X, Y)
- Delete any routed traces

Click **Update PCB**. Review the report for errors; USB pad warnings are the same as before (ignorable).

## 2.2: Move the AT LED to its new position

The LED with Value `LED_AT` (now reference D33 after re-annotation) is currently at the wrong physical position (old: 95.76, 116.28). Move it:

1. Click the `LED_AT` footprint on the board. Look for it on the front side around where row 8 is.
2. Press **E** (edit properties).
3. Set:
   - **X:** 150.48
   - **Y:** 157.32
   - **Side:** Front
   - **Orientation:** 0° (same as all other LEDs)
4. Click **OK**.

## 2.3: Delete ALL data-chain traces on the PCB

Every trace that was part of the LED DOUT→DIN chain is now wrong. Delete all of them. These are the thin signal traces connecting LED pads on the front side — NOT the wider +5V power traces or the GND pour.

How to identify data-chain traces:
- They connect to the small signal pads on WS2812B footprints (DIN/DOUT pads, not VDD/VSS pads)
- They're on F.Cu (front side)
- They're thinner than power traces (0.25mm vs 0.5mm+)

Click each trace → **Delete**. Or: right-click a data trace → **Select → All Tracks in Net** → Delete. Work through until no data-chain traces remain.

Leave power traces (+5V, GND) alone.

## 2.4: Delete power traces going to the AT LED's OLD position

The AT LED moved. Its old position (95.76, 116.28) had +5V and GND traces routed to it. Those traces now go to empty space.

1. Find the dangling +5V and GND traces near position (95.76, 116.28) — they'll end at pads that no longer exist.
2. Click each dangling trace segment → **Delete**.

## 2.5: Route +5V and GND to AT LED's NEW position

D33 (AT) is now at (150.48, 157.32). It needs +5V on its VDD pad and GND on its VSS pad.

1. Press **X** (start trace).
2. Click on D33's VDD pad.
3. Route a trace to the nearest +5V rail or via. Use 0.5mm trace width for power.
4. Press **Escape**.
5. Repeat: **X**, click D33's VSS pad, route to the nearest GND connection (or just leave it — the GND copper pour will connect it automatically after you refill zones in step 2.8).

## 2.6: Route the data chain — clean serpentine

This is the main routing work. The chain goes D1→D2→...→D35 in row-major order. Because LEDs are physically arranged in rows, each connection is short.

Press **X** to start a trace. Use **0.25mm width** for the data chain.

Pattern per row:
- Within a row: route DOUT of the left LED → DIN of the right LED. Short horizontal trace.
- Between rows: route DOUT of the row's rightmost LED → DIN of the next row's leftmost LED. Diagonal or L-shaped trace dropping down one row.

Start:
1. R1's output pad → D1's DIN pad (data enters the chain).
2. D1 DOUT → D2 DIN.
3. D2 DOUT → D3 DIN.
4. D3 DOUT → D4 DIN (end of row 0).
5. D4 DOUT → D5 DIN (drop to row 1 leftmost).
6. D5 DOUT → D6 DIN (end of row 1).
7. D6 DOUT → D7 DIN (drop to row 2).
8. Continue through all 35. Each trace is short.
9. D35's DOUT pad: leave unconnected (end of chain; no trace needed).

If two traces need to cross on the same layer, insert a via (**V** while routing) to jump to B.Cu, cross, then via back up to F.Cu.

The R1 → D1 connection may need a via from B.Cu (where R1 sits on the backside) through to F.Cu (where D1 sits on the front). That's expected.

## 2.7: Route remaining backside signals (if not already done)

If you haven't routed the non-LED signals yet, do them now:

| Signal | From | To | Width |
|---|---|---|---|
| `I2C_SDA` | ESP32 GPIO21 | DS3231 SDA | 0.25mm |
| `I2C_SCL` | ESP32 GPIO22 | DS3231 SCL | 0.25mm |
| `I2S_BCLK` | ESP32 GPIO26 | MAX98357A BCLK | 0.25mm |
| `I2S_LRC` | ESP32 GPIO25 | MAX98357A LRC | 0.25mm |
| `I2S_DIN` | ESP32 GPIO27 | MAX98357A DIN | 0.25mm |
| `SD_CS` | ESP32 GPIO5 | microSD CS | 0.25mm |
| `SD_MOSI` | ESP32 GPIO23 | microSD MOSI | 0.25mm |
| `SD_MISO` | ESP32 GPIO19 | microSD MISO | 0.25mm |
| `SD_SCK` | ESP32 GPIO18 | microSD SCK | 0.25mm |
| `BTN_HOUR` | ESP32 GPIO32 | SW1 → GND | 0.25mm |
| `BTN_MINUTE` | ESP32 GPIO33 | SW2 → GND | 0.25mm |
| `BTN_AUDIO` | ESP32 GPIO14 | SW3 → GND | 0.25mm |
| `LED_DATA_3V3` | ESP32 GPIO13 | 74HC245 A0 | 0.25mm |
| `LED_DATA_5V` | 74HC245 B0 | R1 input | 0.25mm |
| `+5V main feed` | USB-C VBUS | C7 bulk cap → branches | 1.0mm |
| `+3V3` | ESP32 3V3 pin | DS3231 VCC | 0.5mm |

All backside signals stay on B.Cu. The `LED_DATA_5V` → R1 → D1 path crosses from B.Cu to F.Cu via a via.

## 2.8: Pour ground planes on both sides

**Place → Filled Zone**. Select **F.Cu** layer. Draw a rectangle covering the entire board. Set net to `GND`. Click OK.

Repeat for **B.Cu** — same rectangle, same GND net.

Press **B** to refill all zones. KiCad fills empty copper area with GND on both sides. Any pad connected to GND is automatically tied to the pour via thermal reliefs. Any pad NOT on GND is isolated by clearance gaps.

This handles all GND connections automatically — including D33's (AT) VSS pad if you didn't manually route it in step 2.5.

## 2.9: Run DRC

**Inspect → Design Rules Checker → Run DRC.**

Fix any violations:
- **Clearance:** two traces too close → move one.
- **Unconnected nets:** an airwire still exists → route it.
- **Silkscreen on pad:** a label overlaps a pad → move the label text.

Target: **0 errors**. Warnings about USB-C unused pads (same as before) are fine.

## 2.10: Print LED layout at 1:1 for verification

**File → Plot**. Configure:
- Format: PDF
- Layers: F.Cu, F.SilkS, Edge.Cuts only
- Scale: **1:1** (critical)
- Click Plot.

Print the PDF at actual size. Overlay on a hand-drawn 13×13 grid (7"×7", 0.54" cells) or on Chelsea's clock face. Verify each LED lands at the center of its word.

## 2.11: Export gerbers for JLCPCB

**File → Fabrication Outputs → Gerbers:**
- Layers: F.Cu, B.Cu, F.Paste, B.Paste, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
- Click Plot.

**Generate Drill Files** (button in the same dialog):
- Millimeters, decimal format
- PTH and NPTH in single file

**File → Fabrication Outputs → Component Placement:**
- Export CSV (for JLCPCB PCBA pick-and-place)

**BOM:** Tools → Generate Legacy BOM → CSV.

Zip the gerbers:
```bash
cd hardware
zip gerbers.zip *.gbr *.gbrjob *.drl
```

## 2.12: Commit everything

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/ firmware/
git commit -m "fix(hardware): row-major led chain, AT relocated, pcb routing complete"
git push
```

## 2.13: Stop for review

Send me the commit SHA. I'll review:
- LED placement vs grid coordinates
- Data chain continuity (D1→D35 sequential)
- Power distribution to all 35 LEDs + all breakouts
- AT at correct new position
- DRC clean
- Gerber sanity check

---

# Reference: LED position table

All 35 LEDs in row-major D-numbering order. Use this for placement verification.

| D# | WordId | X (mm) | Y (mm) |
|---|---|---:|---:|
| D1 | IT | 13.68 | 6.84 |
| D2 | IS | 54.72 | 6.84 |
| D3 | TEN_MIN | 102.60 | 6.84 |
| D4 | HALF | 150.48 | 6.84 |
| D5 | QUARTER | 47.88 | 20.52 |
| D6 | TWENTY | 136.80 | 20.52 |
| D7 | FIVE_MIN | 27.36 | 34.20 |
| D8 | MINUTES | 116.28 | 34.20 |
| D9 | HAPPY | 47.88 | 47.88 |
| D10 | PAST | 109.44 | 47.88 |
| D11 | TO | 150.48 | 47.88 |
| D12 | ONE | 20.52 | 61.56 |
| D13 | BIRTH | 75.24 | 61.56 |
| D14 | THREE | 143.64 | 61.56 |
| D15 | ELEVEN | 41.04 | 75.24 |
| D16 | FOUR | 109.44 | 75.24 |
| D17 | DAY | 157.32 | 75.24 |
| D18 | TWO | 20.52 | 88.92 |
| D19 | EIGHT | 75.24 | 88.92 |
| D20 | SEVEN | 143.64 | 88.92 |
| D21 | NINE | 27.36 | 102.60 |
| D22 | SIX | 75.24 | 102.60 |
| D23 | TWELVE | 136.80 | 102.60 |
| D24 | NAME | 34.20 | 116.28 |
| D25 | FIVE_HR | 136.80 | 116.28 |
| D26 | TEN_HR | 20.52 | 129.96 |
| D27 | OCLOCK | 82.08 | 129.96 |
| D28 | NOON | 150.48 | 129.96 |
| D29 | IN | 27.36 | 143.64 |
| D30 | THE | 61.56 | 143.64 |
| D31 | MORNING | 129.96 | 143.64 |
| D32 | AFTERNOON | 61.56 | 157.32 |
| D33 | AT | 150.48 | 157.32 |
| D34 | EVENING | 47.88 | 171.00 |
| D35 | NIGHT | 143.64 | 171.00 |
