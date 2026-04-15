# KiCad Schematic Build Guide — v0

**Target version:** KiCad 10.x (released 2026; current stable at time of writing).

**Audience:** Software engineer with zero KiCad experience. Comfortable with technical tools generally. Needs to know where to click.

**Goal of this session:** Produce `hardware/word-clock.kicad_sch` — an electrical schematic that KiCad can run ERC (Electrical Rules Check) on, with no errors. This is activity B in `docs/superpowers/specs/2026-04-15-activity-blocking-graph.md`. It does NOT include PCB layout — that's a later session.

**Estimated time:** ~90 minutes first time through. Take a break between sections if your brain gets foggy — forcing progress on a confused brain in KiCad will produce errors you won't find until PCB layout.

**Key simplification (important):** We're representing each breakout board (MAX98357A, DS3231, microSD, USB-C, ESP32 dev board) as a **single "connector" symbol** with its pin labels — not as the actual chip with all its surrounding support components. This matches physical reality: we're soldering breakouts onto a custom PCB, not populating bare chips. It is **much** simpler than a full chip-level schematic and it's what we actually want.

---

## Part 0: What KiCad is and how it works

Skim this. You'll refer back.

KiCad is an electronic design automation (EDA) tool. It has four programs you'll touch, bundled in one app:

1. **KiCad** (the project manager) — launches the others
2. **Schematic Editor** (`eeschema`) — where you draw the logical circuit. Symbols represent components; wires represent electrical connections. **This session is entirely in the Schematic Editor.**
3. **PCB Editor** (`pcbnew`) — where you draw the physical circuit board. Not this session.
4. **Symbol Editor / Footprint Editor** — where you make custom symbols or footprints. We'll use stock ones; not this session.

### Mental model

- A **symbol** is a schematic drawing of a component (a rectangle with labeled pins). Stored in a **symbol library**.
- A **footprint** is the physical layout of that component's pads on the PCB. Stored separately in a **footprint library**. You assign a footprint to each symbol later, before PCB layout.
- A **net** is a named electrical connection. Two pins on the same net are wired together. You can connect them by drawing a wire, OR by giving both pins the same **net label** (easier for buses).
- **ERC (Electrical Rules Check)** is the "compiler" of the schematic. It catches unconnected pins, pins driving against each other, and missing power connections. Run it often.

### File layout

