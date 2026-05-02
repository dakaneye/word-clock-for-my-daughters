# Daughters' Word Clocks — Design Spec

Date: 2026-04-14
Status: Design locked, ready for implementation planning

## Overview

Two word clocks, one for each daughter, delivered around age 7. Same hardware platform, same firmware, per-clock configuration for name, birthday, acrostic, wood species, and lullaby. Intended to last 40+ years and feel handmade like Chelsea's — same DNA, newer generation.

- **Emory** — born 2023-10-06 — target delivery 2030
- **Nora** — born 2025-03-19 — target delivery 2032

## Scope

One design spec covers both clocks. Differences are configuration values, not design variants.

| Attribute | Emory | Nora |
|---|---|---|
| Face material | birch plywood | walnut |
| Name in grid (row 9) | EMORY | NORA |
| Birthday date | Oct 6 | Mar 19 |
| Hidden acrostic | `EMORY 10 6 2023` (12 chars / fillers) | `NORA MAR 19 2025` (13 chars / fillers) |
| Voice memo at birth minute | 6:10 PM, Dad's voice | 9:17 AM, Dad's voice |
| Lullaby (on-demand button) | one song recorded by Dad | one song recorded by Dad |
| Signature everyday color | warm white | warm white |

## Hardware Platform

- **MCU:** ESP32 (Arduino-core compatible). Picked for WiFi, I2S audio, mature libraries.
- **Time source:** NTP over WiFi; DS3231 RTC as offline fallback.
- **WiFi provisioning:** captive portal on first boot (phone connects to temporary AP, enters SSID + password in browser form). USB-C available as debug + firmware update path.
- **LEDs:** WS2812B RGB, one per word group (~25 per clock).
- **Audio:** MAX98357A I2S DAC + amp, small speaker (~2-3W), microSD card for 16-bit PCM WAV files (44.1 kHz mono). Format was flipped from MP3 during audio-module spec (`docs/superpowers/specs/2026-04-18-audio-design.md`) — eliminates the MP3 decoder dependency.
- **Power:** USB-C only. Any standard 5V/2A source drives the full system. No wall adapter specific to the clock.
- **Buttons:** three tact switches on the right side — hour set, minute set, audio play/stop.
- **No power button.** Always on when plugged in.

## PCB

Single board, ~150mm × 150mm, matching face dimensions. JLCPCB PCBA service assembles both sides.

- **Top side:** WS2812B LEDs placed under each word group on the face.
- **Bottom side:** ESP32 module, I2S amp, microSD slot, USB-C, buttons, DS3231, passives.
- **Connector to speaker:** 2-pin JST.

## Enclosure

Shadow-box construction. All visible exterior is wood. Plastic is hidden inside.

