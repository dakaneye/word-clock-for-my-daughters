# As-ordered fabrication package — 63-LED board

Exact JLCPCB submission for the word-clock main board. This directory is the
immutable as-fabricated record for the 40-year build.

- **Ordered:** 2026-06-07 — JLCPCB, 5 boards, full PCBA (top-side, Economic).
- **Contents:** `word-clock-gerbers.zip` (gerbers + drill), `word-clock-cpl.csv`
  (component placement), `word-clock-bom.csv` (bill of materials). The unzipped
  `gerbers/` directory is gitignored — the `.zip` is the canonical artifact.
- **LED substitution (as-built ≠ as-listed):** `word-clock-bom.csv` lists the
  LED as **WS2812B-B/W (LCSC C114586)**, but the board was ordered with
  **WS2812B-V6 (LCSC C52917433)** — the only in-stock option at order time. The
  swap is a benign revision (standard 5050-4P pinout, 800 kHz GRB); the V6's
  longer (>280 µs) reset is irrelevant at word-clock update rates. Full
  rationale: `docs/hardware/pre-fab-review.md`.
- **LED count:** 63 (designators D1–D64, with D7 unused).

Re-exports from KiCad go to `hardware/regen/` (gitignored); do not regenerate
this package in place.
