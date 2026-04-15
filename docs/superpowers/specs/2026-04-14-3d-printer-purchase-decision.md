# 3D Printer Purchase Decision

Date: 2026-04-14
Status: Recommendation locked, awaiting purchase

## Summary

Buy a **Bambu Lab A1** ($350). Defer AMS Lite until a multi-color project forces the upgrade. Budget ~$485 total including calipers, starter filament, and basic consumables.

This decision serves the word-clock-for-my-daughters project (light-channel honeycomb, PCB standoffs, speaker mount, button housings — all small internal white PLA parts) as the initial use case, but the printer is chosen for a broader household/hobby use profile that extends well past the clocks.

## Use Case Profile

Ranked by expected frequency, based on brainstorm 2026-04-14:

1. **Household repair** — e.g., worn stove knobs where numbers no longer read
2. **Home/garage organization** — shelf brackets, pegboard systems, shed organization, drawer dividers
3. **Kid stuff for Emory & Nora** — name signs, toys, school projects, costumes
4. **Gifts** — personalized one-offs for family
5. **Desk/workspace** — cable management, holders, mods
6. **CAD as a new skill** — treating this as a gateway into parametric design
7. **Electronics projects** (light) — word-clock internals, future ESP32/Raspberry Pi enclosures

Deprioritized: outdoors/car (low interest).

## Claude Integration Profile

- **80% design workflow** — Claude generates parametric CAD from descriptions; I refine, slice, print. Printer-agnostic.
- **20% printer operation** — queue management, monitoring, failure detection. Nice-to-have, not required. Bambu's semi-closed ecosystem is acceptable because this tier is non-critical.

## Options Considered

| Option | Price | Verdict |
|---|---|---|
| Bambu Lab A1 Mini | $200 | Rejected. 180mm bed forces segmentation on garage/kid prints too often. |
| **Bambu Lab A1** | **$350** | **Chosen.** 256mm bed, fast, rock-solid reliability, open-frame fine for PLA/PETG. |
| Bambu Lab A1 + AMS Lite | $540 | Rejected (deferred). Multi-color fails more; master single-color first. Upgrade later. |
| Bambu Lab P1S | $700 | Rejected. Enclosed chamber / ABS capability buys nothing in the use profile. |
| Prusa MK4S | $800-1100 | Rejected. Better for "Claude runs the workshop" vision; overkill for 80/20 design-first profile. Higher setup friction conflicts with "reactive but iterative" usage pattern. |

## Rationale

1. **Build volume matches use cases.** 256mm handles ~90% of household/organization prints in one piece. The remainder segments cleanly with alignment pins or dovetails.
2. **No enclosure needed.** Use cases don't require ABS/ASA. PETG covers functional and moderately-outdoor needs. Enclosure spend doesn't pay back.
3. **Ease-of-use is a requirement, not a preference.** "Reactive but iterative" user profile means friction kills usage. Bambu A1's setup-to-first-print is ~30 min vs. Prusa kit's ~4 hours.
4. **Claude integration lives primarily in CAD**, which is printer-agnostic. Printer choice doesn't meaningfully constrain the 80% case.
5. **The 20% operation tier** is a side-project opportunity via community Bambu-MCP efforts. Fragile but low-stakes.
6. **AMS Lite is purely additive.** Deferring saves $180 now with zero upgrade regret later.

## Initial Purchase List

| Item | Est. Cost |
|---|---|
| Bambu Lab A1 | $350 |
| Digital calipers (sub-$40 range is fine) | $30 |
| Starter filament: PLA Basic, 4 colors (white, black, + 2) | $80 |
| Consumables: glue stick, scraper kit, spare 0.4mm nozzle | $25 |
| **Total** | **~$485** |

Deferred purchases:
- **AMS Lite** ($180) — trigger: first project that genuinely needs multi-color (likely a kid name sign or stove knob with color-inlay numbers)
- **PETG filament** — trigger: first functional print that needs heat/UV/outdoor tolerance
- **Enclosure kit** (3rd-party, ~$100) — trigger: only if I ever want to try ABS (unlikely given use profile)

## CAD Stack

Chosen for a software-engineering mindset + Claude-forward workflow:

- **OpenSCAD** — first tool for simple parametric parts (stove knob, spacers, brackets). Text-based, functional, trivial for Claude to read and write. Perfect for rotationally symmetric or obviously-parametric geometry.
- **CadQuery** — Python-based parametric CAD. For anything too complex for OpenSCAD but still model-as-code. Git-diffable, testable, LLM-friendly. This is the primary long-term target.
- **OnShape** (hobbyist tier) — browser-based constraint-based CAD with a real API. For complex mechanical assemblies where sketch-and-constraint thinking beats code. Public-models-only on the free tier.

Skip Fusion 360 — Autodesk license trajectory is hostile, and CadQuery covers the same ground for this use profile.

## First 90 Days: Project Sequence

Ordered to build skills incrementally:

1. **Benchy + factory calibration print** — day 1, comes with the printer
2. **Stove knob replacement** — canonical first real project. Measure spindle with calipers, model in OpenSCAD, iterate to fit. Paint-fill numbers for a two-tone look without AMS.
3. **Small desk organizer** — pen holder or cable clip. Fast feedback loop to build slicing intuition.
4. **Drawer divider or kitchen organizer** — first print that benefits from thinking about assembly / segmentation.
5. **Pegboard hook or shelf bracket for the garage** — first PETG print for strength/durability.
6. **Name sign for Emory or Nora** — first aesthetic piece. Single-color with paint fill OR trigger for AMS Lite purchase.
7. **Word-clock internals** (when the clock project reaches that phase) — light-channel honeycomb, PCB standoffs, speaker mount, button housings.

## Claude Integration Roadmap

Ordered by leverage-per-hour:

- **Phase 1 (day 1): OpenSCAD via Claude.** No integration needed — Claude writes OpenSCAD directly in-chat. Highest-leverage, zero-setup. This alone is most of the 80%.
- **Phase 2 (~month 1): CadQuery MCP.** Install an existing community MCP or write a thin one. Unlocks programmatic CAD with full Python power.
- **Phase 3 (~month 3): OnShape API MCP.** For complex assemblies. Useful specifically when mechanical constraints matter.
- **Phase 4 (~month 6, optional): Bambu local API MCP.** The 20% operation tier. Community Bambu-MCP projects exist; adopt or contribute. Only worth doing if a real workflow demands it — do not build speculatively.

## Open Questions

- Filament storage — dry box vs. ambient. Revisit after 30 days based on humidity observations.
- Slicer choice — Bambu Studio (default, proprietary) vs. OrcaSlicer (open-source fork, Bambu-compatible). Start with Bambu Studio; switch to OrcaSlicer if I hit a wall.

## Non-Goals

- Print farm operation.
- Production/commercial printing.
- Exotic materials (carbon-fiber nylon, TPU, PC).
- Large-format prints (>256mm) that can't be segmented.
