<!-- docs/hardware/pre-fab-review.md -->
# Pre-fab review — 63-LED board

> **ORDERED 2026-06-07** — JLCPCB, 5 boards, full PCBA (all 5 assembled, top-side
> Economic). Files: `hardware/fab/` (gerbers zip + CPL + BOM). LED substituted to
> **WS2812B-V6 (LCSC C52917433)** — only in-stock option; benign revision swap
> (standard 5050-4P pinout, 800 kHz GRB, V_IH handled by U2; V6's >280 µs reset is
> irrelevant at word-clock update rates). Confirm Production File + Confirm Parts
> Placement both set to Yes. On delivery: JLC placed only the 63 SMD LEDs — hand-
> solder all THT (ESP32 module, RTC/SD/amp headers, U2, R1, C2 + 100 nF caps,
> buttons), then run `firmware/test/hardware_checks/display_checks.md` (esp. the
> D1-glows-RED / GRB color-order check).

Findings from the full design audit (connectivity, power integrity, firmware
parity, ERC/DFM) and the actions to take before fabrication. The board is
electrically complete (0 unconnected, 0 DRC/ERC errors) and verified to light
evenly (worst-case +5V droop ~3 mV, <0.1%). Items below are correctness fixes
and good-practice hardening, in priority order.

## Verification (kicad-mcp-pro, 2026-06-05)

Full MCP validation suite run against the canonical board. **0 DRC errors,
0 unconnected** (confirmed by three independent tools), **0 ERC errors**. The
34 warnings are all benign and individually classified:

- 21 × silkscreen clipped by soldermask — cosmetic.
- 2 × daughtercard footprint ≠ library copy (SD HW-125, RTC DS3231) — cosmetic;
  the board's embedded footprint is authoritative for fab.
- 3 × connector symbol ≠ library (`Conn_01x06/07`) — cosmetic, pre-existing.
- 7 × "tri-state and power input connected" — U2's 7 unused inputs (A2–A8)
  correctly tied to GND. ERC pin-matrix false positive, not a defect.
- 1 × "74HCT245 not found in 74xx" — the 74HC→74HCT rename; the schematic's
  embedded symbol copy is electrically valid (this KiCad's `74xx` lib ships HC,
  not HCT). Optional cleanup: point the `lib_id` at `74xx:74HC245` (pin-identical)
  and keep `Value`=74HCT245 to silence the warning.

DFM "FAIL" lines are tool artifacts: "via drill -0.000 mm" is a parse bug
(`get_board_stats` shows real via drills at 0.300 mm); "DRC violations: 23"
re-counts the warnings above. Footprint-vs-schematic "FAIL" is only H1–H4
(NPTH mounting holes, no schematic symbol by design).

**Level shifter U2 (74HCT245) wiring verified from the board netlist:**
DIR=+5V (A→B), OE#=GND (enabled), A1=/LED_DATA_3V3 in, B1=/LED_DATA_5V out
(matched channel), unused inputs→GND, unused outputs floating. Data path:
`ESP32 GPIO13 → A1(3.3V) → B1(5V) → R1 300Ω → D1 DIN`.

**Firmware↔PCB parity verified:** the 35 per-word LED counts in
`firmware/lib/display/src/led_map.cpp` match the PCB's LED `Value` fields
exactly (35 words / 63 LEDs), and the spans tile the chain [0,63) with no gaps
or overlaps.

**LED grid + chain fully verified (2026-06-06):**
- Every LED is on a clean 13.68 mm row lattice; all **63/63 sit on the correct
  grid row** for their word, and **63/63 fall within the correct word's column
  span** (checked against `firmware/lib/core/src/grid.cpp`).
- The **physical daisy-chain (DOUT→DIN) order exactly matches the firmware
  `led_map` index order** (0 mismatches): data enters at D1 (IT, ← R1), exits at
  D63 (tail, DOUT open). So firmware index i lights the correct physical letter.
- CPL placement positions match the PCB footprint positions for all 63 LEDs.

Remaining parity is physical-only — **GRB color order** and face-overlay
registration — bench checks in
`firmware/test/hardware_checks/display_checks.md` (§2–§3).

## Already done (in this change)

- **Reconciled the rerouted board into canonical** `hardware/word-clock.kicad_pcb`
  (it was the old 15-unconnected hand-routed board; now it's the rerouted
  0-unconnected one, 187 vias). Verified by kicad-cli: **0 DRC violations,
  0 unconnected, 0 ERC errors.** Pre-reconcile board saved as
  `.pre-reconcile-*` if you need to compare.