- **Face:** laser-cut wood (cut-through letters, not etched — the diffuser paper backing glows through the openings). Birch for Emory, walnut for Nora.
- **Side frames (4 pieces):** matching wood species, glued to form the box depth.
- **Back panel:** matching wood, engraved with dedication ("For Emory / love Dad" etc.). Carries 3 button access holes with "Hour / Min / Audio" labels next to each, a speaker vent grid, and one cable exit hole (~16 mm — drilled out from the as-cut 6 mm with a 5/8" step bit at assembly so the USB connector overmold can pass through). A permanently-installed Micro-USB-to-USB-C cable connects the ESP32 module's micro-USB port to the outside world; strain relief is provided by an internal cable P-clip screwed to the back-panel interior 2-3 cm in from the hole (the enlarged hole is too loose for a grommet to grip the cable). **Must be fully removable without damaging the wood, repeatably over 40+ years** — the DS3231 CR2032 coin cell needs replacement every 5-10 years. Attachment: 4 × M3 × 1/2"–5/8" wood screws threaded directly into the 6.4 mm hardwood frame wall at the existing 4 mm-inset corner positions, into pre-drilled ~2 mm pilot holes. The 4 mm panel inset puts the screw centers inside the wall thickness so screws bite into wood. (Original plan — M3 hex spacers epoxied into interior corners + machine screws — abandoned 2026-04-25 after the 4 mm-inset panel geometry was found to make spacer-in-corner placement impossible; the bug became a feature once we switched to wood-screws-into-wall.) Speaker mounts to 2 × M3 hex spacers epoxied to the back-panel interior 37 mm apart behind the vent — no 3D-printed cradle, the speaker has integral M3-compatible mounting flanges. The DS3231 and HW-125 daughterboards install with their battery holder / card slot facing the back panel (right-angle 1×6 sockets on B.Cu enforce this geometrically) so the CR2032 and SD card remain accessible after removing the panel.
- **Internal:** 3D-printed in white PLA — light channel honeycomb (isolates each LED to its word), PCB standoffs, speaker mount, button housings. Never seen.
- **Diffuser:** Selens self-adhesive diffusion film (0.16 mm) bonded to the back of the face. Single-layer stack — the original two-layer spec (film + 3.2 mm opal acrylic) was reduced to film-only on 2026-04-26 after bench testing showed the film alone provides adequate hot-spot suppression at the planned LED-to-film distance, and the opal acrylic introduced ~5 mm of lateral light bleed (its bead-scattered transmission spreads light laterally inside the 3.2 mm sheet). The two acrylic sheets ($37) are shelved as backup if hot spots emerge during full-clock first-light testing — the recovery design would be per-pocket acrylic cuts + light-channel walls extending through the acrylic level.

Dimensions: **7" × 7" face**, depth TBD during build (probably 1-1.5" to fit PCB + speaker + light channels).

## Letter Grid

13 columns × 13 rows. Dense QLOCKTWO style, same density as Chelsea's clock. 12 filler letters spell a hidden acrostic (name + birthdate).

### Emory's grid (reference — lit example: IT IS TWENTY PAST THREE IN THE AFTERNOON)

```
I T E I S M T E N H A L F       ← IT, IS, TEN, HALF · fillers: E, M
Q U A R T E R T W E N T Y       ← QUARTER, TWENTY
F I V E O M I N U T E S R       ← FIVE-min, MINUTES · fillers: O, R
Y H A P P Y P A S T T O 1       ← HAPPY, PAST, TO · fillers: Y, 1
O N E B I R T H T H R E E       ← ONE, BIRTH, THREE
E L E V E N F O U R D A Y       ← ELEVEN, FOUR, DAY
T W O E I G H T S E V E N       ← TWO, EIGHT, SEVEN
N I N E S I X T W E L V E       ← NINE, SIX, TWELVE
E M O R Y 0 A T F I V E 6       ← EMORY, AT, FIVE-hr · fillers: 0, 6
T E N O C L O C K N O O N       ← TEN-hr, OCLOCK, NOON
2 I N T H E M O R N I N G       ← IN, THE, MORNING · filler: 2
A F T E R N O O N 0 A T 2       ← AFTERNOON, AT · fillers: 0, 2
E V E N I N G 3 N I G H T       ← EVENING, NIGHT · filler: 3
```

Filler letters read in order: `E M O R Y 1 0 6 2 0 2 3` = **EMORY 10/6/2023**.

### Nora's grid

Identical to Emory's except row 9. NORA is 4 letters (vs EMORY's 5), so row 9 has one extra filler cell. Nora's grid therefore has 13 filler cells total (vs Emory's 12), and the acrostic is one char longer.

```
N O R A R 1 A T F I V E 9       ← NORA, AT, FIVE-hr · fillers: R, 1, 9
```

Filler letters in reading order across the full grid: `N O R A M A R 1 9 2 0 2 5` = **NORA MAR 19 2025** (13 chars).

### Word inventory (24 clock words + 4 birthday + 12 fillers)

Time-tellers: IT, IS, FIVE-min, TEN-min, QUARTER, TWENTY, HALF, MINUTES, PAST, TO, ONE, TWO, THREE, FOUR, FIVE-hr, SIX, SEVEN, EIGHT, NINE, TEN-hr, ELEVEN, TWELVE, OCLOCK, NOON, IN, THE, AT, MORNING, AFTERNOON, EVENING, NIGHT.

Birthday message: HAPPY, BIRTH, DAY, NAME. Woven across rows 4, 5, 6, 9.

