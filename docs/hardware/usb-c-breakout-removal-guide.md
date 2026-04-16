# KiCad Rework Guide — Remove Cermant USB-C breakout, power via ESP32 module

**Why:** The Cermant USB-C breakout (`J_USB1`) was planned as the user-facing port but never got data lines routed — CC resistors + VBUS + GND only, no D+/D−. The ESP32 module already has its own micro-USB with a CP2102 USB-UART bridge that handles both power and flashing. The Cermant adds no function and needs a hole in the back panel we don't want to design around. Remove it; use a panel-mount `USB-C female → micro-USB male` pigtail to expose the module's native micro-USB as a USB-C port on the back panel.

**Starting state:** `J_USB1` at PCB position (33.5, −7.46) on the bottom side, wired to the +5V rail via VBUS. `C2` 1000µF electrolytic at (25.5, −18.5) sits right next to it as the LED strip's bulk cap.

**End state:** `J_USB1` and its footprint are gone. +5V rail is fed from the ESP32 module's `5V` / `VIN` pin (which is in turn fed from the module's own micro-USB). `C2` bulk cap still sits on the 5V rail and is still physically close to the LED strip inputs. ERC clean, DRC clean, gerbers re-exported.

## Target hardware

- **ESP32 module:** AITRIP ESP-WROOM-32 3-pack (Amazon). Classic ESP32 with CP2102 USB-UART bridge and a micro-USB connector on the module board. 38-pin dev board. Module designator in our schematic: `U1`.
- **Panel-mount adapter** (to be sourced separately — not on the PCB): USB-C female (panel mount) to micro-USB male pigtail, 15–30 cm length, USB 2.0 data capable, with CC resistor pull-downs on the USB-C side. 22–24 AWG power conductors preferred. Example search term on Amazon / Adafruit / Digi-Key: "USB-C panel mount to micro-USB pigtail data."

## Pre-reqs

- `git status` clean or your pending changes saved.
- Close KiCad cleanly before starting if it's open from a previous session (it takes a file lock).
- Read `docs/hardware/kicad-rework-guide.md` once for the general rhythm — this guide follows the same structure.

---

# Part 1: Schematic changes

## 1.1: Locate and delete `J_USB1`

Open `hardware/word-clock.kicad_sch` in KiCad Schematic Editor.

1. Find the symbol labeled `J_USB1` with Value `USB-C breakout (Cermant 16P)`. It has 16 pins including `VBUS`, `GND`, `CC1`, `CC2`, `D+`, `D−`, and several shield pins.
2. Click the symbol. Press **Delete**.
3. Any wires or labels that remain connected only to `J_USB1` stubs will become floating. Delete them too — specifically any `VBUS` labels, any wire running from `J_USB1` VBUS pin to the `+5V` net, and the `GND` wire from `J_USB1` GND pin.

