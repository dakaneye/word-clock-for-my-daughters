# Daughters' Word Clocks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build two word clocks (Emory 2030, Nora 2032) on a shared ESP32 + WS2812B platform with per-kid compile-time configuration, matching the locked design spec at `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`.

**Architecture:** Single PlatformIO project with two build environments (`emory`, `nora`) that differ only by `-D CLOCK_TARGET=` flags pointing at `config_emory.h` / `config_nora.h`. Firmware is split into testable modules (time→words grid logic, color/animation, audio, WiFi/NTP, buttons, config). Native-host tests run the pure-logic modules (`time_to_words`, holiday date math, acrostic filler rendering) in CI without hardware. Hardware-dependent modules compile under the ESP32 env only. KiCad design files and 3D-printable internals live alongside firmware in the same repo. A single enclosure CAD and PCB footprint serve both units; wood species, dedication engraving, and per-kid config are the only diffs.

**Tech Stack:** PlatformIO + Arduino-ESP32 core, FastLED (WS2812B), ESP32 I2S + MAX98357A driver, NTPClient, RTClib (DS3231), Preferences (WiFi creds), KiCad 8.x, FreeCAD (or OpenSCAD) for 3D internals, Fusion 360 or Shaper Origin–friendly SVG for laser-cut wood faces. CI: GitHub Actions with `arduino/compile-sketches`-style PlatformIO build, Unity native tests, hookshot composite actions (`claude-review-code`, `gas-fixer`), release-pilot for tagging.

**Phase ordering rationale:** Repo+CI infra first (unblocks every future PR with linting, review, and vulnerability gates). Then native-testable firmware logic (grid → words, holidays, acrostic) — no hardware required, catches the highest-value bugs early. Then hardware bring-up on a breadboard to de-risk every IC *before* committing to a PCB. Then KiCad v1 using breadboard learnings. Then enclosure CAD against real PCB dimensions. Then integration + Emory unit 1 (prototype). Then Nora unit 2 with fixes baked in.