Time phrasing uses "IT IS [minutes] PAST/TO [hour] IN THE MORNING/AFTERNOON/EVENING / AT NIGHT". Special case: "IT IS TWELVE NOON" at 12:00 PM; midnight uses "IT IS TWELVE AT NIGHT".

## Firmware Behaviors

### Clock face

- **Default everyday color:** warm white. Same for both clocks.
- **Update cadence:** words refresh every 5 minutes (same as Chelsea's). Rounding is floor, matches QLOCKTWO convention.

### Bedtime dim

- **Trigger:** local time between 7:00 PM and 8:00 AM.
- **Behavior:** LEDs drop to ~10% brightness. Clock stays on, still shows time. No words blank out.

### Birthday mode

- **Trigger:** on the kid's birthday (Oct 6 for Emory, Mar 19 for Nora), all day.
- **Behavior:** HAPPY + BIRTH + DAY + NAME light up in rainbow cycle alongside the time words.
- **Birth-minute easter egg:** at the exact birth time on birthday (6:10 PM for Emory, 9:17 AM for Nora), plays Dad's voice memo through the speaker once.

### Holiday modes

Single-day color shifts. Clock displays time normally, but word colors use the holiday palette instead of warm white.

| Holiday | Date | Palette |
|---|---|---|
| MLK Day | 3rd Monday of January | purple |
| Valentine's Day | Feb 14 | red/pink |
| International Women's Day | Mar 8 | purple |
| Earth Day | Apr 22 | green/blue |
| Easter | computed (first Sunday after first full moon after Mar 21) | pastels |
| Juneteenth | Jun 19 | red/black/green (Pan-African) |
| Indigenous Peoples' Day | 2nd Monday of October | earth tones |
| Halloween | Oct 31 | orange/purple |
| Christmas | Dec 25 | red/green |

### Audio button

- **Button press:** plays the lullaby (pre-recorded by Dad) once, then stops.
- **Press during playback:** stops immediately.
- **Volume:** fixed in firmware, tuned by Dad during assembly. Not user-adjustable.

### Time sync

- **On WiFi:** NTP sync on boot and every 24 hours (±30 min jitter). Updates RTC.
- **No WiFi:** free-runs on DS3231. Drift is low (seconds per week) — acceptable.

## Per-Clock Configuration

Single source file (e.g., `config.h`) per clock selects the identity at compile time:

- Name string → drives row 9 letter layout
- Birthday month/day → drives birthday-mode trigger
- Birth time (hour:minute) → drives voice-memo trigger
- Acrostic string → drives filler-cell contents
- Wood species label → for the dedication engraving, no firmware impact

Same PCB, same firmware source, different compile-time flag.

## Delivery Timeline

- **Emory** — delivery window: around her 7th birthday, Oct 2030. Working backwards, prototype and iterate 2027-2029, lock design 2030.
- **Nora** — delivery window: around her 7th birthday, Mar 2032. Emory's clock is the prototype; Nora's is the second unit with any fixes baked in.

## Budget Estimate

Per clock:
- PCBA from JLCPCB (5 boards, since MOQ) — amortized ~$50 per clock
- Laser-cut wood face + frames — ~$40
- ESP32 module, speaker, microSD, buttons, USB-C — ~$30
- 3D-printed internals — ~$10 material
- Misc (paper diffuser, wire, glue, finish) — ~$20

**~$150-180 per clock**, not counting the 3D printer itself if you buy one.

## Open Items for Planning Phase

Deferred to `writing-plans` — not blocking the spec:

- KiCad schematic + layout details (pin budget, specific footprints, DRC)
- Firmware module breakdown (how time/color/animation/audio modules compose)
- Light-channel 3D model dimensions (wall thickness, depth, alignment to PCB LED pads)
- Laser-cut file format (SVG? DXF?) and vendor choice (SendCutSend vs Ponoko vs local)
- Audio file format/encoding, bitrate, SD card filesystem layout
- Captive portal UI design and WiFi credentials storage
- Testing strategy (unit tests for grid logic, hardware test sketches for audio/LEDs)
