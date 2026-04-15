# Phase 2 Parts List — Breadboard Bring-Up

**Gate:** Before Phase 2 of `docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md` can start. Phase 1 firmware logic is complete; this order unblocks hardware bring-up on a breadboard (no PCB yet).

**Budget:** ~$60-85 total for one breadboard kit. Only one set needed — Emory and Nora use the same hardware, differing only by compile-time config.

**Where we are in the roadmap:**

| Phase | Status |
|---|---|
| 0 — Repo + CI | ✅ Done |
| 1 — Native firmware logic | ✅ Done |
| **2 — Breadboard bring-up** | **← Blocked on this parts order** |
| 3 — KiCad PCB v1 | Needs Phase 2 learnings |
| 4 — Enclosure CAD | Needs PCB dimensions |
| 5 — Emory prototype | Needs PCB + enclosure |
| 6 — Nora unit | Needs Emory validated |

## Parts

### ESP32 DevKit V1 — **$8-12** × 1

- **What it is:** Small development board with an ESP32-WROOM-32 module, USB-to-serial chip (CP2102 or CH340), voltage regulator, and pin headers. Just plug into USB, flash firmware, go.
- **Why this clock needs it:** The ESP32 is the brain. It runs the firmware we built in Phase 1, drives the LEDs, talks WiFi for NTP time sync, plays audio over I2S, and reads button presses. Spec locks ESP32 because of the combination of WiFi + I2S audio + mature Arduino libraries.
- **What to look for:** Any variant with 30 or 38 pins, USB-C preferred (matches the final design). ESP32-WROOM-32E chip (the E is the 2020+ revision). Avoid ESP32-S2 / -S3 / -C3 variants for now — they have different pinouts and library quirks; stick with classic ESP32 to match the spec.
- **Sourcing:** Amazon Prime — "ESP32 DevKit V1 USB-C". Prime 2-day is worth the markup here.

### WS2812B LED strip, 1 meter @ 60 LEDs/m — **$8-12** × 1

