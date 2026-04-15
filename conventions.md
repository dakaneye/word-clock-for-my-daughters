# Project Conventions

Personal multi-year project building two ESP32 word clocks. Read this before implementing anything.

## Layout

- `firmware/` — PlatformIO project (single project, two envs: `emory`, `nora`)
  - `src/` — ESP32-only modules (needs Arduino core)
  - `lib/core/` — platform-independent modules, testable on host
  - `test/` — native Unity tests for `lib/core/`
  - `configs/config_emory.h`, `configs/config_nora.h` — per-kid compile-time config
- `hardware/` — KiCad project (schematic, PCB, BOM, production files)
- `enclosure/` — 3D models (STEP / STL / SCAD) + laser-cut vector files (SVG)
- `docs/` — specs, plans, design notes, build photos
- `.github/workflows/` — CI

## Firmware conventions

- Arduino-ESP32 core via PlatformIO. No bare ESP-IDF. No Arduino IDE `.ino`.
- Per-kid config is compile-time (`-D CLOCK_TARGET=emory`), never runtime.
- Anything in `lib/core/` must compile and test on the host (no `#include <Arduino.h>`).
- Hardware-touching code lives in `src/` and may freely include Arduino headers.
- Use `FastLED` for WS2812B, `RTClib` for DS3231, `NTPClient` for time sync.
- Config headers define `constexpr` values only — no runtime mutation.

## Testing

- Native Unity tests run in CI on every PR (`pio test -e native`).
- Hardware tests are manual sketches in `firmware/test-sketches/` — not run in CI.
- New pure-logic module → new test file. No exceptions.

## Hardware conventions

- KiCad 8.x project, one schematic sheet per functional block (MCU, LED, audio, power, connectors).
- JLCPCB PCBA constraints drive part choice (LCSC basic-parts preferred).
- BOM tracked in `hardware/BOM.csv`, generated from KiCad.

## Git

- Branch prefix: `dakaneye/`
- Commits: conventional format (`feat:`, `fix:`, `chore:`, `docs:`...)
- Release-pilot handles version bumps and tags

## Reference

Design spec: `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`
Implementation plan: `docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md`
Chelsea's clock (inspiration, not shared code): https://github.com/dakaneye/word-clock
