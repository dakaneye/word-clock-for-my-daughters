# KiCad PCB Build Guide — v0

**Target version:** KiCad 10.x

**Audience:** Same as the schematic guide — software engineer, zero prior KiCad PCB experience.

**Goal of this session:** Produce `hardware/word-clock.kicad_pcb` — a 7" × 7" double-sided PCB with all electrical components on the back and 35 SMD WS2812B LEDs on the front at positions matching the wood face's letter grid. Run DRC clean. Export gerbers ready for JLCPCB PCBA.

**Estimated time:** ~6-10 hours total, spread across 3-5 sessions. Take breaks — PCB layout is the hardest part of hardware design, and forcing progress on a tired brain creates routing mistakes you won't find until the board is fabricated.

**Critical prerequisite:** Phase A (schematic rework) must complete before Phase B (PCB layout). Don't jump ahead.

---

## Design decisions locked in

| Decision | Value | Rationale |
|---|---|---|
| LEDs | 35× SMD WS2812B (5050 package) on top side of PCB | Spec says "~25 per clock"; actual count is 35 words (one LED per `WordId` enum value). Perfect letter-grid alignment via PCB fabrication; no external strip. |
| Board dimensions | 7" × 7" = 177.8mm × 177.8mm | Matches the wood face; LED positions align with letter cells |
| Cell grid | 13 × 13 cells, 13.68mm each | Derived: 177.8mm / 13 ≈ 13.68mm |
| Layers | 2-layer FR4 | JLCPCB default; sufficient for this complexity |
| Board thickness | 1.6mm | JLCPCB default |
| Top side (F.Cu) | LEDs only, plus their decoupling caps, plus data/power traces between LEDs | Clean LED mount surface |
| Bottom side (B.Cu) | ESP32 dev board header, all breakout headers, 74HC245, buttons, USB-C, bulk cap, all signal routing | Hidden by the wood face; user never sees it |
| Track width (signal) | 0.25mm | JLCPCB default minimum 0.15mm; 0.25mm gives manufacturing headroom |
| Track width (power) | 0.5mm minimum, 1.0mm for +5V main feed | Handles WS2812B current |
| Clearance | 0.2mm | Above JLCPCB's 0.15mm minimum |
| Via drill | 0.3mm | JLCPCB minimum |
| Via pad | 0.6mm | Via + 0.15mm annular ring × 2 |

---

# Phase A: Schematic rework for 35 SMD WS2812B LEDs

This phase replaces the single `J_LED` connector with 35 individual WS2812B chip symbols, chained data-wise and sharing power rails. Do this entirely in the Schematic Editor before touching the PCB editor.

## A.1: Delete the J_LED connector

In the Schematic Editor, click on `J_LED1` (WS2812B strip connector). Press **Delete**.

Also delete any existing wires/labels associated with it. Keep `R1` (the 300Ω series resistor) — we still need that in series between the 74HC245 output and the first LED.

## A.2: Add 35 WS2812B symbols

Press **A** (add symbol). Search **`WS2812B`** in the symbol picker. KiCad's stock library has it at `LED:WS2812B`. Place one instance.

The WS2812B symbol has 4 pins:
- **VDD** (5V supply)
- **VSS** (ground)
- **DIN** (data in)
- **DOUT** (data out)

Now we need 35 of them. The efficient approach in KiCad:

1. Place one WS2812B symbol.
2. Select it, press **Ctrl+D** (duplicate). Click to place the duplicate.
3. Repeat until you have 35 instances arranged in a 5×7 or 6×6 grid on the sheet. Don't worry about visual placement on the schematic — they'll be repositioned on the PCB anyway. The schematic just needs them electrically connected.

Reference designators will auto-assign as D1, D2, ... D35 when you annotate.

## A.3: Map LEDs to WordIds

Each LED corresponds to one `WordId` from the firmware enum. Label each WS2812B symbol with the word it represents. Select each symbol, press **E**, set the **Value** field to the WordId name (e.g., `LED_IT`, `LED_IS`, `LED_HALF`, ...).

