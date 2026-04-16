# KiCad Rework Guide — Fix LED chain order + AT position + finish PCB

**Starting state:** Schematic has 35 WS2812B LEDs with a partial data chain wired in the wrong order. PCB has all 35 LEDs placed at correct grid positions, power rails partially routed, and a half-done data chain. AT LED is at the wrong physical position (95.76, 116.28) — needs to move to (150.48, 157.32).

**End state:** Data chain wired in physical row-major order (left-to-right, top-to-bottom on the clock face). AT on row 11. Power rails complete. All traces routed. DRC clean. Gerbers exportable.

**Key principle:** Schematic layout is for human readability only — it does NOT affect the PCB. Don't rearrange symbols on the schematic sheet. The physical PCB positions come from the X,Y coordinate table. The chain order comes from how you wire DOUT→DIN. D-numbers are cosmetic silkscreen labels.

---

# Part 1: Fix the schematic

## 1.1: Delete ALL data-chain wiring

Delete every wire or label connecting any LED's DOUT to any LED's DIN. Also delete the wire from R1 to the first LED's DIN.

How to identify data-chain wires: they connect DOUT of one WS2812B symbol to DIN of another. They do NOT touch VDD or VSS pins.

**Leave alone:** any wire between VDD and a +5V port, or VSS and a GND port. Only delete DOUT↔DIN connections.

When done, every LED should have:
- VDD still connected to `+5V` ✓
- VSS still connected to `GND` ✓
- DIN floating (no connection) ✓
- DOUT floating (no connection) ✓

## 1.2: Wire the data chain by Value name

**Do NOT rearrange symbols on the sheet.** Your current layout is fine. Find each LED by its **Value** label and wire DOUT→DIN in this exact sequence:

```
R1 output
  → LED_IT
  → LED_IS
  → LED_TEN_MIN
  → LED_HALF
  → LED_QUARTER
  → LED_TWENTY
  → LED_FIVE_MIN
  → LED_MINUTES
  → LED_HAPPY
  → LED_PAST
  → LED_TO
  → LED_ONE
  → LED_BIRTH
  → LED_THREE
  → LED_ELEVEN
  → LED_FOUR
  → LED_DAY
  → LED_TWO
  → LED_EIGHT
  → LED_SEVEN
  → LED_NINE
  → LED_SIX
  → LED_TWELVE
  → LED_NAME
  → LED_FIVE_HR
  → LED_TEN_HR
  → LED_OCLOCK
  → LED_NOON
  → LED_IN
  → LED_THE
  → LED_MORNING
  → LED_AFTERNOON
  → LED_AT
  → LED_EVENING
  → LED_NIGHT (DOUT → no-connect, press Q)
```

Each `→` means: wire previous LED's DOUT to next LED's DIN.

**If two LEDs are neighbors on the sheet:** draw a wire (**W**).
**If two LEDs are far apart on the sheet:** use matching labels (**L**). Example: place label `CHAIN_8` on LED_MINUTES DOUT and the same label `CHAIN_8` on LED_HAPPY DIN. They're connected without a wire crossing the sheet.

34 connections total.

## 1.3: Verify R1 is connected

R1 (300Ω) sits between 74HC245 B0 output and the first LED in the chain:
- One terminal: `LED_DATA_5V` label (from 74HC245 B0)
- Other terminal: connected to LED_IT's DIN (first `→` in the chain above)

If R1 got disconnected during cleanup, re-wire it.

## 1.4: Re-annotate (cosmetic only)

**Tools → Annotate Schematic** → Reset existing + Sort by Y then X → Annotate.

Whatever D-numbers KiCad assigns are fine. They're silkscreen labels, not functional. The firmware mapping table handles WordId→chain-position translation regardless of what D-number each LED gets.

## 1.5: Run ERC

Same ignorable warnings as before. No new errors should appear.

## 1.6: Save and plot