- **What it is:** A flexible PCB strip with 60 addressable RGB LEDs per meter. Each LED has a tiny WS2812B controller chip built in; they chain together and you control them all over a single data line.
- **Why this clock needs it:** One LED per word group (~25 LEDs per clock). The letter grid sits behind a wood face with cut-through letters; each LED glows through one word's letters when that word is "on." WS2812B is the standard for this kind of project — FastLED library has good support, widely available, 5V-friendly.
- **What to look for:** 60 LEDs/m density (closest match to the word spacing on a 7" grid), IP30 (no waterproofing — unneeded indoors and takes up more space), 5V operation. Individually cuttable strip (scissors-friendly segments every 3 LEDs).
- **Why 1m when we only need 30:** You'll cut some, solder some wrong, and bring-up testing wants extras. $8 for a meter vs $6 for a cut length — not worth the hassle.
- **Sourcing:** Amazon — "WS2812B 60 LEDs/m 1m 5V strip". BTF-Lighting is a reliable brand.

### DS3231 RTC breakout board — **$3-5** × 1

- **What it is:** A small module with the DS3231 real-time-clock chip, an AT24C32 EEPROM, a CR2032 battery holder, and I²C headers.
- **Why this clock needs it:** WiFi + NTP is the primary time source, but when WiFi is down (during setup, at grandma's house, after a router outage) the clock must keep ticking. The DS3231 is the industry's most accurate temperature-compensated crystal RTC — drifts only a few seconds per week. Spec calls it out explicitly as the offline fallback.
- **What to look for:** "DS3231 AT24C32" Chinese module. Comes with or without battery; buy a CR2032 separately if not included ($1 at any drugstore).
- **Sourcing:** Amazon Prime bulk packs (5 for $10 is common, cheaper per-unit — useful since you may break one).

### MAX98357A I²S amplifier breakout — **$6-10** × 1

- **What it is:** A small breakout with the MAX98357A — a 3.2W Class D audio amplifier with an I²S digital input and analog speaker output. Takes digital audio directly from the ESP32 (no separate DAC needed) and amplifies it enough to drive a small speaker.
- **Why this clock needs it:** The birthday-minute easter egg plays Dad's voice memo through the speaker. The on-demand lullaby button plays a recording. The MAX98357A is the standard small-scale solution — no external DAC, no complex wiring. Adafruit popularized it; many clones exist.
- **What to look for:** Adafruit product #3006 is the reference. Chinese clones on Amazon work fine for dev (look for ones with identical pinout). Solder the header pins yourself or get one with pre-soldered headers.
- **Sourcing:** Adafruit if you want the "known-good" version with documentation. Amazon clones are 50% cheaper and fine for breadboard testing.

### Small 8Ω speaker, 1-3W — **$3-5** × 1

- **What it is:** A mini speaker driver — 40mm diameter is a good size. Mylar cone, 8Ω impedance, handles 1-3W.
- **Why this clock needs it:** Plays the voice memo and lullaby through the MAX98357A. 3W is the max the amp can push; a 1W speaker will distort, 5W+ won't get loud enough at this power budget. 8Ω matches what the MAX98357A expects. Small enough to mount inside a 1-1.5" deep enclosure.
- **What to look for:** "40mm 8 ohm 3W mini speaker" — comes with flying leads you can crimp or solder to a 2-pin JST connector (spec specifies JST for the final PCB-to-speaker link).
- **Sourcing:** Amazon 4-pack or individually. CUI Devices or Dayton Audio for the serious version; generic clones fine for dev.

### microSD breakout (SPI) — **$3-5** × 1

- **What it is:** A small PCB with a microSD card slot and the level-shifter circuitry needed to talk to it over SPI from a 3.3V microcontroller.
- **Why this clock needs it:** Stores the MP3 files (voice memo + lullaby). The ESP32 has limited flash (~1-2 MB free for user data after firmware); audio files easily exceed that. microSD gives essentially unlimited storage and is trivial to reload by popping the card out.
- **What to look for:** SPI interface (4 wires + power). Some modules include 5V→3.3V regulation; that's fine but not required — ESP32 runs 3.3V logic and the card wants 3.3V too.
- **Sourcing:** Amazon 5-pack — they're basically commodity.

### microSD card, 4-32 GB — **$5-10** × 1

- **What it is:** A postage-stamp-sized flash storage card.
- **Why this clock needs it:** Hosts the audio files. 4 GB is vastly more than needed (a lullaby + a 10-second voice memo is <10 MB), but smaller cards are actually harder to find now than larger ones.
- **What to look for:** Class 10 or better (read speed ≥10 MB/s — overkill for audio but it's the baseline), FAT32 formatted (Windows default). SanDisk / Samsung / Kingston are all fine; avoid no-name brands (higher failure rate).
- **Sourcing:** Literally anywhere — drugstore, Amazon, Best Buy.

### USB-C breakout board — **$3-5** × 1

- **What it is:** A small board that breaks out a USB-C connector's pins to through-hole pads you can wire to.
- **Why this clock needs it:** The final design takes power over USB-C only (no separate wall adapter). For the breadboard, we just need the 5V + GND pins; data lines aren't used yet. The breakout lets us plug a standard USB-C cable into the dev rig.
- **What to look for:** Breakout for USB-C Power-Delivery is overkill — a simple "USB-C breakout 5.1k CC resistor" board is perfect and $3. The 5.1k resistors on the CC pins tell a USB-C power supply to deliver 5V at up to 3A (default profile).
- **Sourcing:** Amazon "USB-C breakout 5.1k" search. Adafruit has a fancy version for $5.

### Tactile switches (6×6mm) — **$3 for a pack of 20** × 1 pack

- **What they are:** Little pushbutton switches. Press and it closes a circuit; release and it opens.
- **Why this clock needs them:** Three buttons on the side of the final enclosure — hour-set, minute-set, audio play/stop. For bring-up, you'll want extras because you'll inevitably bend a leg or desolder a wrong one.
- **What to look for:** 6×6mm through-hole tact switches, any actuation force (50-100gf is comfortable). Bag-of-20 packs are dirt cheap.
- **Sourcing:** Amazon — "6x6x5mm tactile push button switch 20 pack".

### Solderless breadboard (830 tie-point) — **$5-8** × 1 (get 2 if you might want parallel experiments)

- **What it is:** A plastic board with a grid of holes that are internally connected in rows. Poke component leads in, poke jumper wires in, nothing needs to be soldered — you can rearrange freely.
- **Why this clock needs it:** Phase 2 is all about wiring everything together and testing, without committing to a PCB layout. Breadboard lets you move a button, add an LED to the data line, re-pin an I²S wire, all without a soldering iron.
- **What to look for:** "830 tie-point breadboard" is the standard size; fits an ESP32 dev board across the middle split. Metal backing plate is nice but optional.
- **Sourcing:** Amazon 2-pack. $6-8 total.

### Jumper wire kit (M-M, M-F, F-F mixed) — **$5-8** × 1 kit

- **What they are:** Short wires with pre-crimped pin connectors on each end. M (male) is a pin that plugs into a breadboard hole. F (female) is a socket that accepts a pin.
- **Why this clock needs them:** Connect breadboard points to dev-board pins (need M-F — female end plugs onto the dev board's header pins, male end into breadboard), breadboard-to-breadboard (M-M), and breakout-to-breakout (F-F when both have header pins).
- **What to look for:** Kit with 40 each of M-M, M-F, F-F in assorted colors. Length 20cm is a good default.
- **Sourcing:** Amazon "breadboard jumper wires kit" — one pack has everything.

### USB-C cable + 5V 2A USB charger — **$0-10**

- **What they are:** Just a charging cable and a USB power brick.
- **Why this clock needs them:** Powers the dev rig. Most phone chargers deliver 5V at 2A+ and work fine. USB-C cable must be a real one (not a charge-only USB-A to USB-C adapter) for the CC-resistor breakout to work.
- **Sourcing:** Probably already in a drawer at home. If not, any phone charger + cable.

## Total

Low estimate: ~$60. High estimate: ~$85. Ships in 1-2 days from Amazon Prime, 2-4 weeks from AliExpress (don't bother saving $10-15 on 3 weeks of waiting).

## After the order arrives

When the box shows up, say "Phase 2 plan" and I'll run `writing-plans` against the Phase 2 scope. Expected output: ~25-30 TDD tasks covering:

1. Smoke-test each IC individually with a throwaway sketch (LED-only, audio-only, RTC-only, button-only)
2. Display driver wrapping FastLED + the grid module
3. RTC + NTP time sync wrapper
4. Audio player reading MP3s from SD
5. Button debounce + handler
6. WiFi captive portal for first-boot provisioning
7. Main state machine that ties it all together

End of Phase 2: one working breadboard clock, running on Dad's desk for a week before we commit to PCB layout in Phase 3.