Order for clarity (matches firmware enum order):

| D# | Value / WordId | Firmware enum position |
|---|---|---|
| D1 | LED_IT | IT |
| D2 | LED_IS | IS |
| D3 | LED_TEN_MIN | TEN_MIN |
| D4 | LED_HALF | HALF |
| D5 | LED_QUARTER | QUARTER |
| D6 | LED_TWENTY | TWENTY |
| D7 | LED_FIVE_MIN | FIVE_MIN |
| D8 | LED_MINUTES | MINUTES |
| D9 | LED_PAST | PAST |
| D10 | LED_TO | TO |
| D11 | LED_ONE | ONE |
| D12 | LED_TWO | TWO |
| D13 | LED_THREE | THREE |
| D14 | LED_FOUR | FOUR |
| D15 | LED_FIVE_HR | FIVE_HR |
| D16 | LED_SIX | SIX |
| D17 | LED_SEVEN | SEVEN |
| D18 | LED_EIGHT | EIGHT |
| D19 | LED_NINE | NINE |
| D20 | LED_TEN_HR | TEN_HR |
| D21 | LED_ELEVEN | ELEVEN |
| D22 | LED_TWELVE | TWELVE |
| D23 | LED_OCLOCK | OCLOCK |
| D24 | LED_NOON | NOON |
| D25 | LED_IN | IN |
| D26 | LED_THE | THE |
| D27 | LED_AT | AT |
| D28 | LED_MORNING | MORNING |
| D29 | LED_AFTERNOON | AFTERNOON |
| D30 | LED_EVENING | EVENING |
| D31 | LED_NIGHT | NIGHT |
| D32 | LED_HAPPY | HAPPY |
| D33 | LED_BIRTH | BIRTH |
| D34 | LED_DAY | DAY |
| D35 | LED_NAME | NAME |

**Important: D-numbering follows physical row-major grid order, NOT the firmware WordId enum order.** Row-major placement (left-to-right, top-to-bottom) lets the data chain zig-zag across rows with minimal trace backtracking — routing-friendly.

The firmware will need a `WordId → chain_index` mapping table (added in Phase 2 when we wire up the display module). Something like:

```cpp
// firmware/lib/core/include/led_mapping.h (Phase 2)
constexpr uint8_t word_to_led[static_cast<uint8_t>(WordId::COUNT)] = {
    0,  // IT      -> D1
    1,  // IS      -> D2
    2,  // TEN_MIN -> D3
    3,  // HALF    -> D4
    4,  // QUARTER -> D5
    // ...
    8,  // HAPPY   -> D9 (row 3, col 1 — between IS and PAST physically)
    12, // BIRTH   -> D13 (row 4)
    // ...
};
```

For now, just assign D-numbers by physical order below. We'll reconcile with firmware in Phase 2.

## A.4: Wire power rails to all LEDs

Every LED gets +5V and GND. Two approaches:

**Approach 1 (easiest): power ports on every LED.** On each WS2812B symbol, place a `+5V` power port on the VDD pin and a `GND` port on the VSS pin. KiCad's power ports are global nets, so all 35 `+5V` ports are the same net, and all 35 `GND` ports are the same net. Visually noisy on the schematic but electrically correct.

**Approach 2 (cleaner visually): a single power bus.** Draw two horizontal wires across the sheet — label one `+5V`, the other `GND` — and connect each LED's VDD to the +5V wire, VSS to the GND wire. Uses net labels instead of repeated power ports.

For 35 LEDs, Approach 1 is faster to execute. Do that.

## A.5: Chain the data signal

The data chain is: **R1 (300Ω) → D1.DIN → D1.DOUT → D2.DIN → D2.DOUT → ... → D35.DIN**

Net labels to wire:

| Net label | Pins on this net |
|---|---|
| `LED_DATA_OUT` | R1 (300Ω) output side, **D1.DIN** |
| `LED_D1_OUT` | D1.DOUT, D2.DIN |
| `LED_D2_OUT` | D2.DOUT, D3.DIN |
| `LED_D3_OUT` | D3.DOUT, D4.DIN |
| ... | ... (continue sequentially) |
| `LED_D34_OUT` | D34.DOUT, D35.DIN |
| *(D35.DOUT is unused — end of chain — add a no-connect marker with **Q**)* | |

Yes, this means 34 distinct `LED_Dn_OUT` labels. Tedious but mechanical. Each label connects exactly two pins (previous LED's output, next LED's input).

**Shortcut:** draw a short wire from D1.DOUT, place label `LED_D1_OUT` at the end. Then **M** the label, use **Ctrl+D** to duplicate it, edit the text to `LED_D2_OUT`, place on D2.DOUT, etc. Takes ~15-20 minutes for all 34 labels.

## A.6: Add decoupling caps (optional but recommended)

WS2812B datasheet recommends a 100nF cap between VDD and VSS at each LED. Strips in the wild often omit them and work fine, but for a keeper build, add them.

Press **A**, search `C`, set value `100nF`. Duplicate 35 times, one per LED. Place each cap near its LED, terminal 1 to +5V, terminal 2 to GND. 35 more caps.

Auto-annotation will number them C8 through C42 (after the existing C2-C7).

**If you'd rather keep the schematic simple for v1:** skip this step. The 1000µF bulk cap (C7) at the LED strip power entry plus the WS2812B internal circuitry handles most noise. Revisit in v2 if LED flicker is observed.

**My recommendation: skip for v1.** You already have 6 caps; adding 35 more clutters the schematic without clear benefit for a first-pass board. Decide based on your tolerance for schematic density.

## A.7: Re-run ERC

Same steps as before. Expect new warnings you can safely ignore:
- Unused pin on D35.DOUT — add a no-connect marker to silence.
- Any tri-state vs power-input warnings (same as before).

Must-fix errors: anything labeled "Error". Warnings can be dismissed.

## A.8: Annotate and save

**Tools → Annotate Schematic** to assign D1-D35 to the LEDs.

**File → Save.**

**File → Plot → PDF** to regenerate the human-readable schematic PDF.