- **Cmd-S** to save.
- **File → Plot → PDF** to regenerate `word-clock.pdf`.

---

# Part 2: Fix the PCB

## 2.1: Update PCB from Schematic

Open PCB Editor. **Tools → Update PCB from Schematic (F8).**

This pushes the new chain wiring and any new D-numbers to the PCB. Physical footprint positions stay where they are. Click **Update PCB**.

## 2.2: Move the AT LED to its new position

Find the LED with Value `LED_AT` on the front side (currently near position 95.76, 116.28).

1. Click it.
2. Press **E**.
3. Set:
   - **X:** 150.48
   - **Y:** 157.32
   - **Side:** Front
   - **Orientation:** 0°
4. Click OK.

## 2.3: Delete ALL LED data-chain traces on the PCB

Every trace that was part of the LED DOUT→DIN chain. These are thin signal traces (0.25mm) on F.Cu connecting LED pads on the front side.

**Do NOT delete:**
- Wide +5V power traces (0.5mm+)
- GND pour / GND traces
- Any backside signal traces (I2C, SPI, I2S, buttons)

Click each data trace → **Delete**. Or: right-click a data trace → **Select → All Tracks in Net** → Delete.

## 2.4: Delete power traces to AT LED's OLD position

The AT LED moved. Its old position (95.76, 116.28) has +5V and GND traces going to empty space now. Find and delete those dangling trace segments.

## 2.5: Route +5V to AT LED's new position

1. Press **X** (start trace), set width to 0.5mm.
2. Click on AT LED's VDD pad at (150.48, 157.32).
3. Route to the nearest existing +5V trace or via.
4. Press **Escape**.

GND: the ground pour (step 2.8) will connect AT's VSS pad automatically. No manual GND trace needed if you do the pour.

## 2.6: Route the data chain — serpentine by Value

Press **X**, set width to 0.25mm. Route in this physical order across the front side of the board:

**Row 0 (Y=6.84):** left to right
- R1 output pad → LED_IT DIN (via from B.Cu to F.Cu needed here — R1 is on the back)
- LED_IT DOUT → LED_IS DIN
- LED_IS DOUT → LED_TEN_MIN DIN
- LED_TEN_MIN DOUT → LED_HALF DIN

**Row 0→1 jump:**
- LED_HALF DOUT (150.48, 6.84) → LED_QUARTER DIN (47.88, 20.52)

**Row 1 (Y=20.52):**
- LED_QUARTER DOUT → LED_TWENTY DIN

**Row 1→2 jump:**
- LED_TWENTY DOUT (136.80, 20.52) → LED_FIVE_MIN DIN (27.36, 34.20)

**Row 2 (Y=34.20):**
- LED_FIVE_MIN DOUT → LED_MINUTES DIN

**Row 2→3 jump:**
- LED_MINUTES DOUT (116.28, 34.20) → LED_HAPPY DIN (47.88, 47.88)

**Row 3 (Y=47.88):**
- LED_HAPPY DOUT → LED_PAST DIN
- LED_PAST DOUT → LED_TO DIN

**Row 3→4 jump:**
- LED_TO DOUT (150.48, 47.88) → LED_ONE DIN (20.52, 61.56)

**Row 4 (Y=61.56):**
- LED_ONE DOUT → LED_BIRTH DIN
- LED_BIRTH DOUT → LED_THREE DIN

**Row 4→5 jump:**
- LED_THREE DOUT (143.64, 61.56) → LED_ELEVEN DIN (41.04, 75.24)

**Row 5 (Y=75.24):**
- LED_ELEVEN DOUT → LED_FOUR DIN
- LED_FOUR DOUT → LED_DAY DIN

**Row 5→6 jump:**
- LED_DAY DOUT (157.32, 75.24) → LED_TWO DIN (20.52, 88.92)

**Row 6 (Y=88.92):**
- LED_TWO DOUT → LED_EIGHT DIN
- LED_EIGHT DOUT → LED_SEVEN DIN

