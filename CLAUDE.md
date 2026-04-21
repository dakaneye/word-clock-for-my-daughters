# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

Two ESP32-driven word clocks — one for each daughter, delivered around their 7th birthdays (Emory 2030, Nora 2032). Shared hardware platform, per-kid compile-time configuration (name, birthday, wood species, lullaby, hidden acrostic). Personal keepsake with a 40-year design target.

- **Living roadmap:** `TODO.md` (current state supersedes the archived phase plan).
- **Design spec:** `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`.
- **Project conventions:** `conventions.md` — read before implementing.

## Repository shape

- `firmware/` — PlatformIO project, three envs: `emory`, `nora`, `native`.
- `hardware/` — KiCad 8.x project (schematic, PCB, gerbers, BOM).
- `enclosure/` — Laser-cut SVGs + Python generators (`scripts/`) + build123d 3D parts (`3d/`).
- `docs/hardware/` — Bench guides (pinout, smoke test, breadboard bring-up, assembly plan).
- `docs/superpowers/` — Specs and implementation plans (one pair per firmware module).

## Firmware architecture

Two-layer split enforced by the build system:

- **`firmware/lib/core/`** and the pure-logic halves of each peripheral library (`lib/<module>/src/*.cpp` listed explicitly in `[env:native].build_src_filter`) — platform-independent C++17. Must NOT include `<Arduino.h>` or any ESP32 header. These are unit-tested under Unity on the host.
- **`firmware/src/main.cpp`** and the adapter halves of each peripheral library — free to include Arduino/ESP32 headers. Adapter `.cpp` files that live under `lib/<module>/` are wrapped in `#ifdef ARDUINO` because PlatformIO's LDF pulls library sources into native builds regardless of `build_src_filter`. Every new ESP32-only `.cpp` under `lib/` needs this guard.

Per-kid config is **compile-time only**: `-D CLOCK_TARGET_EMORY=1 -include config_emory.h` (mirrored for Nora). No runtime `if (kid == ...)` branches.

### Adding a new pure-logic module

1. Create `firmware/lib/<module>/include/...` + `firmware/lib/<module>/src/<file>.cpp`.
2. Add each `.cpp` to `[env:native].build_src_filter` in `platformio.ini` (it's an explicit allow-list, not a glob).
3. Add `-I lib/<module>/include` to `[env:native].build_flags`.
4. Create `firmware/test/test_<module>_<name>/` with Unity tests.

### Pinned versions (do not bump casually)

`platform = espressif32@6.8.0` is pinned without a caret intentionally — Arduino-ESP32 3.x removes the legacy `driver/i2s.h` API the audio adapter depends on. The `build_unflags = -std=gnu++11` line is also load-bearing; Arduino-ESP32 8.x appends `-std=gnu++11` after ours and silently downgrades ESP32 builds to C++11 without it.

### Captive portal HTML

`firmware/assets/captive-portal/form.html.template` + `form.css` + `gen_form_html.py` produce `form_html.h` via the PlatformIO pre-build hook (`scripts/pio_generate_captive_html.py`). Generated file is gitignored. For local browser preview:

```bash
cd firmware/assets/captive-portal && python gen_form_html.py --kid emory --preview
```

## Common commands

All `pio` commands run from `firmware/`. PlatformIO is typically installed at `~/Library/Python/3.9/bin/pio` on this machine.

```bash
# Build per-kid ESP32 firmware
pio run -e emory
pio run -e nora

# Native (host) Unity tests — the CI gate
pio test -e native
pio test -e native --verbose
pio test -e native --filter test_display_renderer   # single test dir

# Flash hardware
pio run -e emory -t upload
pio device monitor -b 115200

# Enclosure SVG regeneration + tests
cd enclosure/scripts && pip install -r requirements.txt && python build.py
cd enclosure/scripts && pytest

# Captive portal asset generator tests
cd firmware/assets/captive-portal && pytest
```

CI runs `pio run -e {emory,nora}` + `pio test -e native --verbose` on every PR (`.github/workflows/ci.yml`). Hardware sketches in `firmware/test/hardware_checks/` are **manual checklists**, not CI-run.

## Testing rules

- Every new pure-logic module gets a new test directory under `firmware/test/`. No exceptions.
- Hardware-touching behavior (I²C transactions, I²S DMA, WS2812B timing) is verified via the manual checklists under `firmware/test/hardware_checks/<module>_checks.md`, not mocked.

## Hardware / enclosure context

- PCB part choices are constrained by JLCPCB PCBA (LCSC basic parts preferred). BOM in `hardware/word-clock.csv`.
- The face SVGs' letter grid must match `firmware/lib/core/src/grid.cpp` — grid is the source of truth.
- LED positions in the PCB must match the face grid. When moving letters, update both.
- Locked decisions this cycle: frame is a bare shell, buttons press through the back panel, captive cable exits through a grommeted hole, back panel removable via M3 brass spacers for 40-year CR2032 replacement cadence.

## Git

- Branch prefix: `dakaneye/` (never `samdacanay/`).
- Conventional commits; no Claude attribution in messages.
- Selective staging — no bulk `git add -A`.