- **74HC245 → 74HCT245** (U2 level shifter): the ESP32 drives the LED-data
  input at 3.3 V, below the 74HC high threshold (~3.5 V at 5 V VCC). 74HCT's
  threshold (~2.0 V) is in spec. Updated in the schematic, **PCB silk**, master
  BOM (`hardware/word-clock.csv`), and datasheet link.
- **Regenerated all fab exports for 63 LEDs** (the old ones were stale at 35),
  in `hardware/regen/`:
  - `word-clock-cpl.csv` — JLCPCB placement, 63 LEDs. **Verified:** transform
    reverse-engineered from the known-good 35-LED CPL and confirmed (D1 matches
    exactly). All 63 LEDs are **rotation 0, layer Top (F.Cu)** — uniform, no
    finicky rotations.
  - `word-clock-pcba-bom.csv` — 63 designators, LCSC C114586.
  - `word-clock-bom.csv` / `word-clock-pos.csv` — full BOM / raw KiCad pos
    (reference). NB: the raw KiCad `pos` export labels the LEDs "bottom" — that
    is a CLI artifact; the LEDs are on **F.Cu = Top** (the CPL is authoritative).
  Replace the stale `word-clock-*-pos.csv` / `word-clock-pcba-bom.csv` with
  these after the board reconciliation (step 1) and re-export the pos from the
  reconciled board so positions are sourced from the canonical file.
- **Firmware reconciled to 63 LEDs** (separate change): `LED_COUNT=63`,
  per-word LED spans derived from the PCB. Words now light fully and evenly.

### Decouple U2 — C1 moved +3V3 → +5V  (DONE 2026-06-06)
C1 (100 nF) sits **3.8 mm from U2** but was wired to **+3V3**, while U2 is a
**+5V** part and the ESP32 (the only +3V3 IC) is 80 mm away — a mis-railed
decoupling cap for U2. **Fixed and verified** (0 ERC errors, 0 DRC errors,
0 unconnected):
- Schematic: C1's `+3V3` pin detached and connected to a new `+5V` symbol.
  The +3V3 PWR_FLAG (`#FLG01`) was left in place on the remaining +3V3 stub,
  so +3V3 stays driven/flagged — the failure mode that broke an earlier
  by-hand swap (FLG short + `#PWR021` undriven) is avoided.
- PCB: C1 pad 1 re-netted to +5V and connected by a short F.Cu track straight
  to U2 pin 1 (+5V), 3.8 mm away — a low-inductance decap tie at U2's power
  pin. C1's orphan +3V3 branch (3 segments) was removed, each DRC-confirmed
  dangling; J_RTC1, C4, and the ESP32 stayed connected throughout. Zones
  refilled.

## Must do before fab

(none — the items below are resolved or judgment calls.)

### 3. Confirm daughtercard onboard pull-ups (assembly-time)
The main PCB has **no** I²C pull-ups and does not tie the MAX98357A SD_MODE —
both rely on the breakout modules:
- **RTC (ZS-042 / DS3231):** confirm it populates the onboard 4.7 k SDA/SCL
  pull-ups (most do). Without them, I²C floats and the RTC is unreadable.
- **Amp (MAX98357A breakout):** SD pin (shutdown + L/R select) and GAIN are
  left open, relying on the breakout's onboard pull-ups (SD pulled up =
  enabled, (L+R)/2 mono). Confirm the specific breakout populates them, or
  tie SD to a defined level — it doubles as a useful software-mute line.

Buttons need no external pull-ups: firmware sets `INPUT_PULLUP` on
GPIO32/33/14 (verified in `firmware/lib/buttons`).

## Optional / judgment calls

### 4. Input protection — optional, low marginal value
The audit flagged no fuse / reverse-polarity at the +5 V entry (back-fed
through the ESP32 dev-board VIN pin). In practice this is largely covered by
the USB-C supply's own limiting and the dev-board's USB protection, and
reverse polarity is not possible with USB-C. If you want belt-and-suspenders
on a 40-year keepsake, add a **polyfuse (~2 A)** in series at the +5 V entry
(a series Schottky / ideal-diode P-FET would add reverse-polarity protection
but costs ~0.3 V and board space). This adds parts + routing on a full
board — do it deliberately in KiCad, not as a quick patch.

### 5. Solid +5 V plane re-pour — not recommended
CLAUDE.md's intent was a solid B.Cu +5 V plane; the autoroute carved it into
one large island + a few bridged fragments. **This is already verified
electrically fine** (even-lighting droop ~3 mV; 54/63 LEDs on the main plane,
the rest behind low-current bridges). Re-routing a verified-working board to
chase a prettier plane risks regression for no functional gain. Skip unless
there's a specific reason.