**Row 6→7 jump:**
- LED_SEVEN DOUT (143.64, 88.92) → LED_NINE DIN (27.36, 102.60)

**Row 7 (Y=102.60):**
- LED_NINE DOUT → LED_SIX DIN
- LED_SIX DOUT → LED_TWELVE DIN

**Row 7→8 jump:**
- LED_TWELVE DOUT (136.80, 102.60) → LED_NAME DIN (34.20, 116.28)

**Row 8 (Y=116.28):**
- LED_NAME DOUT → LED_FIVE_HR DIN

**Row 8→9 jump:**
- LED_FIVE_HR DOUT (136.80, 116.28) → LED_TEN_HR DIN (20.52, 129.96)

**Row 9 (Y=129.96):**
- LED_TEN_HR DOUT → LED_OCLOCK DIN
- LED_OCLOCK DOUT → LED_NOON DIN

**Row 9→10 jump:**
- LED_NOON DOUT (150.48, 129.96) → LED_IN DIN (27.36, 143.64)

**Row 10 (Y=143.64):**
- LED_IN DOUT → LED_THE DIN
- LED_THE DOUT → LED_MORNING DIN

**Row 10→11 jump:**
- LED_MORNING DOUT (129.96, 143.64) → LED_AFTERNOON DIN (61.56, 157.32)

**Row 11 (Y=157.32):**
- LED_AFTERNOON DOUT → LED_AT DIN

**Row 11→12 jump:**
- LED_AT DOUT (150.48, 157.32) → LED_EVENING DIN (47.88, 171.00)

**Row 12 (Y=171.00):**
- LED_EVENING DOUT → LED_NIGHT DIN
- LED_NIGHT DOUT: leave unconnected (end of chain).

If two traces need to cross on the same layer: press **V** while routing to insert a via, cross on B.Cu, via back to F.Cu.

## 2.7: Route remaining backside signals (if not already done)

Skip this step if you already routed these. All on B.Cu, 0.25mm width unless noted:

| Signal | From | To |
|---|---|---|
| `I2C_SDA` | ESP32 GPIO21 | DS3231 SDA |
| `I2C_SCL` | ESP32 GPIO22 | DS3231 SCL |
| `I2S_BCLK` | ESP32 GPIO26 | MAX98357A BCLK |
| `I2S_LRC` | ESP32 GPIO25 | MAX98357A LRC |
| `I2S_DIN` | ESP32 GPIO27 | MAX98357A DIN |
| `SD_CS` | ESP32 GPIO5 | microSD CS |
| `SD_MOSI` | ESP32 GPIO23 | microSD MOSI |
| `SD_MISO` | ESP32 GPIO19 | microSD MISO |
| `SD_SCK` | ESP32 GPIO18 | microSD SCK |
| `BTN_HOUR` | ESP32 GPIO32 | SW1 → GND |
| `BTN_MINUTE` | ESP32 GPIO33 | SW2 → GND |
| `BTN_AUDIO` | ESP32 GPIO14 | SW3 → GND |
| `LED_DATA_3V3` | ESP32 GPIO13 | 74HC245 A0 |
| `LED_DATA_5V` | 74HC245 B0 | R1 input |
| `+5V main feed` | USB-C VBUS | C7 → branches (1.0mm width) |
| `+3V3` | ESP32 3V3 pin | DS3231 VCC (0.5mm width) |

## 2.8: Pour ground planes on both sides

**Place → Filled Zone.** Select **F.Cu**. Draw rectangle covering entire board. Set net to `GND`. OK.

Repeat for **B.Cu** — same rectangle, same GND net.

Press **B** to refill all zones. Handles all GND connections automatically.

## 2.9: Run DRC

**Inspect → Design Rules Checker → Run DRC.**

Fix violations:
- **Clearance:** move traces apart
- **Unconnected nets:** route the missing connection
- **Silkscreen on pad:** move text

