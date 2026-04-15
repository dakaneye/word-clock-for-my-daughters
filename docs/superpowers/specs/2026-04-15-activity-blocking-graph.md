# Activity Blocking Graph & Sequencing Decision

Date: 2026-04-15
Status: Decision locked — follow this as the working roadmap until re-evaluated

## Purpose

Phase 0 and Phase 1 are complete. The original plan sequenced Phase 2 (breadboard hardware bring-up) strictly after parts arrive, with everything else blocked until then. This document unblocks that thinking: **most Phase 3/4 work can start now in parallel, because only a subset of activities genuinely depends on physical parts in hand.**

## Current state (2026-04-15)

- Phase 0 (repo + CI): ✅ done
- Phase 1 (native firmware logic): ✅ done; 35 tests green; tag `phase-1-complete` pushed
- Parts order placed on Amazon (~$125); arriving **Fri 2026-04-17 to Mon 2026-04-20**
- Bambu Lab A1 3D printer ordered, same arrival window
- DS3231 already owned (spare from Chelsea's clock restock)
- Dual-bus breadboard + M-M jumpers + resistor assortment salvaged from discard pile
- Laser-cut vendor confirmed: **Ponoko** (used for Chelsea's clock in 2015/2026)

## Activity inventory

Every activity from here through delivery, classified by what actually blocks it:

| # | Activity | Blocked on |
|---|---|---|
| A | Pinout spreadsheet (ESP32 GPIO assignments, bus allocation) | nothing |
| B | KiCad schematic v0 (all IC blocks from datasheets + refs) | A |
| C | KiCad footprint/symbol verification against actual parts | parts in hand (partially) |
| D | KiCad PCB layout v0 | B, C |
| E | Laser-cut face SVG — Emory (birch, EMORY row + fillers) | nothing (grid locked in spec) |
| F | Laser-cut face SVG — Nora (walnut, NORA row + fillers) | nothing |
| G | Laser-cut side-frame SVGs | nothing |
| H | Laser-cut back-panel SVGs (USB-C cutout + dedication) | D (PCB outline needed) |
| I | 3D light-channel honeycomb CAD | D (LED pad positions) |
| J | 3D PCB standoffs CAD | D (mounting-hole positions) |
| K | 3D speaker mount CAD | D (dimensions + connector location) |
| L | 3D button housing CAD | D (button positions) |
| M | Captive portal HTML/CSS (first-boot WiFi setup) | nothing |
| N | Audio recording — Dad's voice memo at birth minute | nothing |
| O | Audio recording — lullaby (one per kid) | nothing |
| P | Wood species sourcing research | nothing |
| Q | Diffuser material test (paper vs frosted PETG) | parts (LEDs) |
| R | Audio-only breadboard bring-up | parts |
| S | PCB v1 fab at JLCPCB | D (+ R as validation) |
| T | 3D print test run | I–L complete + printer calibrated |
| U | Cardboard laser-cut test (prove face readability before real wood) | E–G complete |
| V | Emory unit assembly | S, T, U complete |
| W | 30-day burn-in test | V |
| X | Nora unit assembly + final audio recordings | W |

## Blocking graph

```
Phase 1 ✅
    │
    ▼
    A ──┐
        │
        ├──► B ──► D ──┬──► H ──► U ───────────────┐
    C ──┘              ├──► I ──┐                  │
                       ├──► J ──┤                  │
                       ├──► K ──┤                  │
                       └──► L ──┴──► T ────────────┤
                                                   │
  (independent tracks, start now)                  ▼
    E ──► U ─────────────────────────────────────► V ──► W ──► X
    F ───────────────────────────────────────────►
    G ──► U
    M ──── (integrates in Phase 2 firmware) ─────►
    N ──── (loads onto SD in Phase 5) ───────────►
    O ──── (loads onto SD in Phase 5) ───────────►
    P ──── (informs Q + U + V) ──────────────────►

  (parts-blocked)
    Parts arrive ──► R ──► (informs S) ──────────►
                     Q ───────────────────────────►
```

## Five parallel tracks

**Track 1 — KiCad (highest leverage; unblocks 5 downstream activities)**
- A: Pinout spreadsheet
- B: Schematic v0 (crib from Adafruit ESP32 Audio BFF for audio subsystem)
- C: Footprint verification (partial now, finalize when parts arrive)
- D: PCB layout v0
- **Output:** fab-ready gerbers + BOM, ready to submit to JLCPCB

**Track 2 — Laser-cut clock face (independent; visible progress fast)**
- E: Emory face SVG
- F: Nora face SVG
- G: Side-frame SVGs
- P: Wood sourcing (confirm Ponoko stocks birch + walnut at needed thickness)
- U: Cardboard test cut (~$10 from Ponoko or SendCutSend) to prove readability before committing to real wood

**Track 3 — Audio recording (emotional payoff; can be re-done later)**
- N: Voice memo recording (6:10 PM Emory / 9:17 AM Nora)
- O: Lullaby recording per kid
- **Note:** these are re-recordable any time up to delivery (2030/2032). v1 now, final v2 closer to ship.

**Track 4 — Captive portal UI (one-evening self-contained task)**
- M: HTML form for WiFi provisioning; test in a browser with mock data

**Track 5 — Diffuser + enclosure polish (blocked on parts + PCB)**
- Q: Diffuser material test (paper vs frosted PETG) once LEDs arrive
- H, I, J, K, L: 3D CAD and back-panel SVG, once PCB layout D is done

## Decision: sequencing

**Weeks 1-2 (2026-04-15 through parts/printer arrival)**

Interim work before parts arrive (~2-5 days away):
- Start Track 1 with A (pinout spreadsheet), then B (schematic v0)
- Start Track 2 with E + F (clock face SVGs) in parallel
- Optional: Track 4 (captive portal) on a low-brain evening
- Optional: Track 3 (audio recording) if inspired

**Week 3 (parts + printer in hand)**
- Audio-only breadboard bring-up (R) — highest-risk subsystem first, 1 evening
- Finish Track 1 B/D using any learnings from R
- Ponoko order for cardboard face test (U)
- 3D printer setup + calibration prints
- Track 5 starts: D informs H/I/J/K/L

**Weeks 4-6**
- Submit PCB v1 to JLCPCB (S) — 2-week fab + ship
- 3D-print internals (T) in parallel
- Ponoko order for real wood faces + sides (after cardboard test passes)
- PCB arrives → assemble Emory unit (V)

**Weeks 7-10**
- 30-day burn-in test (W) on Emory unit on the user's desk
- Any PCB v2 spin if bugs found

**Weeks 11-12**
- Nora unit assembly (X)
- Final audio recordings go on SD cards
- Both clocks boxed for storage until delivery (2030 / 2032)

**Total realistic timeline:** ~12 weeks to both units assembled and validated. Original plan estimated "2027-2029" prototype iteration; the more aggressive schedule is feasible because most activities parallelize.

## Why this changed

The original Phase 2→3→4→5 ordering was conservative — wait-and-see at each phase boundary. Re-examining the actual dependency graph: (a) KiCad work needs only datasheets, not physical parts; (b) clock-face SVGs need only the locked letter-grid spec; (c) audio + captive portal are standalone. Only the *audio bring-up* genuinely needs parts in hand as a de-risking step before PCB commit.

## Reminders

- Parts are **already ordered** — don't re-place the wishlist.
- 3D printer is **already ordered** — don't re-evaluate the printer decision (spec at `docs/superpowers/specs/2026-04-14-3d-printer-purchase-decision.md`).
- Ponoko is the laser-cut vendor — no need to re-evaluate SendCutSend etc. unless Ponoko's birch or walnut stock has changed.
- The 3D-print fallback (outsource via SendCutSend) in the printer spec remains a valid backup if the Bambu A1 has issues.