**Do NOT delete:**
- The `+5V` net label itself anywhere else it appears (it's still the main 5V rail).
- `C2` (1000µF bulk cap) — keep it.
- The `GND` net or `GND` labels.
- Any other passives near where `J_USB1` was.

## 1.2: Verify the ESP32 module's 5V pin feeds the +5V rail

On the `U1` (ESP32) symbol, find the pin labeled `5V` or `VIN` (pin number depends on the symbol — it's one of the outer pins on the 38-pin module; check existing wiring). This pin should already have a wire to the `+5V` net label. If it does, **good — nothing to change.** If somehow it doesn't (would be unusual), add a `+5V` power label to that pin now.

This pin is bidirectional on the AITRIP module: when the module's micro-USB is plugged in, it sources 5V out on this pin; when 5V is injected here externally, it powers the module. Our new architecture uses it as an output (micro-USB in, 5V out to the main rail).

## 1.3: Verify `C2` still sits on the +5V rail

`C2` should have one terminal on `+5V` and the other on `GND`. It does, pre-edit. Confirm visually.

## 1.4: Delete orphaned `VBUS` net

The `VBUS` net was specific to the Cermant breakout. If KiCad shows `VBUS` as an orphaned net after the deletion (i.e., referenced nowhere), it goes away automatically on save. Nothing to do.

## 1.5: Run ERC

**Inspect → Electrical Rules Checker → Run.**

Expected result: clean, same baseline warnings as before the edit. If new "unconnected pin" warnings appear, they're the pins of `J_USB1` that lost their labels — should have been deleted in step 1.1. Re-check.

## 1.6: Save + regenerate PDF

- **Cmd-S** to save the schematic.
- **File → Plot → PDF** to regenerate `hardware/word-clock.pdf`.

---

# Part 2: PCB changes

## 2.1: Update PCB from Schematic

Open PCB Editor. **Tools → Update PCB from Schematic (F8).**

This pushes the deletion to the board. Click **Update PCB**. After it runs, `J_USB1`'s footprint should be gone. If it isn't, the footprint will have turned red (marked as "no schematic symbol") — delete it manually.

## 2.2: Delete orphaned traces from `J_USB1`'s old pads

The `J_USB1` footprint was at (33.5, −7.46). Even after the footprint is gone, traces that used to run from its `VBUS` pad to the rest of the +5V rail may be left as dangling stubs. Find and delete them:

1. **Select → All Tracks in Net → +5V** is helpful to visualize the whole rail at once.
2. Any `+5V` trace segment that terminates in empty space near the (33.5, −7.46) region is orphaned. Delete.
3. Same check for `GND` traces from the old `J_USB1` GND pad, though the GND pour will mostly mask this.

**Do NOT delete:**
- Trace from `C2` to the +5V rail.
- Trace from the ESP32 module's 5V/VIN pin to the +5V rail.
- Any other 5V traces feeding the LEDs, the MAX98357A, or the 74HC245.

## 2.3: Re-route or confirm +5V feeds everything

After the Cermant is gone, the +5V rail's power source is the ESP32 module's 5V pin. Walk the rail visually from that pin outward and confirm every previously-connected load still has a path:

- `C2` (1000µF bulk cap)
- WS2812B strip +5V pads (all 35 LEDs)
- `U2` (74HC245) Vcc pin
- `MAX98357A` VIN pin (breakout header)
- `R1` (300Ω) — actually sits between 74HC245 output and LED data, not on the 5V rail. Ignore.

If the rail is broken anywhere, press **X** and re-route with 0.5–1.0 mm trace width (match surrounding style).

## 2.4: Refill ground pour

Press **B** to refill zones. This rebuilds the GND plane around the now-empty Cermant area, closing any holes in the ground pour left behind by deleted traces.

## 2.5: Run DRC

**Inspect → Design Rules Checker → Run DRC.**

Target: 0 errors. Fix any new unconnected-pad warnings by routing (likely none, since we only deleted things).

## 2.6: Re-export fabrication outputs

- **File → Plot →** Gerbers. Layers per the original `kicad-rework-guide.md` step 2.11.
- **Generate Drill Files.**
- **Component Placement → CSV** to regenerate `hardware/word-clock-all-pos.csv`.
- **Tools → Generate Legacy BOM → CSV** to regenerate `hardware/word-clock.csv`.

```bash
cd hardware
zip gerbers.zip *.gbr *.gbrjob *.drl
```

## 2.7: Commit

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add hardware/ docs/hardware/
git commit -m "fix(hardware): remove cermant usb-c breakout, power via esp32 micro-usb"
```

---

# Part 3: Downstream edits (not KiCad)

## 3.1: Update `docs/hardware/pinout.md`

The Power section opens with "Input: USB-C 5V via USB-C breakout". That's now wrong. Change it to:

> **Input:** USB-C from the back panel via a panel-mount USB-C-female-to-micro-USB-male pigtail, plugged into the ESP32 module's on-board micro-USB port. The CP2102 bridge on the module handles enumeration for firmware flashing; the module's 5V pin feeds the main +5V rail.

Keep the power budget + mitigation discussion — still applies.

## 3.2: Update `docs/hardware/phase-2-parts-list.md`

The USB-C breakout line item is now obsolete. Either delete it or note it as "not populated — superseded by USB-C-to-micro-USB panel-mount pigtail."

Add a new line item for the pigtail adapter. Example:

> **USB-C panel mount to micro-USB male pigtail — ~$6-10**
> 15-30 cm USB 2.0 pigtail with a panel-mount USB-C female on one end and a micro-USB male plug on the other. Required so the user-visible port on the back panel is USB-C while the ESP32 module's native connector stays micro-USB. Must carry both USB 2.0 data (for firmware flashing) and sufficient current for the clock's worst-case draw; look for 22-24 AWG conductors on the power pair.

## 3.3: Verify the AITRIP module's USB input can carry system current

One remaining open question: does the AITRIP ESP-WROOM-32 have a polyfuse on its USB VBUS input? Some cheap clones do (often 500 mA), others route VBUS straight through. A 500 mA fuse will nuisance-trip when LEDs + audio run together.

When parts arrive, inspect the module under a magnifier near the micro-USB connector:
- Small 0603 / 0805 yellow or green component in series with VBUS: polyfuse (bad for us).
- Zero-ohm jumper or direct trace: fine.
- A small SMD diode: Schottky for reverse-polarity protection, fine.

If a polyfuse is present, either (a) bridge it with a solder blob and a bit of wire, (b) replace it with a larger-rated polyfuse, or (c) re-introduce a power-only 5V inlet on the PCB (a 2-pin JST-XH or screw terminal) that feeds the +5V rail directly, leaving the module's micro-USB for data-only. This decision should be made before committing to the gerbers for fab.

## 3.4: Plan the back-panel cutout location

The ESP32 module is at PCB position (73, −72.5) with 180° rotation on the bottom side. The micro-USB connector faces parallel to the PCB surface (toward an edge, not toward the back panel).

Two options:
1. **Cutout in a frame strip** — breaks the "frame is a bare shell" decision; requires re-ordering the frame. **Avoid.**
2. **Internal pigtail routes inside the case** — the micro-USB male end of the panel-mount pigtail plugs directly into the module's connector, and the USB-C female end is mounted in the back panel at whatever position is most convenient. This is the intended approach. The pigtail is a short internal cable, fully hidden.

Pick the back-panel USB-C mounting position in the back-panel SVG design (tracked separately in `TODO.md` / back panel design task). Keep the pigtail reach in mind (15-30 cm gives plenty of slack for any placement).