Target: 0 errors. USB-C pad warnings are fine.

## 2.10: Print LED layout at 1:1

**File → Plot.** Layers: F.Cu + F.SilkS + Edge.Cuts only. Scale: **1:1.** Plot.

Print at actual size. Overlay on a 7"×7" grid to verify LED positions match word centers.

## 2.11: Export gerbers

**File → Fabrication Outputs → Gerbers:**
- Layers: F.Cu, B.Cu, F.Paste, B.Paste, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
- Plot.

**Generate Drill Files:** Millimeters, decimal, PTH+NPTH single file.

**Component Placement:** File → Fabrication Outputs → Component Placement → CSV.

**BOM:** Tools → Generate Legacy BOM → CSV.

```bash
cd hardware
zip gerbers.zip *.gbr *.gbrjob *.drl
```

## 2.12: Commit

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/
git commit -m "fix(hardware): row-major led chain, AT relocated, pcb complete"
git push
```

## 2.13: Stop for review

Send me the commit SHA. I'll review against the position table and chain order before you submit to JLCPCB.

---

# Reference: LED position table

Physical positions on the 7"×7" (177.8mm) board. Each LED centered under its word.

| WordId | Grid (row, col, len) | X (mm) | Y (mm) |
|---|---|---:|---:|
| IT | 0, 0, 2 | 13.68 | 6.84 |
| IS | 0, 3, 2 | 54.72 | 6.84 |
| TEN_MIN | 0, 6, 3 | 102.60 | 6.84 |
| HALF | 0, 9, 4 | 150.48 | 6.84 |
| QUARTER | 1, 0, 7 | 47.88 | 20.52 |
| TWENTY | 1, 7, 6 | 136.80 | 20.52 |
| FIVE_MIN | 2, 0, 4 | 27.36 | 34.20 |
| MINUTES | 2, 5, 7 | 116.28 | 34.20 |
| HAPPY | 3, 1, 5 | 47.88 | 47.88 |
| PAST | 3, 6, 4 | 109.44 | 47.88 |
| TO | 3, 10, 2 | 150.48 | 47.88 |
| ONE | 4, 0, 3 | 20.52 | 61.56 |
| BIRTH | 4, 3, 5 | 75.24 | 61.56 |
| THREE | 4, 8, 5 | 143.64 | 61.56 |
| ELEVEN | 5, 0, 6 | 41.04 | 75.24 |
| FOUR | 5, 6, 4 | 109.44 | 75.24 |
| DAY | 5, 10, 3 | 157.32 | 75.24 |
| TWO | 6, 0, 3 | 20.52 | 88.92 |
| EIGHT | 6, 3, 5 | 75.24 | 88.92 |
| SEVEN | 6, 8, 5 | 143.64 | 88.92 |
| NINE | 7, 0, 4 | 27.36 | 102.60 |
| SIX | 7, 4, 3 | 75.24 | 102.60 |
| TWELVE | 7, 7, 6 | 136.80 | 102.60 |
| NAME | 8, 0, 5 | 34.20 | 116.28 |
| FIVE_HR | 8, 8, 4 | 136.80 | 116.28 |
| TEN_HR | 9, 0, 3 | 20.52 | 129.96 |
| OCLOCK | 9, 3, 6 | 82.08 | 129.96 |
| NOON | 9, 9, 4 | 150.48 | 129.96 |
| IN | 10, 1, 2 | 27.36 | 143.64 |
| THE | 10, 3, 3 | 61.56 | 143.64 |
| MORNING | 10, 6, 7 | 129.96 | 143.64 |
| AFTERNOON | 11, 0, 9 | 61.56 | 157.32 |
| AT | 11, 10, 2 | 150.48 | 157.32 |
| EVENING | 12, 0, 7 | 47.88 | 171.00 |
| NIGHT | 12, 8, 5 | 143.64 | 171.00 |