**Scope note:** Phases 0 and 1 are fully detailed below as executable TDD plans. Phases 2–6 are outlined with entry criteria, deliverables, and flagged open questions; each should get its own dedicated sub-plan (rerun `superpowers:writing-plans` against that phase's scope) when its entry criteria are met. Trying to fully plan a PCB in 2026 that won't be fabricated until ~2028 is a waste — the spec's "Open Items for Planning Phase" correctly defers those decisions.

## Amendments

- **2026-04-14 (execution day):** Dropped Tasks 0.5 (CodeQL), 0.6 (hookshot claude-review-code), 0.7 (hookshot gas-fixer), and 0.9 (trivy+gitleaks scan). Equivalent coverage is provided centrally by `dakaneye/.github` sync (settings.yml baseline ruleset + template workflows for CodeQL, security scan, and dependabot auto-merge). What used to be Task 0.10 (repo wrap-up + branch protection) is now Task 0.9 and reshaped into "trigger/verify org sync." Original task text below is left intact for historical reference but superseded — follow the current task list, not those sections.
- **2026-04-14:** Added a 3D-printing-fallback branch to Phase 4 (enclosure). If in-house printing (Bambu A1 purchase) isn't available when Phase 4 starts, outsource the internals via SendCutSend / Craftcloud / Shapeways. Budget impact: +$60-80/clock, still within the $150-180/clock spec target. See `docs/superpowers/specs/2026-04-14-3d-printer-purchase-decision.md` § Fallback.

---

## Phase Roadmap

| Phase | Goal | Entry criteria | Status |
|---|---|---|---|
| 0 | Repo + CI + integrations live | Empty repo | Ready now |
| 1 | Native-testable firmware logic | Phase 0 complete | Ready after P0 |
| 2 | Breadboard hardware bring-up | Parts on hand | Needs parts purchase (~2027) |
| 3 | KiCad schematic + PCB v1 | P2 bring-up works end-to-end | Needs P2 learnings |
| 4 | Enclosure CAD + laser-cut face | PCB v1 dimensions final | Needs P3 |
| 5 | Emory unit 1 (prototype) | PCBs fabbed, enclosure parts cut | Needs P3+P4 |
| 6 | Nora unit 2 + delivery prep | Emory unit validated | Needs P5 |

---

## Phase 0: Repo & CI Infrastructure

**Goal:** Every future PR gets automated build, native test, security scan, and review. Release-pilot can cut tags. Repo matches the chainguard-dev public-repo-setup baseline.

**Files created in this phase:**
- `.github/workflows/ci.yml` — build both PlatformIO envs + run native tests
- `.github/workflows/codeql.yml` — CodeQL for C/C++
- `.github/workflows/scan.yml` — trivy / secret scan
- `.github/workflows/claude-review.yml` — hookshot claude-review-code
- `.github/workflows/gas-fixer.yml` — hookshot gas-fixer for Dependabot
- `.github/workflows/release-pilot.yml` — release-pilot tagger
- `.github/dependabot.yml`
- `.github/CODEOWNERS`
- `.github/pull_request_template.md`
- `LICENSE` (MIT)
- `CODE_OF_CONDUCT.md`
- `CONTRIBUTING.md`
- `SECURITY.md`
- `platformio.ini` (env skeleton, expanded in Phase 1)
- `conventions.md` (project conventions for future Claude sessions)

---

### Task 0.1: Invoke public-repo-setup skill for baseline files

**Files:**
- Create: `LICENSE`, `CODE_OF_CONDUCT.md`, `CONTRIBUTING.md`, `SECURITY.md`
- Create: `.github/pull_request_template.md`, `.github/CODEOWNERS`, `.github/dependabot.yml`

- [ ] **Step 1: Invoke the public-repo-setup skill**

The skill handles license choice, code of conduct, contributing guide, security policy, dependabot, and CODEOWNERS for a public personal repo. Run it scoped to "ESP32 firmware / personal hardware project, MIT license, single maintainer (@dakaneye)."

Expected outputs after skill run:
- `LICENSE` — MIT, copyright holder "Samuel Dacanay"
- `CODE_OF_CONDUCT.md` — Contributor Covenant 2.1
- `CONTRIBUTING.md` — brief: how to build, how to open a PR, branch prefix `dakaneye/`
- `SECURITY.md` — report to samjdacanay@gmail.com, no bug bounty
- `.github/dependabot.yml` — weekly for `github-actions` and `pip` (for platformio tooling)
- `.github/CODEOWNERS` — `* @dakaneye`
- `.github/pull_request_template.md` — summary / test plan / checklist

- [ ] **Step 2: Verify files and commit**

Run:
```bash
ls LICENSE CODE_OF_CONDUCT.md CONTRIBUTING.md SECURITY.md .github/dependabot.yml .github/CODEOWNERS .github/pull_request_template.md
```

Expected: all seven paths print without error.

- [ ] **Step 3: Commit**

```bash
git add LICENSE CODE_OF_CONDUCT.md CONTRIBUTING.md SECURITY.md .github/
git commit -m "chore: scaffold public-repo baseline files"
```

---

### Task 0.2: Write project conventions file

**Files:**
- Create: `conventions.md`

- [ ] **Step 1: Write conventions.md**

```markdown
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
Chelsea's clock (inspiration, not shared code): https://github.com/dakaneye/word-clock
```

- [ ] **Step 2: Commit**

```bash
git add conventions.md
git commit -m "docs: project conventions for future contributors"
```

---

### Task 0.3: Scaffold PlatformIO project skeleton

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/src/main.cpp` (stub)
- Create: `firmware/lib/core/README.md` (placeholder so git tracks the dir)
- Create: `firmware/test/README.md` (placeholder)
- Create: `firmware/configs/config_emory.h`, `firmware/configs/config_nora.h`

- [ ] **Step 1: Write platformio.ini**

Create `firmware/platformio.ini`:

```ini
[platformio]
default_envs = emory, nora
src_dir = src
lib_dir = lib
test_dir = test

[env]
platform = espressif32@^6.8.0
framework = arduino
board = esp32dev
monitor_speed = 115200
build_flags =
    -std=gnu++17
    -Wall
    -Wextra
    -I configs
lib_deps =
    fastled/FastLED@^3.7.0
    adafruit/RTClib@^2.1.4
    arduino-libraries/NTPClient@^3.2.1

[env:emory]
build_flags = ${env.build_flags} -D CLOCK_TARGET_EMORY=1 -include config_emory.h

[env:nora]
build_flags = ${env.build_flags} -D CLOCK_TARGET_NORA=1 -include config_nora.h

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++17
    -Wall
    -Wextra
    -I lib/core/include
lib_compat_mode = off
test_ignore = embedded/*
```

- [ ] **Step 2: Write stub main.cpp**

Create `firmware/src/main.cpp`:

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
}

void loop() {
  delay(1000);
}
```

- [ ] **Step 3: Write per-kid config headers**

Create `firmware/configs/config_emory.h`:

```cpp
#pragma once

#define CLOCK_NAME         "EMORY"
#define CLOCK_BIRTH_MONTH  10
#define CLOCK_BIRTH_DAY    6
#define CLOCK_BIRTH_YEAR   2023
#define CLOCK_BIRTH_HOUR   18
#define CLOCK_BIRTH_MINUTE 10
#define CLOCK_ACROSTIC     "EMORY1062023"
#define CLOCK_WOOD_LABEL   "birch"
```

Create `firmware/configs/config_nora.h`:

```cpp
#pragma once

#define CLOCK_NAME         "NORA"
#define CLOCK_BIRTH_MONTH  3
#define CLOCK_BIRTH_DAY    19
#define CLOCK_BIRTH_YEAR   2025
#define CLOCK_BIRTH_HOUR   9
#define CLOCK_BIRTH_MINUTE 17
#define CLOCK_ACROSTIC     "NORAMAR192025"
#define CLOCK_WOOD_LABEL   "walnut"
```

- [ ] **Step 4: Write placeholder READMEs for empty dirs**

Create `firmware/lib/core/README.md`:
```markdown
# lib/core

Platform-independent modules. Everything here must compile on the host with `pio test -e native`.
No `#include <Arduino.h>`. See `conventions.md`.
```

Create `firmware/test/README.md`:
```markdown
# test

Native Unity tests for `lib/core/`. Run locally with `pio test -e native`.
```

- [ ] **Step 5: Verify both ESP32 envs compile**

Run from `firmware/`:
```bash
pio run -e emory
pio run -e nora
```

Expected: both envs finish with `SUCCESS`. Binary sizes print. No warnings about undefined `CLOCK_NAME`.

- [ ] **Step 6: Commit**

```bash
git add firmware/
git commit -m "feat: scaffold platformio project with emory/nora envs"
```

---

### Task 0.4: Build CI workflow for firmware compile

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Write ci.yml**

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

permissions:
  contents: read

jobs:
  build-firmware:
    name: Build firmware (${{ matrix.env }})
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        env: [emory, nora]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: Cache PlatformIO
        uses: actions/cache@v4
        with:
          path: |
            ~/.platformio/.cache
            ~/.cache/pip
          key: pio-${{ runner.os }}-${{ hashFiles('firmware/platformio.ini') }}
      - name: Install PlatformIO
        run: pip install --upgrade platformio
      - name: Build ${{ matrix.env }}
        working-directory: firmware
        run: pio run -e ${{ matrix.env }}

  native-tests:
    name: Native tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: Cache PlatformIO
        uses: actions/cache@v4
        with:
          path: |
            ~/.platformio/.cache
            ~/.cache/pip
          key: pio-native-${{ runner.os }}-${{ hashFiles('firmware/platformio.ini') }}
      - name: Install PlatformIO
        run: pip install --upgrade platformio
      - name: Run native tests
        working-directory: firmware
        run: pio test -e native --verbose
```

- [ ] **Step 2: Commit and push to a PR branch to verify**

```bash
git checkout -b dakaneye/ci-firmware
git add .github/workflows/ci.yml
git commit -m "ci: build both firmware envs and run native tests on PR"
git push -u origin dakaneye/ci-firmware
gh pr create --fill
```

- [ ] **Step 3: Verify CI passes**

Run:
```bash
gh pr checks --watch
```

Expected: `build-firmware (emory)`, `build-firmware (nora)`, `native-tests` all pass. (native-tests may be a no-op until Phase 1 adds tests — that's fine, Unity reports "no tests" as success.)

- [ ] **Step 4: Merge**

```bash
gh pr merge --squash --delete-branch
git checkout main && git pull
```

---

### Task 0.5: Add CodeQL workflow

**Files:**
- Create: `.github/workflows/codeql.yml`

- [ ] **Step 1: Write codeql.yml**

```yaml
name: CodeQL

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  schedule:
    - cron: '0 6 * * 1'

permissions:
  actions: read
  contents: read
  security-events: write

jobs:
  analyze:
    name: Analyze C++
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: Install PlatformIO
        run: pip install --upgrade platformio
      - uses: github/codeql-action/init@v3
        with:
          languages: cpp
      - name: Build for CodeQL
        working-directory: firmware
        run: pio run -e emory
      - uses: github/codeql-action/analyze@v3
        with:
          category: /language:cpp
```

- [ ] **Step 2: Commit on a PR branch and verify**

```bash
git checkout -b dakaneye/codeql
git add .github/workflows/codeql.yml
git commit -m "ci: add CodeQL analysis for firmware c++"
git push -u origin dakaneye/codeql
gh pr create --fill
gh pr checks --watch
```

Expected: CodeQL job passes (may take 5-10 min).

- [ ] **Step 3: Merge**

```bash
gh pr merge --squash --delete-branch
git checkout main && git pull
```

---

### Task 0.6: Wire up hookshot claude-review-code

**Files:**
- Create: `.github/workflows/claude-review.yml`

- [ ] **Step 1: Check the hookshot composite action API**

Run:
```bash
gh api repos/dakaneye/hookshot/contents/claude-review-code/action.yml --jq '.content' | base64 -d | head -80
```

Read the `inputs:` section. Note required vs optional inputs (commonly: `anthropic-api-key` secret, `github-token`, PR number, paths filter).

- [ ] **Step 2: Write claude-review.yml using the action's documented inputs**

Create `.github/workflows/claude-review.yml`:

```yaml
name: Claude review

on:
  pull_request:
    branches: [main]

permissions:
  contents: read
  pull-requests: write

jobs:
  review:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - uses: dakaneye/hookshot/claude-review-code@main
        with:
          anthropic-api-key: ${{ secrets.ANTHROPIC_API_KEY }}
          github-token: ${{ secrets.GITHUB_TOKEN }}
```

If Step 1 revealed different or additional required inputs, adjust this yaml to match the action's schema exactly. **Do not guess inputs** — copy them from the action.yml file read in step 1.

- [ ] **Step 3: Ensure ANTHROPIC_API_KEY repo secret exists**

Run:
```bash
gh secret list | grep ANTHROPIC_API_KEY || echo "MISSING"
```

If missing:
```bash
op read "op://Private/Anthropic API Key/credential" | gh secret set ANTHROPIC_API_KEY
```

- [ ] **Step 4: Commit on a PR branch to exercise the action**

```bash
git checkout -b dakaneye/hookshot-review
git add .github/workflows/claude-review.yml
git commit -m "ci: wire hookshot claude-review-code"
git push -u origin dakaneye/hookshot-review
gh pr create --fill
```

- [ ] **Step 5: Verify the review posts a PR comment**

Run:
```bash
gh pr checks --watch
gh pr view --json comments --jq '.comments[] | {author: .author.login, body: .body[0:200]}'
```

Expected: a comment from `github-actions[bot]` or claude-review bot with at least some review content.

- [ ] **Step 6: Merge**

```bash
gh pr merge --squash --delete-branch
git checkout main && git pull
```

---

### Task 0.7: Wire up hookshot gas-fixer for Dependabot PRs

**Files:**
- Create: `.github/workflows/gas-fixer.yml`

- [ ] **Step 1: Check gas-fixer action signature**

```bash
gh api repos/dakaneye/hookshot/contents/gas-fixer/action.yml --jq '.content' | base64 -d | head -80
```

Note: if gas-fixer isn't yet implemented in hookshot (it was filed as issue #2 on Apr 1, 2026 per memory notes), skip this task and leave a TODO in `conventions.md` to revisit when the action lands.

- [ ] **Step 2: If the action exists, write the workflow**

Create `.github/workflows/gas-fixer.yml`:

```yaml
name: GAS auto-fix

on:
  pull_request:
    branches: [main]

permissions:
  contents: write
  pull-requests: write
  security-events: read

jobs:
  fix:
    if: github.actor == 'dependabot[bot]'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dakaneye/hookshot/gas-fixer@main
        with:
          anthropic-api-key: ${{ secrets.ANTHROPIC_API_KEY }}
          github-token: ${{ secrets.GITHUB_TOKEN }}
```

Adjust inputs to match the action's real schema from step 1.

- [ ] **Step 3: Commit and merge via PR**

```bash
git checkout -b dakaneye/hookshot-gas-fixer
git add .github/workflows/gas-fixer.yml
git commit -m "ci: wire hookshot gas-fixer for dependabot PRs"
git push -u origin dakaneye/hookshot-gas-fixer
gh pr create --fill
gh pr checks --watch
gh pr merge --squash --delete-branch
git checkout main && git pull
```

(Full end-to-end verification waits for the next Dependabot PR.)

---

### Task 0.8: Wire up release-pilot

**Files:**
- Create: `.github/workflows/release-pilot.yml`
- Create: `.release-pilot.yml` (config, if the tool uses one)

- [ ] **Step 1: Check release-pilot invocation pattern**

```bash
gh api repos/dakaneye/release-pilot/contents/README.md --jq '.content' | base64 -d | sed -n '1,120p'
```

Read the "Usage" section to learn: trigger events, required secrets, config file name, tag format.

- [ ] **Step 2: Write release-pilot.yml per the docs**

Create `.github/workflows/release-pilot.yml` following the pattern from the README. Typical shape:

```yaml
name: Release

on:
  push:
    branches: [main]

permissions:
  contents: write
  pull-requests: write

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - uses: dakaneye/release-pilot@main
        with:
          github-token: ${{ secrets.GITHUB_TOKEN }}
```

If release-pilot requires a config file for version bumping rules, create `.release-pilot.yml` per its schema. For a firmware project, `v0.1.0` starting version, conventional-commits mapping is typical.

- [ ] **Step 3: Commit, merge, and verify a release draft is created**

```bash
git checkout -b dakaneye/release-pilot
git add .github/workflows/release-pilot.yml .release-pilot.yml 2>/dev/null
git commit -m "ci: wire release-pilot for automated version tags"
git push -u origin dakaneye/release-pilot
gh pr create --fill
gh pr checks --watch
gh pr merge --squash --delete-branch
git checkout main && git pull
```

- [ ] **Step 4: Verify release-pilot reacts to the merge**

Run:
```bash
gh run list --workflow=release-pilot.yml --limit 1
gh release list
```

Expected: a release PR or draft exists for `v0.1.0`.

---

### Task 0.9: Add trivy/secret scan workflow

**Files:**
- Create: `.github/workflows/scan.yml`

- [ ] **Step 1: Write scan.yml (trivy fs + gitleaks)**

```yaml
name: Security scan

on:
  pull_request:
    branches: [main]
  schedule:
    - cron: '0 7 * * 1'

permissions:
  contents: read
  security-events: write

jobs:
  trivy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: aquasecurity/trivy-action@0.28.0
        with:
          scan-type: fs
          scan-ref: .
          format: sarif
          output: trivy.sarif
          severity: CRITICAL,HIGH
      - uses: github/codeql-action/upload-sarif@v3
        with:
          sarif_file: trivy.sarif

  gitleaks:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - uses: gitleaks/gitleaks-action@v2
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

- [ ] **Step 2: Commit via PR and verify**

```bash
git checkout -b dakaneye/scan
git add .github/workflows/scan.yml
git commit -m "ci: add trivy fs + gitleaks secret scanning"
git push -u origin dakaneye/scan
gh pr create --fill
gh pr checks --watch
gh pr merge --squash --delete-branch
git checkout main && git pull
```

---

### Task 0.10: Phase 0 wrap-up — verify repo state

- [ ] **Step 1: Confirm all workflows present and green on main**

```bash
ls .github/workflows/
gh run list --branch main --limit 10
```

Expected: `ci.yml`, `codeql.yml`, `claude-review.yml`, `gas-fixer.yml` (or TODO), `release-pilot.yml`, `scan.yml`. All recent runs on `main` are green.

- [ ] **Step 2: Confirm branch protection on main**

```bash
gh api repos/dakaneye/word-clock-for-my-daughters/branches/main/protection 2>/dev/null || echo "not protected"
```

If not protected, enable:
```bash
gh api -X PUT repos/dakaneye/word-clock-for-my-daughters/branches/main/protection \
  -f required_status_checks[strict]=true \
  -f required_status_checks[contexts][]=build-firmware (emory) \
  -f required_status_checks[contexts][]=build-firmware (nora) \
  -f required_status_checks[contexts][]=native-tests \
  -f enforce_admins=false \
  -f required_pull_request_reviews[required_approving_review_count]=0 \
  -f restrictions=
```

(Single-maintainer repo — PR-required-but-self-approved-via-checks is the right balance.)

- [ ] **Step 3: Tag Phase 0 complete**

If release-pilot didn't already cut v0.1.0, tag manually:

```bash
git tag -a phase-0-complete -m "Phase 0: repo + CI infrastructure complete"
git push origin phase-0-complete
```

**Phase 0 done.** Every future PR runs: firmware build (both envs), native tests, CodeQL, trivy, gitleaks, claude-review. Release-pilot drafts versions. Dependabot PRs get auto-fixed by gas-fixer (when available).

---

## Phase 1: Native-Testable Firmware Logic

**Goal:** Implement every pure-logic firmware module that can be tested on a laptop with no ESP32 attached. This phase produces zero hardware output but catches the highest-value bugs (wrong time phrasing, wrong acrostic placement, wrong holiday dates) years before a PCB is fabbed.

**Modules in scope (all under `firmware/lib/core/`):**
- `grid` — 13×13 letter grid definition, coordinates for each word group
- `time_to_words` — `(hour, minute) → list of words to light`, ported from Chelsea's clock and extended with AT NIGHT / NOON / MORNING / AFTERNOON / EVENING phrasing per spec
- `acrostic` — given a grid + filler letter sequence, compute (row, col, char) tuples for the filler cells
- `holidays` — given a date, return the holiday palette (or none). Includes computed holidays (Easter, MLK day, etc.)
- `birthday` — given a date + config, return (is_birthday, is_birth_minute)
- `dim_schedule` — given a local hour/minute, return the dim multiplier (1.0 daytime, ~0.1 bedtime)

**Out of scope for Phase 1:** FastLED calls, RTC I/O, NTP, WiFi, I2S, buttons — all hardware-touching code comes in Phase 2+.

**Sub-plan trigger:** This phase has ~6 modules with ~15-20 tasks. **Rerun `superpowers:writing-plans` against this phase scope** to produce the bite-sized TDD plan for each module. That plan's entry criteria: Phase 0 complete, `pio test -e native` runs successfully (even with zero tests).

**Expected task shape per module:**
1. Define the header (types + function signatures) in `lib/core/include/<module>.h`
2. Write failing Unity test for the simplest case
3. `pio test -e native` → fail
4. Write minimal implementation in `lib/core/<module>.cpp`
5. `pio test -e native` → pass
6. Add edge-case tests (midnight, noon, DST boundaries, Feb 29, etc.)
7. Implement until green
8. Commit per module or per coherent group of tests

**Port-from-Chelsea work:** copy `time_to_words.cpp`/`.h` from `dakaneye/word-clock/word-clock/` into `firmware/lib/core/`, add `#include <Arduino.h>` guards, replace `String` with `std::string_view` / `const char*`, extend the word set for the new phrasings (IN THE MORNING, AT NIGHT, NOON). **Do not git-submodule or cross-reference** — it's a fresh copy owned by this repo, per the user directive.

**Phase 1 exit criteria:**
- All six modules have ≥80% branch coverage under native tests
- `pio test -e native` runs in <10s, passes on CI
- A golden-file test round-trips all 1440 minutes of a day through `time_to_words` for both clock targets
- Holiday dates verified against a known-good calendar for 2025-2035

---

## Phase 2: Breadboard Hardware Bring-Up

**Entry criteria:** Phase 1 complete. Parts purchased: ESP32 devkit, WS2812B strip (≥30 LEDs), DS3231 breakout, MAX98357A breakout, small speaker, microSD breakout, USB-C breakout, three tact switches, breadboard, jumpers. Budget ~$60.

**Goal:** End-to-end demo on a breadboard — ESP32 lights words for real time from NTP/DS3231, plays a stored MP3 through the speaker on button press, dims at night. No PCB, no enclosure, no wood.

**Deliverables:**
- `firmware/src/` modules wired: `display` (FastLED), `rtc` (RTClib), `ntp`, `audio` (I2S + SD), `buttons`, `wifi_provision`, `main.cpp` state machine
- `firmware/test-sketches/` — small manual sketches for each IC (LED-only, audio-only, RTC-only, button-only) for bring-up debugging
- Video demo of full breadboard running both envs (flash emory, then nora)

**Open questions for this phase's own plan:**
- SD card filesystem layout: flat root with `lullaby.mp3` + `birth.mp3`? Or `emory/` + `nora/` subdirs on one card that serves both builds during dev?
- Audio encoding: MP3 decoded in-firmware (`ESP8266Audio` works on ESP32) vs pre-converted WAV?
- Captive portal library: `WiFiManager` (battle-tested) vs custom (full control)?
- WiFi credentials storage: `Preferences` NVS (standard) — confirm.

**Sub-plan trigger:** Rerun `superpowers:writing-plans` when parts are on hand. Plan will cover ~8-10 modules, likely 25-30 TDD tasks.

---

## Phase 3: KiCad Schematic + PCB v1

**Entry criteria:** Phase 2 breadboard fully functional. Pin assignments finalized and documented. Each IC known-good on chosen ESP32 pins.

**Goal:** Produce JLCPCB-fab-ready gerbers + BOM + CPL for a 7"×7" double-sided board. Fab and populate 5 boards (MOQ).

**Deliverables:**
- `hardware/word-clock.kicad_pro` + associated schematic/PCB/lib files
- `hardware/BOM.csv` using LCSC basic parts where possible
- `hardware/CPL.csv` pick-and-place file
- `hardware/gerbers/` production output zip
- `hardware/docs/pinmap.md` — canonical ESP32 pin assignments
- One DRC-clean, bring-up-verified revision in-hand

**Open questions for this phase's own plan:**
- Exact ESP32 module SKU (ESP32-WROOM-32E vs -32D vs C3/S3 variant)
- WS2812B footprint choice — JLCPCB-stocked reverse-mount vs top-side
- Power budget: 25 LEDs × 60mA full-white worst case = 1.5A; sizing of bulk caps and traces
- USB-C CC resistor config for 5V-only device (no PD)
- Programming header: castellated + pogo pins vs dedicated header?
- JLCPCB vs PCBWay vs OSHPark — cost/lead-time tradeoff for small runs

**Sub-plan trigger:** Rerun `superpowers:writing-plans` when Phase 2 is blessed.

---

## Phase 4: Enclosure CAD + Laser-Cut Face

**Entry criteria:** PCB v1 in hand. Final PCB dimensions measured and photographed. All through-board features (USB-C, buttons, speaker, SD slot) located.

**Goal:** All wood + 3D-printed parts designable and orderable from CAD files committed to the repo.

**Deliverables:**
- `enclosure/face.svg` — 13×13 cut-through letter grid, 7"×7", for laser cutter
- `enclosure/face-engraving.svg` (if any etching for dedication)
- `enclosure/back-panel-emory.svg`, `enclosure/back-panel-nora.svg` — includes USB-C cutout and engraved dedication
- `enclosure/frame-sides.svg` — 4 pieces, wood species determined at order time
- `enclosure/internals.step` + `.stl` — 3D-printed light channel honeycomb, PCB standoffs, speaker mount
- `enclosure/assembly.md` — glue-up order, finishing notes
- Test laser-cut in cardboard + test 3D print in cheap PLA before ordering real wood

**3D-printing branch point (decide at phase entry):**

- **Path A — In-house print** (preferred): Bambu A1 is set up and calibrated per `docs/superpowers/specs/2026-04-14-3d-printer-purchase-decision.md`. Print the light-channel honeycomb, PCB standoffs, speaker mount, button housings in white PLA. Zero marginal cost per clock beyond filament (~$10).
- **Path B — Outsource**: If the printer isn't available (not purchased, broken, otherwise blocked), send the same STL/STEP files to an online FDM service. Primary: SendCutSend (also the face-laser vendor — single PO). Alternatives: Craftcloud (aggregator), Shapeways/JLC3DP. Budget delta: +$60-80/clock. See fallback matrix in the 3D-printer spec.
- **Vendor-neutrality requirement:** CAD files committed to `enclosure/` must be standard STEP + STL with no printer-specific slicer settings embedded. Path B must work without modifying the CAD sources.
- **Decision trigger:** At Phase 4 kickoff, check printer status. If operational and calibrated, Path A. If not, Path B with no re-plan needed.

**Open questions for this phase's own plan:**
- Laser-cut vendor: SendCutSend (easy, limited wood species) vs Ponoko (more species) vs local maker-space (cheapest, most hassle) — note: choosing SendCutSend gives a one-vendor story if Path B for 3D printing is also needed.
- CAD tool: Fusion 360 (Mac-friendly, parametric) vs FreeCAD (open, clunky) vs OpenSCAD for internals only
- Diffuser: white paper (cheap, diffuses well, fragile) vs 0.5mm frosted PETG (durable, less diffuse)
- Light-channel wall thickness and depth — needs empirical testing with real LEDs at target brightness
- Wood finish: natural oil (birch and walnut both look great oiled) vs lacquer

**Sub-plan trigger:** Rerun `superpowers:writing-plans` when Phase 3 PCB is in hand and measured.

---

## Phase 5: Emory Unit 1 — Prototype Assembly + Field Test

**Entry criteria:** Phase 3 PCBs fabbed + populated. Phase 4 wood + 3D parts in hand. Emory config is active.

**Goal:** One fully assembled Emory clock that runs on my desk for ≥30 consecutive days without intervention. This is the prototype: every defect found here feeds back into PCB v2 (if needed) or enclosure v2.

**Deliverables:**
- One assembled Emory clock, booting, keeping time, dimming at night, playing lullaby on button press, birthday-mode tested by manually setting RTC to Oct 6
- Defect log in `docs/build-log-emory.md`
- If major PCB defects found: Phase 3.5 — PCB v2 spin, re-fab.

**Exit criteria:** 30-day continuous-operation test logged. All spec behaviors verified.

**Sub-plan trigger:** Rerun `superpowers:writing-plans` after Phase 4. Sub-plan will be shorter — it's mostly assembly + QA checklist, not new code.

---

## Phase 6: Nora Unit 2 + Delivery

**Entry criteria:** Phase 5 blessed. 30-day test passed. Any PCB / enclosure v2 spins complete.

**Goal:** Build Nora's clock to the final revised design, in walnut. Prepare delivery packaging. Record voice memo and lullaby for both clocks onto final SD cards.

**Deliverables:**
- Nora clock assembled and 7-day bench-tested
- Both clocks' SD cards populated with Dad's voice recordings
- Packaging (gift box / dedication card)
- Both clocks boxed and stored until each delivery date

**Long-haul items (through 2030 / 2032):**
- Annual "power it on, verify it still works" check
- Re-flash with any firmware fixes released
- Re-record audio if Dad's voice ages noticeably (optional, probably fine)

---

## Self-Review

**Spec coverage check:**

- ESP32 + WiFi + NTP + DS3231 → Phase 1 logic + Phase 2 hardware. ✓
- WS2812B, ~25 LEDs → Phase 2 breadboard, Phase 3 PCB. ✓
- MAX98357A + microSD audio → Phase 2 breadboard, Phase 3 PCB. ✓
- USB-C power only, no power button → Phase 3 PCB. ✓
- Three tact switches → Phase 2 + Phase 3. ✓
- 7"×7" wood face, birch/walnut, shadow-box → Phase 4. ✓
- 3D-printed internals → Phase 4. ✓
- 13×13 letter grid, per-kid row 9 → Phase 1 `grid` module (grid data per kid). ✓
- Hidden acrostic → Phase 1 `acrostic` module. ✓
- Warm white default, 5-min update → Phase 1 (color const in config) + Phase 2 (display module). ✓
- Bedtime dim 7PM–8AM → Phase 1 `dim_schedule` module. ✓
- Birthday mode + rainbow cycle → Phase 1 `birthday` module + Phase 2 animation. ✓
- Birth-minute voice memo → Phase 1 `birthday` trigger + Phase 2 audio module. ✓
- Holiday modes (9 holidays, some computed) → Phase 1 `holidays` module. ✓
- Audio button behavior → Phase 2 `buttons` + `audio` modules. ✓
- Captive-portal WiFi provisioning → Phase 2 `wifi_provision` module. ✓
- Per-clock config via compile-time flag → Phase 0 (configs) + Phase 1 (consumption). ✓
- Budget ~$150-180/clock → Phase 2+3+4 BOMs will validate. ✓
- Delivery 2030/2032 → Phase 5/6. ✓

No unplanned spec requirements.

**Placeholder scan:** Phase 0 tasks are fully detailed with exact code, commands, and expected output. Phases 1–6 are *phase outlines* (explicitly scoped as "rerun `writing-plans` against this phase"), not TDD task plans — their "open questions" lists are legitimate planning deliverables for those sub-plans, not placeholders in Phase 0's plan.

**Type consistency:** `CLOCK_NAME`, `CLOCK_BIRTH_MONTH`, `CLOCK_BIRTH_DAY`, `CLOCK_BIRTH_YEAR`, `CLOCK_BIRTH_HOUR`, `CLOCK_BIRTH_MINUTE`, `CLOCK_ACROSTIC`, `CLOCK_WOOD_LABEL` are defined identically across `config_emory.h` and `config_nora.h`. The `emory` and `nora` env names are consistent across `platformio.ini`, `ci.yml`, and branch-protection rules.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task in Phase 0, review between tasks, fast iteration.
2. **Inline Execution** — Execute Phase 0 tasks in this session using `executing-plans`, batch execution with checkpoints.

Which approach?
