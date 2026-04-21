# Archive

Docs that describe reasoning for decisions already made or work already
completed. Kept for historical context (why did we pick X?) but not part
of the live working set.

Live docs live at the normal paths:

- `docs/superpowers/specs/` — active design contracts for shipped modules
- `docs/superpowers/plans/` — live master plan + in-flight module plans
- `docs/hardware/` — forward-looking reference (pinout, bring-up, assembly,
  PCB rework) and research notes consulted during ongoing work
- `TODO.md` — live roadmap (supersedes the archived activity-blocking-graph)

## What's in here

### `specs/`

| File | Archived because |
|---|---|
| `2026-04-14-3d-printer-purchase-decision.md` | Decision executed — Bambu A1 ordered and arriving. Retained for the fallback plan (SendCutSend / Craftcloud) if the printer ever fails. |
| `2026-04-15-activity-blocking-graph.md` | Parallel-work sequencing question answered. `TODO.md` is the live roadmap now. |

### `plans/`

| File | Archived because |
|---|---|
| `2026-04-14-phase-1-firmware-logic.md` | All TDD tasks executed; tagged `phase-1-complete`. |
| `2026-04-15-laser-cut-face-implementation.md` | SVGs generated and ordered from Ponoko. |
| `2026-04-16-buttons-implementation.md` | Executed 2026-04-16 / 2026-04-17 session. `firmware/lib/buttons/` shipped; hardware verification in `firmware/test/hardware_checks/buttons_checks.md`. |
| `2026-04-16-captive-portal-implementation.md` | Executed 2026-04-16. `firmware/lib/wifi_provision/` shipped; hardware verification in `firmware/test/hardware_checks/wifi_provision_checks.md`. |

### `hardware/`

| File | Archived because |
|---|---|
| `2026-04-15-kicad-rework.md` | LED chain re-wiring + AT reposition + routing all executed. Commit `405864a` landed the PCB source; gerbers + drill + CPL + BOM exported; DRC clean. Still useful as the historical reference for the step 2.11 gerber-export procedure and the documented word-center LED coordinates. |
| `2026-04-17-usb-c-breakout-removal.md` | Cermant USB-C breakout removed from schematic + PCB + BOM. Guide also describes the now-abandoned panel-mount pigtail approach; final design uses a captive Micro-USB-to-USB-C cable through a grommet. |

## Why keep these at all?

Three reasons:

1. **Provenance.** Someone looking at `firmware/lib/wifi_provision/` in 2030
   can trace the design decisions back to the spec and the TDD execution
   record.
2. **Pattern reference.** Future Phase 2 modules (`display`, `rtc`, `ntp`,
   `audio`) will follow the same spec → plan → execute shape; the archived
   plans are reference examples.
3. **Fallback branches.** The 3D-printer decision document has a
   fallback-to-SendCutSend branch that becomes live again if the printer
   ever fails permanently.

Nothing in here should be modified; update the replacement live docs or
create a new one.