Everything goes in `hardware/` at the repo root. KiCad makes these files:
- `word-clock.kicad_pro` — project file
- `word-clock.kicad_sch` — schematic (this session's output)
- `word-clock.kicad_pcb` — PCB layout (later session)
- `sym-lib-table`, `fp-lib-table` — per-project library references
- `*.kicad_sym`, `*.kicad_mod` — custom symbol/footprint libraries (we likely won't need to create any)

All of these are plain-text (S-expression format). Git-friendly. Commit as you go.

---

## Part 1: Install KiCad and create the project

### Step 1.1 — Install KiCad 10.x

macOS: download from https://www.kicad.org/download/macos/ — get the **stable** build (10.x at time of writing).

Alternative with Homebrew:

```bash
brew install --cask kicad
```

Verify: open KiCad.app. The project manager window opens with a sidebar of editors (Schematic Editor, PCB Editor, etc.).

**KiCad 10 notes relevant to this guide:**
- Keyboard shortcuts referenced below (A, L, P, W, E, M, R, Q, Escape) are stable and unchanged from KiCad 6–9.
- Symbol library names (`Connector_Generic`, `Device`, `74xx`, `Switch`) are unchanged.
- File format (`.kicad_sch`, `.kicad_pcb` — S-expression) is unchanged since KiCad 6.
- KiCad 10 added a lasso selection tool, dark mode on Windows, and importers for Allegro/PADS/gEDA — none affect this guide.
- Menu paths (File, Inspect, etc.) are the same as KiCad 8/9. If a specific menu item seems to have moved, use the command palette: **? key** or **Ctrl/Cmd-K** (if available in KiCad 10) to search.

### Step 1.2 — Create the project

In the repo:

```bash
mkdir -p hardware
```

In KiCad: **File → New Project**. Browse to `/Users/samueldacanay/dev/personal/word-clock-for-my-daughters/hardware/`. Name the project `word-clock`. Leave "Create a new folder for the project" UNCHECKED (we already made `hardware/`).

**Template selection:** if KiCad prompts you to pick a template (Arduino shield, Raspberry Pi HAT, etc.), choose **"Empty Project"** or skip the template. Our clock is a standalone board — not a daughter card for another system — so we want a blank starting point. Template-based projects pre-populate the schematic with the host's connector, which we don't need.

This creates `hardware/word-clock.kicad_pro` and empty `.kicad_sch` / `.kicad_pcb` files.

### Step 1.3 — Commit the project skeleton

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/
git commit -m "chore: scaffold kicad project"
```

(Yes, an empty KiCad project. Gets the directory in git before we fill it.)

---

## Part 2: Symbols we'll need

This is our parts list translated into KiCad symbol terms. I've picked library symbols you can find without hunting.

| Real part | KiCad symbol | Library | Notes |
|---|---|---|---|
| ESP32 DevKit V1 (AITRIP 3-pack) | `Connector_Generic:Conn_02x19_Odd_Even` (38-pin dual-row) | built-in | We'll label each pin with its signal manually |
| MAX98357A breakout | `Connector_Generic:Conn_01x07` (7-pin single-row header) | built-in | LRC, BCLK, DIN, GAIN, SD, GND, VIN |
| DS3231 ZS-042 breakout | `Connector_Generic:Conn_01x06` (6-pin single-row header) | built-in | 32K, SQW, SCL, SDA, VCC, GND |
| HW-125 microSD breakout | `Connector_Generic:Conn_01x06` (6-pin single-row header) | built-in | GND, VCC, MISO, MOSI, SCK, CS |
| USB-C breakout (Cermant 16P) | `Connector_Generic:Conn_01x04` (4-pin single-row header) | built-in | We'll label: VBUS, GND, CC1, CC2. The physical breakout has 16 pads; we only use 4. Footprint (on PCB) will match all 16 pads later — that's a PCB-layer concern, not schematic. |
| WS2812B strip (external) | `Connector_Generic:Conn_01x03` (3-pin single-row header) | built-in | +5V, GND, DATA |
| Tact switch × 3 | `Switch:SW_Push` | built-in | Standard momentary push button |
| 74HCT245 level shifter | `74xx:74HC245` (symbol) — set **Value** to `74HCT245` | built-in | 20-pin octal bus transceiver. KiCad has one schematic symbol per base part (74HC245 = 74HCT245 pinout); HC vs HCT family is a BOM concern. We only use 1 of 8 channels; rest tied off. |
| 100nF ceramic cap (×5-8) | `Device:C` | built-in | Decoupling near each IC |
| 1000µF electrolytic cap | `Device:CP` (polarized) | built-in | Bulk cap near LED strip |
| 4.7kΩ resistor (×2) | `Device:R` | built-in | I²C pullups — only if DS3231 breakout doesn't have them |
| 300Ω resistor | `Device:R` | built-in | Series on WS2812B data line |
| 10kΩ resistor (×3) | `Device:R` | built-in | External pullups on tact switches — but `INPUT_PULLUP` in firmware makes these optional |

### Step 2.1 — Open the Schematic Editor

From the KiCad project manager, double-click `word-clock.kicad_sch` in the sidebar. Empty schematic sheet opens.

Key navigation:
- **Scroll wheel:** zoom
- **Right-click + drag** or **middle-click + drag:** pan
- **Escape:** cancel current action (use this when you get confused)
- **Delete key:** remove selected item

---

## Part 3: Place symbols

Work top-to-bottom roughly in this order. Left side of sheet for inputs (USB-C, buttons), middle for ESP32, right side for outputs (LEDs, audio, RTC, SD).

### Step 3.1 — Place the ESP32 dev board connector

1. Press **A** (Add Symbol). Dialog opens.
2. In the filter/search box, type `Conn_02x19_Odd_Even`.
3. Select it. Click **OK**.
4. Click on the sheet around the center to place it. Press **Escape** to stop placing.

It appears as a 2×19 pin rectangle. We'll label each pin below.

Select the symbol, press **E** (Edit Properties). Set:
- **Reference:** `U1` (auto-assigned, leave as-is or rename to `ESP32`)
- **Value:** `ESP32 DevKit V1`

### Step 3.2 — Rename the ESP32 pin names to match the silkscreen

The AITRIP ESP-WROOM-32 3-pack follows the **DOIT ESP32 DEVKIT V1 38-pin layout**. Before wiring, rename each pin of the connector symbol from the default `1, 2, 3...` to the silkscreen label printed on the physical board. This makes the schematic self-documenting.

Select the ESP32 connector, press **E** (edit properties). Find the pin list (labeled "Pins" or "Edit Pin Properties" depending on KiCad version). Rename pins in this order (1 through 38, matching the DOIT DEVKIT V1 layout):

| Pin # | New name | GPIO | Our use |
|---:|---|---|---|
| 1 | `3V3` | power output | +3V3 rail source |
| 2 | `EN` | reset | unused |
| 3 | `VP` | GPIO36 (input-only) | unused |
| 4 | `VN` | GPIO39 (input-only) | unused |
| 5 | `D34` | GPIO34 (input-only) | unused |
| 6 | `D35` | GPIO35 (input-only) | unused |
| 7 | `D32` | GPIO32 | BTN_HOUR |
| 8 | `D33` | GPIO33 | BTN_MINUTE |
| 9 | `D25` | GPIO25 | I2S_LRC |
| 10 | `D26` | GPIO26 | I2S_BCLK |
| 11 | `D27` | GPIO27 | I2S_DIN |
| 12 | `D14` | GPIO14 | BTN_AUDIO |
| 13 | `D12` | GPIO12 (strapping) | reserved |
| 14 | `GND` | ground | GND rail |
| 15 | `D13` | GPIO13 | LED_DATA_3V3 |
| 16 | `D9` | GPIO9 (flash — don't use) | — |
| 17 | `D10` | GPIO10 (flash — don't use) | — |
| 18 | `D11` | GPIO11 (flash — don't use) | — |
| 19 | `VIN` | 5V input | (optional 5V path) |
| 20 | `GND` | ground | GND rail |
| 21 | `D23` | GPIO23 | SD_MOSI |
| 22 | `D22` | GPIO22 | I2C_SCL |
| 23 | `TX0` | GPIO1 | UART debug (reserved) |
| 24 | `RX0` | GPIO3 | UART debug (reserved) |
| 25 | `D21` | GPIO21 | I2C_SDA |
| 26 | `GND` | ground | GND rail |
| 27 | `D19` | GPIO19 | SD_MISO |
| 28 | `D18` | GPIO18 | SD_SCK |
| 29 | `D5` | GPIO5 | SD_CS |
| 30 | `TX2` | GPIO17 | unused |
| 31 | `RX2` | GPIO16 | unused |
| 32 | `D4` | GPIO4 | unused |
| 33 | `D0` | GPIO0 (strapping) | unused |
| 34 | `D2` | GPIO2 (strapping + LED) | unused |
| 35 | `D15` | GPIO15 (strapping) | unused |
| 36 | `D8` | GPIO8 (flash — don't use) | — |
| 37 | `D7` | GPIO7 (flash — don't use) | — |
| 38 | `D6` | GPIO6 (flash — don't use) | — |

After this rename, each pin shows its silkscreen name in the schematic. You can then attach net labels in Part 4 by matching signal name to pin name (e.g., `LED_DATA_3V3` goes on the pin named `D13`).

**Verify when the board arrives:** confirm the silkscreen labels match this table. AITRIP is a DOIT clone but occasional batches vary — if physical pin 15 says "D13" you're good.

### Step 3.3 — Place MAX98357A breakout

Press **A**. Search `Conn_01x07`. Place it to the right of the ESP32. Press Escape.

Select it, press **E**. Set:
- **Reference:** `J_AMP`
- **Value:** `MAX98357A breakout`

To label the individual pins of this connector (the 7 pins on the board):

1. Select the symbol, press **E**.
2. In the properties dialog, find the **Pin numbers/names** section (might be under a sub-dialog — "Edit pin properties" or similar depending on KiCad version).
3. Set pin 1 name to `LRC`, pin 2 to `BCLK`, pin 3 to `DIN`, pin 4 to `GAIN`, pin 5 to `SD`, pin 6 to `GND`, pin 7 to `VIN`.

If that's too fiddly, alternative: leave pin names as `1-7` and write small text labels next to each pin later. Both work; pin names are just more self-documenting.

### Step 3.4 — Place DS3231 breakout

Press **A**. Search `Conn_01x06`. Place it below the MAX98357A. Press Escape.

- **Reference:** `J_RTC`
- **Value:** `DS3231 ZS-042`
- Pin names: `32K`, `SQW`, `SCL`, `SDA`, `VCC`, `GND`

### Step 3.5 — Place microSD breakout

Same steps. Label:
- **Reference:** `J_SD`
- **Value:** `HW-125 microSD`
- Pin names: `GND`, `VCC`, `MISO`, `MOSI`, `SCK`, `CS`

### Step 3.6 — Place USB-C breakout

Press **A**. Search `Conn_01x04`. Place it.
- **Reference:** `J_USB`
- **Value:** `USB-C breakout (Cermant 16P)`
- Pin names: `VBUS`, `GND`, `CC1`, `CC2`
- The Cermant board physically has 16 pads; we only need 4. The unused 12 pads will be left unconnected on the final PCB (or broken out to extra test points if convenient).

### Step 3.7 — Place WS2812B strip connector

Press **A**. Search `Conn_01x03`. Place it.
- **Reference:** `J_LED`
- **Value:** `WS2812B strip`
- Pin names: `+5V`, `GND`, `DATA`

### Step 3.8 — Place 3 tact switches

Press **A**. Search `SW_Push`. Place 3 of them, one at a time. Press Escape after each.

- **References:** `SW1`, `SW2`, `SW3` (auto-numbered)
- **Values:** `Hour`, `Minute`, `Audio` (set via **E** for each)

### Step 3.9 — Place 74HCT245 level shifter

Press **A**. Search **`74HC245`** (no T — KiCad uses one symbol for both HC and HCT families; they have identical pinouts). Place it between the ESP32 and the WS2812B strip connector.
- **Reference:** `U1`
- **Value:** `74HCT245` (override the symbol's default `74HC245` — HCT is the family we need for TTL-compatible 3.3V input thresholds)

The 74HCT245 is an **octal bus transceiver** — 8 bidirectional channels. Each channel has an `A` pin and a `B` pin; direction is set by a single shared `DIR` (also labeled `A→B` on the KiCad symbol) pin:
- `DIR = HIGH`: A inputs → B outputs (what we want — ESP32 3.3V drives A, shifted 5V comes out B)
- `DIR = LOW`: B inputs → A outputs (the other direction; not our case)

One shared `CE` (chip enable, also labeled `/OE`) — active low. Tie to GND to enable all channels.

Shared `Vcc` (5V) and `GND` pins.

**We only use channel 0:** `A0` → GPIO13 LED data, `B0` → (through 300Ω series resistor) → WS2812B DIN.

**Unused channels (A1-A7 inputs, B1-B7 outputs):**
- Tie `A1-A7` all to `GND` (floating CMOS inputs waste power and oscillate)
- Leave `B1-B7` unconnected (outputs can safely float)

We'll wire these in Part 4.

### Step 3.10 — Place passive components

For now, just place them on the sheet — we'll wire them in Part 4.

Press **A**, search:
- `C` (non-polarized cap) — place 6 of them. Default value `C`, set to `100nF` via **E** → Value.
- `CP` (polarized cap) — place 1. Default value `CP`, set to `1000µF / 10V`.
- `R` (resistor) — place 1 for the 300Ω series (WS2812B data), 2 for I²C pullups (may remove later), 3 for tact switch pullups (optional). Set values: `300`, `4.7k`, `10k`.

Don't worry about exact placement yet — drag with **M** (move) later.

---

## Part 4: Wire everything up with labels, not lines

For small schematics you can draw wires with the **W** tool. For our project with ~15 nets across 8 symbols, **labels are much cleaner** — you don't have wires crossing the whole sheet.

### Rule

Any two pins with the **same text label** are on the same electrical net. No wire needed.

Labels come in three flavors:
- **Local label** (`L` key): scoped to the current sheet
- **Global label** (Ctrl-L): visible across all sheets (we have one sheet, so same as local for us)
- **Power port** (`P` key): special labels for GND, +3V3, +5V, VCC — visually different, semantically identical to labels

Use **power ports** for GND, +3V3, +5V. Use **local labels** for everything else.

**Important note about +3V3:** the ESP32 dev board has its OWN onboard 3.3V regulator (AMS1117-3.3) that takes the 5V USB input and produces the 3.3V rail. When we connect the ESP32's `3V3` pin to our `+3V3` net, the ESP32 is the **source** — not a consumer. Everything else on the +3V3 net (DS3231 VCC, ESP32 decoupling cap) draws from that source. ERC needs to know this, handled via the `PWR_FLAG` trick in Step 5.2.

### Step 4.1 — Place power ports

Press **P**. Search for each:
- `GND` — place 1 instance near each symbol's GND pin (drag a wire from GND pin, then drop the port)
- `+5V` — place near USB-C VBUS, WS2812B +5V pin, 74HCT245 Vcc, microSD VCC, MAX98357A VIN
- `+3V3` — place near ESP32 3V3 pin, DS3231 VCC

**How to use a power port:** press **P**, pick the power symbol (GND, +5V, +3V3), and place it on the sheet. Then press **W** (wire) and draw a short wire from the symbol's pin to the port. The port itself IS the net label — no separate text needed.

Keep wires short — the labels do the heavy lifting, not the wire geometry.

### Step 4.2 — Label signal nets

For each non-power signal, add a **local label** (press **L**, type the name) at the tip of the symbol's pin. Same label on two pins = electrically connected. No wires needed.

**Mechanics:**
1. Hover at the open (outer) end of a pin.
2. Press **L**. Dialog opens.
3. Type the label name (case-sensitive, no spaces — e.g., `LED_DATA_3V3`).
4. Click to place. The label attaches to the pin.
5. Repeat at the other end — any other pin you want on the same net. The label text must match **exactly**.

The table below assumes you completed Step 3.2 (renaming ESP32 pins to silkscreen names like `D13`, `D22`). Every row tells you the exact pin name on each symbol to attach the label to.

| Net label | ESP32 pin | Breakout / IC pin | Other pins on net |
|---|---|---|---|
| `LED_DATA_3V3` | `D13` on ESP32 | `A0` on U1 (74HCT245) | — |
| `LED_DATA_5V` | — | `B0` on U1 (74HCT245) | one side of R (300Ω) |
| `LED_DATA_OUT` | — | `DATA` on J_LED (WS2812B strip) | other side of R (300Ω) |
| `I2C_SDA` | `D21` on ESP32 | `SDA` on J_RTC (DS3231) | one side of R (4.7kΩ pullup to +3V3 — **only if DS3231 breakout lacks onboard pullups**; verify with a multimeter) |
| `I2C_SCL` | `D22` on ESP32 | `SCL` on J_RTC (DS3231) | one side of R (4.7kΩ pullup to +3V3 — same caveat as SDA) |
| `I2S_BCLK` | `D26` on ESP32 | `BCLK` on J_AMP (MAX98357A) | — |
| `I2S_LRC` | `D25` on ESP32 | `LRC` on J_AMP (MAX98357A) | — |
| `I2S_DIN` | `D27` on ESP32 | `DIN` on J_AMP (MAX98357A) | — |
| `SD_CS` | `D5` on ESP32 | `CS` on J_SD (microSD) | — |
| `SD_MOSI` | `D23` on ESP32 | `MOSI` on J_SD (microSD) | — |
| `SD_MISO` | `D19` on ESP32 | `MISO` on J_SD (microSD) | — |
| `SD_SCK` | `D18` on ESP32 | `SCK` on J_SD (microSD) | — |
| `BTN_HOUR` | `D32` on ESP32 | — | one side of SW1 (other side → `GND` port) |
| `BTN_MINUTE` | `D33` on ESP32 | — | one side of SW2 (other side → `GND` port) |
| `BTN_AUDIO` | `D14` on ESP32 | — | one side of SW3 (other side → `GND` port) |

**ESP32 power + ground pins (use power ports, not labels):**
- Pin `3V3` → attach a `+3V3` power port directly (this is the ESP32's 3.3V output — it drives the +3V3 rail)
- Pin `VIN` → attach `+5V` power port (ESP32 is tolerant of 5V in here; becomes a 5V tap if needed)
- All pins named `GND` (pins 14, 20, 26) → each gets a `GND` power port

**Pins on the ESP32 that stay unconnected:** VP, VN, D34, D35, D12, D9, D10, D11, D8, D7, D6, TX0, RX0, TX2, RX2, D4, D0, D2, D15, EN. Add a **no-connect marker** (press **Q**) on each — this silences ERC's "pin not connected" warning for pins we intentionally leave unused.

**Sanity check after this step:** every wired ESP32 pin should have either a net label (`D13` has `LED_DATA_3V3`, etc.) or a power port (`3V3` has `+3V3`, `GND` has `GND`). Every non-wired pin should have a no-connect marker. No ESP32 pin is bare.

### Step 4.3 — Wire the level shifter correctly

The 74HCT245 is a single-direction-at-a-time octal bus transceiver. Wire it:

- **Vcc (pin 20):** `+5V`
- **GND (pin 10):** `GND`
- **DIR / A→B (pin 1):** `+5V` (selects A inputs → B outputs direction)
- **CE / /OE (pin 19):** `GND` (active-low enable — tie to GND to always enable the transceiver)
- **Used channel — channel 0:**
  - `A0` (pin 2) → net `LED_DATA_3V3` (from ESP32 GPIO13)
  - `B0` (pin 18) → net `LED_DATA_5V` (to the 300Ω resistor, then to WS2812B DIN)
- **Unused A inputs (A1-A7, pins 3-9):** all tied to `GND` via short wires. Floating CMOS inputs waste power and can oscillate. Cleanest way in KiCad: drop a power port `GND` near each, wire them in a vertical bus.
- **Unused B outputs (B1-B7, pins 17-11):** leave unconnected. Press **Q** on each to add a "no connect" marker so ERC doesn't complain.

Rule of thumb: **unused inputs get tied off; unused outputs get no-connect flags.**

### Step 4.4 — Wire the tact switches

Each switch has 2 electrically-meaningful terminals (the other 2 are duplicates). Wire:
- One side: to the net label `BTN_HOUR` / `BTN_MINUTE` / `BTN_AUDIO`
- Other side: to `GND`
- ESP32 firmware uses `INPUT_PULLUP`, so external 10kΩ pullups are optional. You can place them from the BTN_* nets to `+3V3` for belt-and-suspenders or leave them off and rely on firmware. I'd leave them off for cleanliness.

### Step 4.5 — Wire the decoupling caps

Place a 100nF cap between VIN/VCC and GND for each IC with a power pin:
- MAX98357A: cap between `+5V` and `GND` near it
- DS3231: cap between `+3V3` and `GND` near it
- microSD: cap between `+5V` and `GND` near it
- 74HCT245: cap between `+5V` and `GND` near it
- ESP32: cap between `+3V3` and `GND` near it (onboard cap exists on dev board; still add one on PCB)

Bulk cap (1000µF polarized):
- + side: `+5V` near WS2812B strip
- − side: `GND`
- Orientation matters for polarized caps; KiCad marks the + terminal. Double-check before PCB.

---

## Part 5: Run ERC (Electrical Rules Check)

ERC is the compiler. It catches most of the common mistakes.

### Step 5.1 — Launch ERC

**Inspect → Electrical Rules Checker**. A dialog opens. Click **Run ERC**.

### Step 5.2 — Read the results

ERC will list warnings and errors. Typical first-time issues:

| Message | What it means | Fix |
|---|---|---|
| "Pin not connected" on an ESP32 pin | You didn't label every pin on the ESP32 connector | OK to ignore for unused GPIOs; add a "no connect" marker (**Q**) if you want to silence it |
| "Input pin not driven" on a BTN_* net | Switch's other side isn't wired yet | Connect the switch's other side to GND |
| "Power input pin not driven" | +5V or +3V3 net has no power source | The ESP32 dev board's 3V3 pin IS the source; mark it as a power output by editing the ESP32 symbol's pin type, OR add a PWR_FLAG on +3V3 and +5V (PWR_FLAG tells ERC "this net has external power — stop complaining") |
| "Two outputs connected together" | Serious — two things trying to drive the same net. Fix the wiring. | |

The `PWR_FLAG` trick: press **P**, search `PWR_FLAG`, place one on the +5V net (near USB-C VBUS) and one on the +3V3 net (near ESP32 3V3). This silences the "no power source" warnings.

### Step 5.3 — Iterate until ERC is clean

Fix one issue at a time, re-run ERC, repeat until the window shows "0 errors, 0 warnings" (or only warnings you've consciously decided to ignore).

Take notes on anything you ignored and why.

---

## Part 6: Save, export, commit

### Step 6.1 — Save

**File → Save** (or Cmd-S). Saves `word-clock.kicad_sch`.

### Step 6.2 — Export a PDF for human review

**File → Plot → PDF**. Click Plot. Creates `word-clock.pdf` in the project directory. Check it in git too — makes schematic reviews easier without requiring KiCad.

### Step 6.3 — Commit

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/
git commit -m "feat(hardware): schematic v0 — ERC clean, ready for review"
git push
```

---

## Part 7: Stop and request review

Before going to PCB layout, send me the commit SHA and I'll review the schematic against:

1. The pinout (`docs/hardware/pinout.md`) — every pin labeled correctly?
2. Datasheet compatibility (`docs/hardware/research/`) — 74HCT245 wired correctly? DS3231 orientation right?
3. Internal consistency — no floating inputs, no stacked pullups, correct cap orientations
4. Completeness — every signal in the pinout represented on the schematic

I'll reply with a checklist of findings (blockers / major / minor). Fix issues, re-run ERC, commit, repeat until clean. Then we move to PCB layout (a separate guide).

---

## If you get stuck

**General rule:** Press **Escape** liberally to cancel an action. Right-click on things to see context menus. Hover and press **E** to edit whatever's under the cursor.

**Common gotchas:**

- **Symbol not appearing in search:** press **A** then check the "Symbol filter" field at the top of the dialog — it may have stale text. Clear it. If libraries aren't loaded: **Preferences → Manage Symbol Libraries** and confirm the global library table has entries for `Connector_Generic`, `Switch`, `74xx`, `Device`.
- **Can't drop a wire:** wires snap to grid. If a symbol's pin is off-grid, **M** to move the symbol and press **R** to rotate 90° until it aligns.
- **Label not connecting to pin:** the label must be placed exactly on the tip of the pin (the open end, not the symbol body end). If it's even one grid square off, they're not electrically connected. Zoom in to verify.
- **ERC reports "pin not connected" on something you did connect:** you probably labeled it on the wrong end of a wire. Delete the wire, redo.

**Nuclear option if KiCad crashes:** the project files are plain text. Git will save you. If something goes sideways, `git status` to see what changed, `git checkout hardware/word-clock.kicad_sch` to revert.

---

## What this guide does NOT cover

(Intentionally out of scope for this session. Each is a separate future guide.)

- Footprint assignment (matching each schematic symbol to a physical PCB footprint)
- PCB layout (placing components on the board, routing traces)
- Design Rules Check (DRC) — the PCB version of ERC
- Gerber export (the files JLCPCB fabs from)
- BOM generation (the parts list JLCPCB uses for assembly)

We'll tackle PCB layout after the schematic is reviewed clean.