## A.9: Commit the schematic rework

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/word-clock.kicad_sch hardware/word-clock.pdf
git commit -m "feat(hardware): schematic — 35 smd ws2812b leds chained"
git push
```

---

# Phase B: PCB Editor basics and footprint assignment

Before you can lay out a PCB, every schematic symbol must have a footprint assigned (the physical pad layout). This is a one-time tedious step.

## B.1: Open the Footprint Assignment tool

From the Schematic Editor: **Tools → Assign Footprints** (or an icon that looks like a chip with a pointer). Opens the CvPcb tool with three panes:

- Left: footprint libraries (folders)
- Middle: your unassigned symbols
- Right: footprint preview (shows the physical pad layout)

## B.2: Assign a footprint to each symbol

Click each symbol in the middle pane, then browse the left pane to find an appropriate footprint. Double-click to assign. The footprint name appears next to the symbol.

For our project:

| Symbol | Assigned footprint | Library path |
|---|---|---|
| **ESP32 DevKit V1** (2x19 header) | **⚠️ See note below** — `Connector_PinHeader_2.54mm:PinHeader_2x19_P2.54mm_Vertical` is WRONG (row spacing is only 2.54mm; real dev board has 22.86mm row spacing) | See below |
| **J_AMP1** (MAX98357A breakout header, 7-pin) | `Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical` | Connector_PinHeader_2.54mm |
| **J_RTC1** (DS3231 breakout header, 6-pin) | `Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical` | Connector_PinHeader_2.54mm |
| **J_SD1** (microSD breakout header, 6-pin) | `Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical` | Connector_PinHeader_2.54mm |
| **J_USB1** (Cermant USB-C 16-pin breakout) | `Connector_PinHeader_2.54mm:PinHeader_1x16_P2.54mm_Vertical` | Connector_PinHeader_2.54mm — Cermant physically has 16 pads though we only wire 4 |
| **U2** (74HC245, DIP-20) | `Package_DIP:DIP-20_W7.62mm` | Package_DIP |
| **SW1-SW3** (tact switches, 6x6mm) | `Button_Switch_THT:SW_PUSH_6mm` | Button_Switch_THT |
| **R1** (300Ω resistor, through-hole) | `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal` | Resistor_THT |
| **C2-C6** (100nF ceramic) | `Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm` | Capacitor_THT |
| **C7** (1000µF polarized electrolytic) | `Capacitor_THT:CP_Radial_D10.0mm_P5.00mm` | Capacitor_THT |
| **D1-D35** (WS2812B SMD) | `LED_SMD:LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm` | LED_SMD |
| **PWR_FLAG symbols** | (no footprint needed — these are schematic-only) | — |

When you click on a WS2812B symbol, the preview pane will show the 4-pad 5050 footprint — square, ~5mm × 5mm.

### ESP32 DevKit V1 footprint — special handling required

The ESP32 DevKit V1 is two parallel 1×19 pin headers with **22.86mm (0.9")** between the two rows — NOT 2.54mm like a standard 2×19 connector. The stock `PinHeader_2x19` footprint has both rows at 2.54mm apart, which is physically wrong for our board.

**Two options to fix this:**

1. **Create a custom footprint** (cleanest): Open **Footprint Editor** (from KiCad project manager). Duplicate `PinHeader_2x19_P2.54mm_Vertical` into a local library, then edit pad positions so rows are 22.86mm apart instead of 2.54mm. Save as `ESP32_DEVKITV1_38pin`. Assign this footprint to the ESP32 symbol.

2. **Use two separate 1×19 footprints** (hacky workaround): split the ESP32 symbol in the schematic into two 1×19 connector symbols representing left-side and right-side pins. Each gets a `PinHeader_1x19_P2.54mm_Vertical` footprint. On the PCB, place them 22.86mm apart. This fragments the schematic though — worse than option 1.

**My recommendation: option 1.** Takes ~15 min first time. Search KiCad forums for "ESP32 DevKit V1 footprint" — several users have published their own that you can reference or import. Also check 3rd-party libraries (SparkFun, Adafruit) via KiCad's **Preferences → Manage Symbol Libraries → Add existing library** if you want pre-made.

**Do not proceed to placement (Phase C) with the 2.54mm row spacing** — the ESP32 dev board will not physically fit the PCB pads.

**Save as you go** (Cmd+S in the CvPcb window).

## B.3: Launch the PCB Editor

Back in the main KiCad project manager, double-click `word-clock.kicad_pcb`. An empty PCB editor opens.

## B.4: Update PCB from Schematic

**Tools → Update PCB from Schematic** (or **F8**). Dialog opens showing what will be added. Click **Update PCB**.

All 35 LED footprints, the ESP32 header, all breakout headers, the 74HC245, switches, resistor, and caps appear in a pile at the origin. Looks like a mess. That's fine — we'll place them next.

## B.5: Set board outline (Edge.Cuts layer)

Select the **Edge.Cuts** layer from the right-side panel (gray/yellow color by default).

Use **Place → Rectangle** (or the rectangle tool in the left toolbar) to draw a 177.8mm × 177.8mm rectangle. Click the top-left corner of the intended board, then drag to the bottom-right corner.

Exact coordinates: **(0, 0)** top-left, **(177.8, 177.8)** bottom-right. In KiCad, you can type exact coordinates in the properties panel after drawing the rectangle.

Tip: set the **grid origin** at the top-left of the board outline — **Place → Grid Origin**, click at (0, 0). This makes it easy to position LEDs relative to the board's top-left corner.

## B.6: Commit progress

```bash
git add hardware/word-clock.kicad_pcb
git commit -m "feat(hardware): pcb board outline 7x7, footprints assigned"
git push
```

---

# Phase C: Component placement

This is the longest phase. We place 35 LEDs on the front side at computed coordinates matching the letter grid, and all other components on the back side.

## C.1: LED placement on the front side (F.Cu)

The 13×13 letter grid maps to LED positions. For word `WordId` with span `(row, col_start, length)`, the LED center is:

```
X_mm = (col_start + length/2) × 13.68
Y_mm = (row + 0.5) × 13.68
```

Coordinates are relative to the top-left corner of the PCB (which we set as the grid origin in B.5).

Complete table (derived from `firmware/lib/core/src/grid.cpp` spans, Emory layout; Nora differs only in NAME position):

| D# | WordId | row | col_start | length | X (mm) | Y (mm) |
|---|---|---:|---:|---:|---:|---:|
| D1 | IT | 0 | 0 | 2 | 13.68 | 6.84 |
| D2 | IS | 0 | 3 | 2 | 54.72 | 6.84 |
| D3 | TEN_MIN | 0 | 6 | 3 | 102.60 | 6.84 |
| D4 | HALF | 0 | 9 | 4 | 150.48 | 6.84 |
| D5 | QUARTER | 1 | 0 | 7 | 47.88 | 20.52 |
| D6 | TWENTY | 1 | 7 | 6 | 136.80 | 20.52 |
| D7 | FIVE_MIN | 2 | 0 | 4 | 27.36 | 34.20 |
| D8 | MINUTES | 2 | 5 | 7 | 116.28 | 34.20 |
| D9 | HAPPY | 3 | 1 | 5 | 47.88 | 47.88 |
| D10 | PAST | 3 | 6 | 4 | 109.44 | 47.88 |
| D11 | TO | 3 | 10 | 2 | 150.48 | 47.88 |
| D12 | ONE | 4 | 0 | 3 | 20.52 | 61.56 |
| D13 | BIRTH | 4 | 3 | 5 | 75.24 | 61.56 |
| D14 | THREE | 4 | 8 | 5 | 143.64 | 61.56 |
| D15 | ELEVEN | 5 | 0 | 6 | 41.04 | 75.24 |
| D16 | FOUR | 5 | 6 | 4 | 109.44 | 75.24 |
| D17 | DAY | 5 | 10 | 3 | 157.32 | 75.24 |
| D18 | TWO | 6 | 0 | 3 | 20.52 | 88.92 |
| D19 | EIGHT | 6 | 3 | 5 | 75.24 | 88.92 |
| D20 | SEVEN | 6 | 8 | 5 | 143.64 | 88.92 |
| D21 | NINE | 7 | 0 | 4 | 27.36 | 102.60 |
| D22 | SIX | 7 | 4 | 3 | 75.24 | 102.60 |
| D23 | TWELVE | 7 | 7 | 6 | 136.80 | 102.60 |
| D24 | NAME | 8 | 0 | 5 | 34.20 | 116.28 |
| D25 | AT | 8 | 6 | 2 | 95.76 | 116.28 |
| D26 | FIVE_HR | 8 | 8 | 4 | 136.80 | 116.28 |
| D27 | TEN_HR | 9 | 0 | 3 | 20.52 | 129.96 |
| D28 | OCLOCK | 9 | 3 | 6 | 82.08 | 129.96 |
| D29 | NOON | 9 | 9 | 4 | 150.48 | 129.96 |
| D30 | IN | 10 | 1 | 2 | 27.36 | 143.64 |
| D31 | THE | 10 | 3 | 3 | 61.56 | 143.64 |
| D32 | MORNING | 10 | 6 | 7 | 129.96 | 143.64 |
| D33 | AFTERNOON | 11 | 0 | 9 | 61.56 | 157.32 |
| D34 | EVENING | 12 | 0 | 7 | 47.88 | 171.00 |
| D35 | NIGHT | 12 | 8 | 5 | 143.64 | 171.00 |

**To place each LED at its computed position:**

1. Click the LED's footprint in the board view (it starts piled at the origin).
2. Press **E** (edit properties).
3. In the properties dialog, set **Position X** and **Position Y** to the values from the table.
4. Set **Layer** to `F.Cu` (front copper) — the LED must mount on the top side.
5. Set **Rotation** to `0°` (all LEDs should face the same way; we'll verify orientation matters in D.1).

Or use the "move to position" command: select LED, press **M**, type the coordinates directly.

**Alternate: script it.** A short Python script using KiCad's scripting interface can place all 35 LEDs from the table in ~1 second. If you're comfortable with that, it saves 30 minutes. See `hardware/scripts/place_leds.py` (to be written — ask me to generate it if you want this).

## C.1a: Mounting holes

Add four M3 screw mounting holes, one near each corner of the PCB, 5mm in from each edge. These let you screw the PCB into the 3D-printed enclosure internals.

Use **Place → Footprint**, search `MountingHole_3.2mm`, place at:
- (5, 5)
- (172.8, 5)
- (5, 172.8)
- (172.8, 172.8)

Each hole is 3.2mm clearance for M3 screw + annular ring.

## C.2: Backside component placement

All other components go on `B.Cu` (back side). Select each footprint, press **E**, set Layer to `B.Cu` (it'll flip automatically — footprint mirrors as expected).

Suggested arrangement (back side, looking through the board from the front):

```
┌─────────────────────────────────────┐
│ [USB-C]         [microSD]  [buttons]│
│                                     │
│ [DS3231]  [ESP32 dev]      [MAX98]  │
│           [header pins]             │
│                                     │
│ [74HC245]  [bulk cap C7]            │
│                                     │
└─────────────────────────────────────┘
```

Exact placement is flexible — whatever makes routing easier. Considerations:
- **USB-C:** near an edge so the cable reaches. Top-left corner is conventional.
- **Buttons:** along one edge (right side) so they're accessible through the enclosure's side cut-outs.
- **MicroSD:** near an edge so you can swap cards without full disassembly.
- **ESP32 header:** center-ish; it's the biggest component.
- **74HC245:** near the ESP32 and near where its output routes up to the LED chain on the front.
- **Bulk cap C7:** near the USB-C power entry point.

## C.3: Commit placement

```bash
git add hardware/word-clock.kicad_pcb
git commit -m "feat(hardware): pcb component placement"
git push
```

---

# Phase D: Routing traces

## D.1: Verify LED orientation before routing

WS2812B chips have a specific pin orientation. Look at the footprint — pin 1 (VDD) should be in a consistent corner for all 35 LEDs. If they're rotated inconsistently, the data chain routing becomes much harder.

Best practice: rotate all LEDs so pin 1 is in the same corner (e.g., top-left). The **R** key rotates a selected component 90° at a time.

## D.2: Power rails

Start with the high-current paths.

- **+5V rail:** from USB-C VBUS entry point, through bulk cap C7, splitting to feed: (a) the 74HC245, microSD breakout, MAX98357A breakout; (b) all 35 LEDs on the front side. Use **1.0mm traces** for this.
- **+3V3 rail:** from ESP32's 3V3 pin to DS3231 VCC. **0.5mm traces.**
- **GND:** don't manually route. See D.4 (copper pour) — we'll fill empty space with ground planes on both sides, which carries the return current for everything.

Press **X** (route single track). Click on a pad, drag to another pad, click to complete. **V** adds a via if you need to change layers.

## D.3: Signal routing

Route each signal net with **0.25mm traces**:
- I²C: SDA and SCL from ESP32 D21/D22 pins to DS3231 SDA/SCL.
- VSPI: MOSI/MISO/SCK/CS from ESP32 to microSD.
- I²S: BCLK/LRC/DIN from ESP32 to MAX98357A.
- Buttons: BTN_HOUR/MINUTE/AUDIO from ESP32 to the three switches, with other side of each switch to GND.
- LED data chain: 74HC245 B0 → R1 (300Ω) → LED1.DIN → LED1.DOUT → LED2.DIN → ... → LED35.DIN. **This is the most error-prone route.** Go slow. Each LED connects to the NEXT one in sequence. Use the schematic as the reference.

## D.4: Copper pour for ground

Fill empty PCB area with GND copper to reduce ground impedance and improve noise immunity.

**Place → Filled Zone** (or zone drawing tool). Select layer **F.Cu**. Draw a rectangle covering the whole board area. In the properties dialog that pops up, set **Net** to `GND`. Click OK.

Repeat for **B.Cu** (same process, same GND net).

Press **B** to refill all zones — KiCad calculates the fill pattern, avoiding traces and pads. Any pad connected to GND gets automatically tied to the pour via thermal reliefs.

Both sides now have GND fill. Vias through non-fill regions tie the two pours together — add a few "stitching vias" along the board edges if desired (not critical for this frequency range).

## D.5: Commit routing

```bash
git add hardware/word-clock.kicad_pcb
git commit -m "feat(hardware): pcb routing v1 — power, signals, gnd pour"
git push
```

---

# Phase E: Design Rules Check (DRC)

## E.1: Configure design rules

**File → Board Setup → Design Rules → Constraints**. Set:
- Minimum clearance: **0.2mm**
- Minimum track width: **0.15mm** (JLCPCB minimum; we'll use 0.25mm actual)
- Minimum via diameter: **0.6mm**
- Minimum via drill: **0.3mm**

**File → Board Setup → Design Rules → Net Classes**. Create net classes:
- `Default` (signals): width 0.25mm, via 0.6/0.3mm
- `Power` (+5V, +3V3): width 0.5mm
- `Power_Main` (+5V main feed): width 1.0mm
- `GND` (default, filled by pours)

Assign each net to the right class via **Custom Rules** or the net class list.

## E.2: Run DRC

**Inspect → Design Rules Checker** (or the ladybug icon). Click **Run DRC**.

Expect a list of violations. Common ones and fixes:

| Violation | Cause | Fix |
|---|---|---|
| Clearance violation | Two traces or pads are within 0.2mm | Move one; usually caused by close placement |
| Unconnected nets | A signal wasn't routed | Look at the airwires (thin lines showing unrouted connections); route them |
| Silkscreen on pad | A reference designator overlaps a pad | Move or hide the ref des; **E** on the text |
| Overlapping courtyard | Two components are too close | Move one |

Fix iteratively. Re-run DRC after each fix.

## E.3: Target state: DRC clean

"0 errors, 0 warnings" in the DRC output. May have a handful of warnings you've consciously decided to ignore (mark them dismissed).

---

# Phase F: Print LED placement for physical verification

Before sending to fab, print the LED layout at 1:1 scale and overlay it on the wood face to confirm the LEDs align with the cut-through letters.

## F.1: Plot the front side to PDF

**File → Plot**. In the dialog:
- **Plot format:** PDF
- **Output directory:** `./plots/` (KiCad creates this)
- **Included layers:** check only `F.Cu`, `F.SilkS`, `Edge.Cuts` (and deselect all others)
- **Scale:** **1:1** (critical — any scaling breaks the overlay check)
- **Drill marks:** small
- Click **Plot**.

Result: `hardware/plots/word-clock-F_Cu.pdf` or similar.

## F.2: Print at 1:1

Open the PDF. Print with **"Actual size"** or **"100% scaling"** selected — NOT "fit to page" (which scales down). Use tiled printing across multiple 8.5×11" sheets if needed.

## F.3: Overlay on the wood face

Once the Ponoko laser-cut face arrives (or on a cardboard test cut), tape the printed sheet over the face with letter grid lines aligned to the cut-through letters. Each LED position should land under a letter or word group — not between letters.

**If misaligned:** fix the LED placement in the PCB editor (Phase C) and re-plot. Iterate until alignment is tight.

**Tolerance:** ±1mm is fine — the 3D-printed light channel will widen the illumination spot.

---

# Phase G: Export gerbers for JLCPCB

Once DRC is clean and LED placement is verified by the overlay check, export the files JLCPCB needs.

## G.1: Generate gerbers

**File → Fabrication Outputs → Gerbers**. In the dialog:
- **Output directory:** `plots/gerbers/`
- **Plot format:** Gerber
- **Included layers** (check these; leave others unchecked):
  - F.Cu, B.Cu (copper layers)
  - F.Paste, B.Paste (solder paste)
  - F.SilkS, B.SilkS (silkscreen)
  - F.Mask, B.Mask (solder mask)
  - Edge.Cuts (board outline)
- Click **Plot**.

## G.2: Generate drill files

Still in the Gerber dialog, click **Generate Drill Files**. Use defaults:
- **Drill units:** Millimeters
- **Zeros format:** Decimal
- **PTH and NPTH in single file:** checked

## G.3: Generate BOM and pick-and-place (for PCBA)

For JLCPCB's assembly service, you need two additional files:

- **BOM:** Tools → Generate Legacy BOM → CSV. Edit to include only parts JLCPCB needs to stock (their "Basic" or "Extended" parts catalog).
- **Pick-and-place (CPL):** File → Fabrication Outputs → Component Placement. Export as CSV with columns `Designator, Val, Package, MidX, MidY, Rotation, Layer`.

## G.4: Zip and upload to JLCPCB

```bash
cd hardware/plots/gerbers
zip ../word-clock-gerbers.zip *
```

Go to [jlcpcb.com](https://jlcpcb.com) → PCB → upload the zip. Review the Gerber Viewer's rendering (it should look like your PCB). Configure:
- **Layers:** 2
- **Dimensions:** 177.8mm × 177.8mm (should auto-detect from Edge.Cuts)
- **Quantity:** 5 (minimum order)
- **PCB Color:** your choice; **black** makes the backside blend in once the wood face is on
- **Surface Finish:** HASL (lead-free) is cheapest; ENIG looks better and ages better
- **PCBA:** select if you want JLCPCB to populate components; upload BOM + CPL

Review quote. Place order.

## G.5: Commit production files

```bash
git add hardware/plots/ hardware/bom.csv hardware/cpl.csv
git commit -m "feat(hardware): production gerbers + bom + cpl for jlcpcb"
git push
```

---

## Part H: Stop and request review

Before placing the JLCPCB order, send me the commit SHA. I'll review:

1. **Gerber visual check:** every layer looks right in an online Gerber viewer
2. **DRC clean:** no blocking violations
3. **LED placement:** math matches the grid; 1:1 print overlay verified
4. **BOM sanity:** every part number resolves to a real JLCPCB catalog entry
5. **Pick-and-place:** rotations and positions match the PCB

I'll reply with blockers / majors / minors. Fix, re-export, commit, repeat until clean.

---

## If you get stuck

**Common PCB gotchas:**

- **LED pin 1 orientation is wrong on some LEDs:** data chain won't work. All LEDs must have consistent pin 1 orientation. **R** key rotates; do it uniformly across all 35.
- **DRC clearance violations around the LED pads:** 5050 LEDs are tight; the 0.2mm clearance may be hard to hit. Increase board size by 5mm or loosen the clearance to 0.15mm.
- **Airwires never go away:** something on the schematic isn't connected in the PCB. Update PCB from Schematic (F8) to re-sync.
- **Gerber viewer shows wrong dimensions:** you scaled when plotting. Always plot at 1:1.
- **JLCPCB rejects the order:** read their auto-rejection notes carefully. Usually it's silkscreen-on-pad (fixable) or traces too close to the board edge (move them in 0.2mm).

**Nuclear option:** `git checkout hardware/word-clock.kicad_pcb` reverts your PCB to the last commit. Git will save you.

---

## What this guide does NOT cover

- Advanced layout techniques (differential pairs, impedance-controlled traces) — not needed for this project
- Multi-board panelization — we're single-board
- Custom footprint creation — we're using stock footprints only
- Stencil generation — handled by JLCPCB
- Firmware-side LED ordering (how `leds[0]` → physical LED D1) — that lives in Phase 2 firmware, not here
