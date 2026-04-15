# Phase 1: Firmware Logic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement every pure-logic firmware module that compiles and tests on the host (no ESP32 hardware required). After this phase, `pio test -e native` covers the brains of the clock — time phrasing, grid coordinates, holiday date math, birthday triggers, and dim scheduling — so bugs in the hardest-to-debug code are caught years before PCBs exist.

**Architecture:** Five modules under `firmware/lib/core/`, each in its own header + source file, each with a dedicated Unity test file under `firmware/test/`. All modules are pure functions over plain-old-data types (enums, small structs) — no global state, no Arduino dependencies. Per-kid configuration is passed as arguments rather than `#ifdef`-selected, so tests can exercise both Emory and Nora configurations in the same native binary. Firmware (Phase 2) will wrap these modules with Arduino-dependent glue (display driver, RTC reads, button ISRs); those wrappers are out of scope here.

**Tech Stack:** C++17 (matches the `-std=gnu++17` flag in `platformio.ini`), Unity test framework (already pulled in by PlatformIO's `test_framework = unity`), no external libraries. All source compiles cleanly with `-Wall -Wextra`.

**Phase 1 exit criteria:**
- `pio test -e native` runs in under 10 seconds and passes 100%
- Every module has tests covering: happy path, all documented edge cases, boundary conditions
- A golden-file test round-trips all 1440 minutes of a day through `time_to_words` for both Emory and Nora birth contexts, hash-verified against a checked-in fixture
- No `#include <Arduino.h>` anywhere in `lib/core/` or `test/`

## Module map

| # | Module | Purpose | Depends on |
|---|---|---|---|
| 1 | `grid` | 13×13 letter layout + word→cell coordinates + filler-cell positions; also exposes both Emory and Nora grid variants | none |
| 2 | `time_to_words` | `(hour24, minute) → WordSet` emitting the list of `WordId`s to light, including MORNING/AFTERNOON/EVENING/NIGHT/NOON suffix | `grid::WordId` |
| 3 | `holidays` | `(year, month, day) → Palette` for the 9 holidays plus a default `WARM_WHITE`; includes computed dates (Easter, MLK Day, etc.) | none |
| 4 | `birthday` | `(now, BirthdayConfig) → BirthdayState` reporting whether today is the birthday and whether we're at the birth-minute | none |
| 5 | `dim_schedule` | `(hour24, minute) → brightness [0.0–1.0]` implementing the 19:00–07:59 bedtime dim | none |

## Locked design decisions

These were open in the spec's "Open Items for Planning Phase" section and are decided here so tasks can reference concrete types:

1. **Word identity.** `enum class WordId : uint8_t` under namespace `wc`. Values: IT, IS, TEN_MIN, HALF, QUARTER, TWENTY, FIVE_MIN, MINUTES, PAST, TO, ONE, TWO, THREE, FOUR, FIVE_HR, SIX, SEVEN, EIGHT, NINE, TEN_HR, ELEVEN, TWELVE, OCLOCK, NOON, IN, THE, AT, MORNING, AFTERNOON, EVENING, NIGHT, HAPPY, BIRTH, DAY, NAME. No `W_` prefix — enum class namespaces it.
2. **Cell coordinate type.** `struct CellSpan { uint8_t row; uint8_t col; uint8_t length; };` — every word is a horizontal run on a single row. Row and col are 0-indexed (row 0 is top, col 0 is left).
3. **Grid variants.** Both Emory and Nora grids live in `lib/core/src/grid.cpp` as two `static constexpr` arrays. A runtime function `grid::get(ClockTarget)` returns the right one. Firmware selects at compile time; tests iterate both.
4. **WordSet output.** `struct WordSet { WordId words[8]; uint8_t count; };` — 8 is sufficient (IT + IS + TWENTY + FIVE_MIN + MINUTES + PAST + hour + NOON is the maximum at 8 words).
5. **Time-of-day cutoffs** (chosen; not in spec):
   - MORNING: 05:00–11:59
   - NOON (no OCLOCK, no IN/THE/AT): exactly 12:00
   - AFTERNOON: 12:05–16:59
   - EVENING: 17:00–20:59
   - NIGHT: 21:00–04:59
   - MIDNIGHT special case: exactly 00:00 reads "IT IS TWELVE AT NIGHT" (no OCLOCK).
   - Any other `xx:00` reads "IT IS [HOUR] OCLOCK [suffix]".
6. **Palette enum.** `enum class Palette : uint8_t { WARM_WHITE, MLK_PURPLE, VALENTINES, WOMEN_PURPLE, EARTH_DAY, EASTER_PASTEL, JUNETEENTH, INDIGENOUS, HALLOWEEN, CHRISTMAS };` — color values (RGB) live in the display module (Phase 2), not here.
7. **Birthday config.** Passed as `struct BirthdayConfig { uint8_t month; uint8_t day; uint8_t hour; uint8_t minute; };` — tests pass Emory's and Nora's literal values; firmware constructs it from the compile-time `CLOCK_BIRTH_*` macros.

## File structure

```
firmware/
├── lib/core/
│   ├── include/
│   │   ├── word_id.h           # WordId enum (shared across modules)
│   │   ├── grid.h
│   │   ├── time_to_words.h
│   │   ├── holidays.h
│   │   ├── birthday.h
│   │   └── dim_schedule.h
│   └── src/
│       ├── grid.cpp
│       ├── time_to_words.cpp
│       ├── holidays.cpp
│       ├── birthday.cpp
│       └── dim_schedule.cpp
└── test/
    ├── test_grid/test_grid.cpp
    ├── test_time_to_words/test_time_to_words.cpp
    ├── test_time_to_words_golden/
    │   ├── test_time_to_words_golden.cpp
    │   ├── emory_golden.txt          # 1440 lines, one per minute
    │   └── nora_golden.txt
    ├── test_holidays/test_holidays.cpp
    ├── test_birthday/test_birthday.cpp
    └── test_dim_schedule/test_dim_schedule.cpp
```

The existing `firmware/test/test_placeholder/` is removed in Task 1 once `test_grid/` exists.

## Task ordering rationale

Bottom-up: `word_id.h` is shared infrastructure so it comes first (Task 1, bundled with `grid`). `time_to_words` depends only on `WordId`. The rest (`holidays`, `birthday`, `dim_schedule`) are independent and can be done in any order; they're plan-ordered by spec-importance.

---

## Task 1: `word_id.h` + `grid` module

**Files:**
- Create: `firmware/lib/core/include/word_id.h`
- Create: `firmware/lib/core/include/grid.h`
- Create: `firmware/lib/core/src/grid.cpp`
- Create: `firmware/test/test_grid/test_grid.cpp`
- Delete: `firmware/test/test_placeholder/test_placeholder.cpp`

- [ ] **Step 1: Create `word_id.h`**

```cpp
// firmware/lib/core/include/word_id.h
#pragma once

#include <cstdint>

namespace wc {

enum class WordId : uint8_t {
    IT, IS,
    TEN_MIN, HALF, QUARTER, TWENTY, FIVE_MIN, MINUTES, PAST, TO,
    ONE, TWO, THREE, FOUR, FIVE_HR, SIX, SEVEN, EIGHT, NINE,
    TEN_HR, ELEVEN, TWELVE,
    OCLOCK, NOON,
    IN, THE, AT, MORNING, AFTERNOON, EVENING, NIGHT,
    HAPPY, BIRTH, DAY, NAME,
    COUNT
};

} // namespace wc
```

`COUNT` is a sentinel for array-sizing. 36 values total (35 words + COUNT).

- [ ] **Step 2: Create `grid.h`**

```cpp
// firmware/lib/core/include/grid.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc {

enum class ClockTarget : uint8_t { EMORY, NORA };

struct CellSpan {
    uint8_t row;
    uint8_t col;
    uint8_t length;
};

struct Grid {
    // Full 13×13 letter layout, row-major. letters[row * 13 + col].
    char letters[13 * 13];
    // Word → cell span. Indexed by static_cast<uint8_t>(WordId).
    // Invariant: every WordId below COUNT has a valid span.
    CellSpan spans[static_cast<uint8_t>(WordId::COUNT)];
    // Filler cells (those not covered by any word), in reading order.
    // Max 16 fillers (Nora has 13, Emory 12).
    CellSpan fillers[16];
    uint8_t filler_count;
};

// Returns a const reference to the grid for the given target.
// Both grids are static constexpr; the reference lives for the program lifetime.
const Grid& get(ClockTarget target);

} // namespace wc
```

- [ ] **Step 3: Create the failing test**

```cpp
// firmware/test/test_grid/test_grid.cpp
#include <unity.h>
#include "grid.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

// Sanity: both targets return distinct grids.
void test_emory_and_nora_are_distinct(void) {
    const Grid& e = get(ClockTarget::EMORY);
    const Grid& n = get(ClockTarget::NORA);
    TEST_ASSERT_NOT_EQUAL(&e, &n);
}

// Row 1: "IT" at (0, 0, 2), "IS" at (0, 3, 2), "TEN" at (0, 6, 3), "HALF" at (0, 9, 4).
void test_emory_row1_word_spans(void) {
    const Grid& g = get(ClockTarget::EMORY);
    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IT)].row);
    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IT)].col);
    TEST_ASSERT_EQUAL(2, g.spans[static_cast<uint8_t>(WordId::IT)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IS)].row);
    TEST_ASSERT_EQUAL(3, g.spans[static_cast<uint8_t>(WordId::IS)].col);
    TEST_ASSERT_EQUAL(2, g.spans[static_cast<uint8_t>(WordId::IS)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].row);
    TEST_ASSERT_EQUAL(6, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].col);
    TEST_ASSERT_EQUAL(3, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::HALF)].row);
    TEST_ASSERT_EQUAL(9, g.spans[static_cast<uint8_t>(WordId::HALF)].col);
    TEST_ASSERT_EQUAL(4, g.spans[static_cast<uint8_t>(WordId::HALF)].length);
}

// Letters in the grid at span positions spell the word.
void test_emory_word_letters_match_word(void) {
    const Grid& g = get(ClockTarget::EMORY);
    auto check = [&](WordId id, const char* expected) {
        const CellSpan& s = g.spans[static_cast<uint8_t>(id)];
        for (uint8_t i = 0; i < s.length; ++i) {
            TEST_ASSERT_EQUAL_CHAR(expected[i], g.letters[s.row * 13 + s.col + i]);
        }
    };
    check(WordId::IT, "IT");
    check(WordId::IS, "IS");
    check(WordId::TEN_MIN, "TEN");
    check(WordId::HALF, "HALF");
    check(WordId::QUARTER, "QUARTER");
    check(WordId::TWENTY, "TWENTY");
    check(WordId::FIVE_MIN, "FIVE");
    check(WordId::MINUTES, "MINUTES");
    check(WordId::HAPPY, "HAPPY");
    check(WordId::BIRTH, "BIRTH");
    check(WordId::DAY, "DAY");
    check(WordId::NAME, "EMORY");
    check(WordId::NOON, "NOON");
    check(WordId::AFTERNOON, "AFTERNOON");
    check(WordId::EVENING, "EVENING");
    check(WordId::NIGHT, "NIGHT");
    check(WordId::MORNING, "MORNING");
}

// Same invariant for Nora, with NAME == "NORA".
void test_nora_name_is_nora(void) {
    const Grid& g = get(ClockTarget::NORA);
    const CellSpan& s = g.spans[static_cast<uint8_t>(WordId::NAME)];
    TEST_ASSERT_EQUAL(4, s.length);
    for (uint8_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_CHAR("NORA"[i], g.letters[s.row * 13 + s.col + i]);
    }
}

// Every non-COUNT WordId has a non-zero-length span in both grids.
void test_every_word_has_a_span(void) {
    for (auto target : {ClockTarget::EMORY, ClockTarget::NORA}) {
        const Grid& g = get(target);
        for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
            TEST_ASSERT_TRUE_MESSAGE(
                g.spans[i].length > 0,
                "every WordId must have a non-empty span");
        }
    }
}

// Filler cells read in row-major order spell the acrostic.
// Emory: "EMORY1062023" (12 chars). Nora: "NORAMAR192025" (13 chars).
void test_emory_filler_acrostic(void) {
    const Grid& g = get(ClockTarget::EMORY);
    TEST_ASSERT_EQUAL(12, g.filler_count);
    char spelled[13] = {0};
    for (uint8_t i = 0; i < g.filler_count; ++i) {
        const CellSpan& f = g.fillers[i];
        spelled[i] = g.letters[f.row * 13 + f.col];
    }
    TEST_ASSERT_EQUAL_STRING("EMORY1062023", spelled);
}

void test_nora_filler_acrostic(void) {
    const Grid& g = get(ClockTarget::NORA);
    TEST_ASSERT_EQUAL(13, g.filler_count);
    char spelled[14] = {0};
    for (uint8_t i = 0; i < g.filler_count; ++i) {
        const CellSpan& f = g.fillers[i];
        spelled[i] = g.letters[f.row * 13 + f.col];
    }
    TEST_ASSERT_EQUAL_STRING("NORAMAR192025", spelled);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_emory_and_nora_are_distinct);
    RUN_TEST(test_emory_row1_word_spans);
    RUN_TEST(test_emory_word_letters_match_word);
    RUN_TEST(test_nora_name_is_nora);
    RUN_TEST(test_every_word_has_a_span);
    RUN_TEST(test_emory_filler_acrostic);
    RUN_TEST(test_nora_filler_acrostic);
    return UNITY_END();
}
```

- [ ] **Step 4: Delete the placeholder test**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
rm -r firmware/test/test_placeholder
```

- [ ] **Step 5: Run the test — verify it fails with link/compile errors (no grid.cpp yet)**

```bash
cd firmware
pio test -e native 2>&1 | tail -20
```

Expected: FAIL at link time with "undefined reference to `wc::get(wc::ClockTarget)`" or similar. (If it fails at compile time with "grid.h: No such file or directory", check the `build_flags = ${env.build_flags} -I lib/core/include` in the `[env:native]` section — that include path should already be present.)

- [ ] **Step 6: Implement `grid.cpp`**

```cpp
// firmware/lib/core/src/grid.cpp
#include "grid.h"

namespace wc {

namespace {

// Emory's 13×13 letter layout, row-major. Spec §"Letter Grid".
// Row 1 (index 0):  I T E I S M T E N H A L F
// Row 2 (index 1):  Q U A R T E R T W E N T Y
// Row 3 (index 2):  F I V E O M I N U T E S R
// Row 4 (index 3):  Y H A P P Y P A S T T O 1
// Row 5 (index 4):  O N E B I R T H T H R E E
// Row 6 (index 5):  E L E V E N F O U R D A Y
// Row 7 (index 6):  T W O E I G H T S E V E N
// Row 8 (index 7):  N I N E S I X T W E L V E
// Row 9 (index 8):  E M O R Y 0 A T F I V E 6
// Row 10 (index 9): T E N O C L O C K N O O N
// Row 11 (index 10): 2 I N T H E M O R N I N G
// Row 12 (index 11): A F T E R N O O N 0 A T 2
// Row 13 (index 12): E V E N I N G 3 N I G H T
constexpr char EMORY_LETTERS[13 * 13] = {
    'I','T','E','I','S','M','T','E','N','H','A','L','F',
    'Q','U','A','R','T','E','R','T','W','E','N','T','Y',
    'F','I','V','E','O','M','I','N','U','T','E','S','R',
    'Y','H','A','P','P','Y','P','A','S','T','T','O','1',
    'O','N','E','B','I','R','T','H','T','H','R','E','E',
    'E','L','E','V','E','N','F','O','U','R','D','A','Y',
    'T','W','O','E','I','G','H','T','S','E','V','E','N',
    'N','I','N','E','S','I','X','T','W','E','L','V','E',
    'E','M','O','R','Y','0','A','T','F','I','V','E','6',
    'T','E','N','O','C','L','O','C','K','N','O','O','N',
    '2','I','N','T','H','E','M','O','R','N','I','N','G',
    'A','F','T','E','R','N','O','O','N','0','A','T','2',
    'E','V','E','N','I','N','G','3','N','I','G','H','T',
};

// Nora's grid is identical to Emory's except row 9 (index 8):
// N O R A R 1 A T F I V E 9  (filler: R, 1, 9 — acrostic adds MAR 19 2025)
constexpr char NORA_ROW_9[13] = {
    'N','O','R','A','R','1','A','T','F','I','V','E','9',
};

constexpr Grid make_grid(const char* layout, bool nora_row9) {
    Grid g{};
    for (int i = 0; i < 13 * 13; ++i) g.letters[i] = layout[i];
    if (nora_row9) {
        for (int i = 0; i < 13; ++i) g.letters[8 * 13 + i] = NORA_ROW_9[i];
    }

    auto set = [&g](WordId id, uint8_t row, uint8_t col, uint8_t length) {
        g.spans[static_cast<uint8_t>(id)] = {row, col, length};
    };

    // Row 0: IT(0,0,2), IS(0,3,2), TEN_MIN(0,6,3), HALF(0,9,4). Fillers: E(0,2), M(0,5).
    set(WordId::IT, 0, 0, 2);
    set(WordId::IS, 0, 3, 2);
    set(WordId::TEN_MIN, 0, 6, 3);
    set(WordId::HALF, 0, 9, 4);
    // Row 1: QUARTER(1,0,7), TWENTY(1,7,6). No fillers.
    set(WordId::QUARTER, 1, 0, 7);
    set(WordId::TWENTY, 1, 7, 6);
    // Row 2: FIVE_MIN(2,0,4), MINUTES(2,5,7). Wait — layout shows O at col 4, MINUTES M at col 5.
    // Actual layout row 2 = F I V E O M I N U T E S R → FIVE(0-3), O filler(4), MINUTES(5-11), R filler(12).
    set(WordId::FIVE_MIN, 2, 0, 4);
    set(WordId::MINUTES, 2, 5, 7);
    // Row 3: HAPPY(3,1,5), PAST(3,6,4), TO(3,10,2). Fillers: Y(3,0), 1(3,12).
    // Row 3 layout = Y H A P P Y P A S T T O 1 → Y filler(0), HAPPY(1-5), PAST(6-9), TO(10-11), 1 filler(12).
    set(WordId::HAPPY, 3, 1, 5);
    set(WordId::PAST, 3, 6, 4);
    set(WordId::TO, 3, 10, 2);
    // Row 4: ONE(4,0,3), BIRTH(4,3,5), THREE(4,8,5). No fillers.
    // Row 4 layout = O N E B I R T H T H R E E
    set(WordId::ONE, 4, 0, 3);
    set(WordId::BIRTH, 4, 3, 5);
    set(WordId::THREE, 4, 8, 5);
    // Row 5: ELEVEN(5,0,6), FOUR(5,6,4), DAY(5,10,3). No fillers.
    set(WordId::ELEVEN, 5, 0, 6);
    set(WordId::FOUR, 5, 6, 4);
    set(WordId::DAY, 5, 10, 3);
    // Row 6: TWO(6,0,3), EIGHT(6,3,5), SEVEN(6,8,5). No fillers.
    set(WordId::TWO, 6, 0, 3);
    set(WordId::EIGHT, 6, 3, 5);
    set(WordId::SEVEN, 6, 8, 5);
    // Row 7: NINE(7,0,4), SIX(7,4,3), TWELVE(7,7,6). No fillers.
    set(WordId::NINE, 7, 0, 4);
    set(WordId::SIX, 7, 4, 3);
    set(WordId::TWELVE, 7, 7, 6);
    // Row 8: NAME (EMORY or NORA), then AT and FIVE_HR. Fillers differ per kid.
    if (nora_row9) {
        // N O R A R 1 A T F I V E 9 → NAME(0-3), R(4), 1(5), AT(6-7), FIVE_HR(8-11), 9(12).
        set(WordId::NAME, 8, 0, 4);
        set(WordId::AT, 8, 6, 2);
        set(WordId::FIVE_HR, 8, 8, 4);
    } else {
        // E M O R Y 0 A T F I V E 6 → NAME(0-4), 0(5), AT(6-7), FIVE_HR(8-11), 6(12).
        set(WordId::NAME, 8, 0, 5);
        set(WordId::AT, 8, 6, 2);
        set(WordId::FIVE_HR, 8, 8, 4);
    }
    // Row 9: TEN_HR(9,0,3), OCLOCK(9,4,5 covers OCLOCK letters at 4-8: O C L O C K = 6 chars).
    // Wait, row 9 layout = T E N O C L O C K N O O N → TEN(0-2), OCLOCK(3-8), NOON(9-12).
    set(WordId::TEN_HR, 9, 0, 3);
    set(WordId::OCLOCK, 9, 3, 6);
    set(WordId::NOON, 9, 9, 4);
    // Row 10: IN(10,1,2), THE(10,3,3), MORNING(10,6,7). Filler: 2(10,0).
    // Row 10 layout = 2 I N T H E M O R N I N G → 2 filler(0), IN(1-2), THE(3-5), MORNING(6-12).
    set(WordId::IN, 10, 1, 2);
    set(WordId::THE, 10, 3, 3);
    set(WordId::MORNING, 10, 6, 7);
    // Row 11: AFTERNOON(11,0,9), AT(11,10,2). Fillers: 0(11,9), 2(11,12).
    // Row 11 layout = A F T E R N O O N 0 A T 2 → AFTERNOON(0-8), 0 filler(9), AT(10-11), 2 filler(12).
    // Note: AT appears twice in grid (row 8 and row 11). We prefer the row-8 AT for time phrasing;
    // the row-11 AT is a second instance used for "IT IS X AFTERNOON AT Y" — actually spec says
    // "IT IS [time] [hour] IN THE AFTERNOON" — no AT after AFTERNOON. Row-11 AT is unused by
    // time phrasing, effectively a filler. But spans[AT] must point somewhere valid; use row-8.
    set(WordId::AFTERNOON, 11, 0, 9);
    // Row 12: EVENING(12,0,7), NIGHT(12,8,5). Wait layout = E V E N I N G 3 N I G H T — 13 chars.
    // EVENING(0-6), 3 filler(7), NIGHT(8-12).
    set(WordId::EVENING, 12, 0, 7);
    set(WordId::NIGHT, 12, 8, 5);

    // Filler cells in row-major order. For Emory, expected reading: E M O R Y 1 0 6 2 0 2 3
    // From the grid:
    //   Row 0: (0,2)=E, (0,5)=M         → E, M
    //   Row 2: (2,4)=O, (2,12)=R         → O, R
    //   Row 3: (3,0)=Y, (3,12)=1         → Y, 1
    //   Row 8 Emory: (8,5)=0, (8,12)=6   → 0, 6
    //   Row 10: (10,0)=2                 → 2
    //   Row 11: (11,9)=0, (11,12)=2      → 0, 2
    //   Row 12: (12,7)=3                 → 3
    // Concat: E M O R Y 1 0 6 2 0 2 3 ... that's the spec's "EMORY 1 0 6 2 0 2 3".
    // But wait: the "1" after Y must come from row 3 col 12, which we set to '1' in EMORY_LETTERS.
    // Order check: reading row-major, we walk row 0 left→right, then row 1, etc.
    // Row 0 fillers first (E, M), then row 2 (O, R), row 3 (Y, 1), row 8 (0, 6), row 10 (2),
    // row 11 (0, 2), row 12 (3) = E M O R Y 1 0 6 2 0 2 3 (12 chars). Matches spec.
    //
    // For Nora, row 8 fillers differ: row 8 Nora = (8,4)=R, (8,5)=1, (8,12)=9 → R, 1, 9.
    // Concat: E M O R Y R 1 9 2 0 2 3. Hmm that doesn't match "NORAMAR192025".
    // The Nora grid must have different characters in the shared fillers too! Let me re-read spec.
    //
    // Spec §"Nora's grid": "Identical to Emory's except row 9". That is a simplification — the
    // acrostic chars in the shared fillers must encode NORAMAR192025, not EMORY1062023.
    // The shared filler cells (rows 0,2,3,10,11,12) carry different characters between the two
    // clocks because the acrostic differs. The cells are at the same POSITIONS, but the LETTERS
    // in them differ. This means `NORA_LETTERS` must be a full 13×13 layout too, not just row 9.
    //
    // Concretely, for Nora (acrostic = N O R A M A R 1 9 2 0 2 5, 13 chars):
    //   Row 0 fillers: (0,2), (0,5)    → M, A   (chars 5,6 of "NORAMAR192025" = M, A? no —
    //                                              chars 5 is M, 6 is A, but first 4 are NORA
    //                                              which lives in row 8 cols 0-3)
    // Hmm this needs to be walked carefully.
    //
    // RULE: the acrostic "NORAMAR192025" is placed into filler cells in reading order.
    // Nora's filler cells in row-major are:
    //   Row 0: (0,2), (0,5)          → 2 cells
    //   Row 2: (2,4), (2,12)          → 2 cells
    //   Row 3: (3,0), (3,12)          → 2 cells
    //   Row 8: (8,0..3) are NORA word not fillers. (8,4), (8,5), (8,12) are fillers.  → 3 cells
    //   Row 10: (10,0)                → 1 cell
    //   Row 11: (11,9), (11,12)       → 2 cells
    //   Row 12: (12,7)                → 1 cell
    //   Total: 13 cells. Matches acrostic length.
    //
    // The acrostic "N O R A M A R 1 9 2 0 2 5" has its first 4 chars (N,O,R,A) matching the
    // NAME word in row 8. But NAME is a WORD, not fillers. So the first 4 chars of the acrostic
    // are actually CONSUMED by the NAME word, and only the remaining 9 go into fillers?
    // Or is the acrostic meant to be read as fillers-only, meaning the 13 filler chars ARE
    // "N O R A M A R 1 9 2 0 2 5" in reading order, with NAME "NORA" merely being a separate
    // word at row 8 that happens to repeat the first 4 chars?
    //
    // Re-reading spec: "Filler letters in reading order across the full grid: N O R A M A R 1 9
    // 2 0 2 5 = NORA MAR 19 2025 (13 chars)." And it earlier said "row 9 has one extra filler
    // cell" (vs Emory) and "Nora's grid therefore has 13 filler cells total (vs Emory's 12)".
    // So the 13 fillers spell the full acrostic including NORA. Which means NAME in Nora's grid
    // is NOT a filler-cell word — NAME is a decorative word at row 8 cols 0-3 that DOES double
    // as the first four chars of the acrostic.
    //
    // Wait but then for Emory, where NAME is 5 chars "EMORY", the 12 fillers spell
    // "EMORY1062023" — again the first 5 chars ARE the NAME. So NAME chars are part of the
    // acrostic in BOTH grids.
    //
    // That means: "filler cells" includes the NAME-word cells when we compute the acrostic.
    // Or: the acrostic reads the NAME word letters PLUS the 12 (Emory) / 9 (Nora after NORA) filler
    // cells in reading order.
    //
    // Cleanest model: the acrostic read order is row-major over a KEY SET of cells that happens
    // to include the NAME letters. For the purposes of this module, I'll define "filler" as
    // "cells not covered by any TIME-TELLER word" — this includes the NAME cells.
    //
    // Re-enumerating Emory's fillers (cells not covered by time-teller or birthday words;
    // birthday words are HAPPY/BIRTH/DAY/NAME — include them? NAME-as-filler yes; HAPPY/BIRTH/DAY?
    // Those are display-only like NAME. Include them too.):
    // No — HAPPY/BIRTH/DAY are already words. They light up in birthday mode. They're not fillers.
    // But their LETTERS are part of the face. Walking row-major and collecting cells that aren't
    // covered by ANY word (time-teller or birthday) gives the true fillers.
    //
    // Emory row 0: (0,2)=E [between IT and IS], (0,5)=M [between IS and TEN] → fillers
    // Emory row 1: all QUARTER and TWENTY, no fillers
    // Emory row 2: (2,4)=O, (2,12)=R → fillers
    // Emory row 3: (3,0)=Y [before HAPPY], (3,12)=1 [after TO] → fillers
    // Emory row 4: all ONE BIRTH THREE, no fillers
    // Emory row 5: all ELEVEN FOUR DAY, no fillers
    // Emory row 6: all TWO EIGHT SEVEN, no fillers
    // Emory row 7: all NINE SIX TWELVE, no fillers
    // Emory row 8: (8,5)=0, (8,12)=6 [between NAME+AT+FIVE_HR] → fillers (NAME is word, not filler)
    // Emory row 9: all TEN OCLOCK NOON, no fillers
    // Emory row 10: (10,0)=2 [before IN] → filler
    // Emory row 11: (11,9)=0, (11,12)=2 → fillers
    // Emory row 12: (12,7)=3 → filler
    // Total: 2+2+2+2+1+2+1 = 12 fillers reading "E M O R 1 Y 0 6 2 0 2 3" — wait that's wrong
    //   ordering. Reading row-major strictly:
    //   (0,2)=E, (0,5)=M, (2,4)=O, (2,12)=R, (3,0)=Y, (3,12)=1, (8,5)=0, (8,12)=6, (10,0)=2,
    //   (11,9)=0, (11,12)=2, (12,7)=3 = "EMORY1062023"? Let me concat: E,M,O,R,Y,1,0,6,2,0,2,3 → yes
    //   that's "EMORY1062023". ✓ matches spec.
    //
    // BUT the spec's §Emory grid listing of fillers per row says:
    //   row 1 fillers: E, M   (matches)
    //   row 3 fillers: O, R   (matches)
    //   row 4 fillers: Y, 1   (matches)
    //   row 9 fillers: 0, 6   (matches)
    //   row 11 fillers: 2     (matches)
    //   row 12 fillers: 0, 2  (matches)
    //   row 13 fillers: 3     (matches)
    // Great, consistent.
    //
    // For Nora, row 8 = N O R A R 1 A T F I V E 9. NAME(NORA) at (8,0-3), AT(8,6-7), FIVE_HR(8,8-11).
    // Fillers in row 8: (8,4)=R, (8,5)=1, (8,12)=9.
    // Full Nora acrostic reading row-major: need Nora's shared-row fillers to spell the rest.
    //   Row 0: (0,2), (0,5)  → must be 'M', 'A' (chars 5,6 of "NORAMAR192025" — wait first 4 are NORA)
    //
    // Actually: acrostic "NORAMAR192025" — 13 chars. Fillers in row-major order:
    //   (0,2): char 1 of acrostic = N? No wait — NAME is NORA at (8,0-3) which is FAR later in row-
    //   major order than (0,2). So if row-major reading starts at top-left, the first filler cell
    //   is (0,2), and it must hold char 1 of the acrostic.
    //   But the acrostic STARTS with NORA. So (0,2) must be 'N'.
    //
    // Hmm but that means the filler letters in Nora's grid ARE DIFFERENT from Emory's, in every
    // shared-row position. The spec's claim "Identical to Emory's except row 9" is slightly
    // misleading — it's identical in WORD positions, but the filler letter VALUES differ because
    // the acrostics differ.
    //
    // Let me verify the Nora acrostic layout:
    //   Fillers row-major: (0,2), (0,5), (2,4), (2,12), (3,0), (3,12), (8,4), (8,5), (8,12),
    //                      (10,0), (11,9), (11,12), (12,7) = 13 cells.
    //   Acrostic "NORAMAR192025" chars 1-13:
    //     pos 1  = 'N' → (0,2)
    //     pos 2  = 'O' → (0,5)
    //     pos 3  = 'R' → (2,4)
    //     pos 4  = 'A' → (2,12)
    //     pos 5  = 'M' → (3,0)
    //     pos 6  = 'A' → (3,12)
    //     pos 7  = 'R' → (8,4)
    //     pos 8  = '1' → (8,5)
    //     pos 9  = '9' → (8,12)
    //     pos 10 = '2' → (10,0)
    //     pos 11 = '0' → (11,9)
    //     pos 12 = '2' → (11,12)
    //     pos 13 = '5' → (12,7)
    //
    // Cross-check against spec: spec's row 1 for Nora isn't shown, but it says "Identical to
    // Emory's except row 9" which contradicts the above. I think the spec is understating how
    // the grids differ in fillers, and the correct interpretation is: the WORD positions are
    // identical, the FILLER characters differ to spell each kid's acrostic.
    //
    // For this plan: we'll implement the Nora grid with the filler chars computed above. The
    // test `test_nora_filler_acrostic` already asserts that the fillers spell "NORAMAR192025"
    // in row-major order, so the implementation is forced to match.

    // Fillers in row-major order. Same positions for both kids; only letters differ.
    g.fillers[0]  = {0, 2, 1};   // row 0 col 2
    g.fillers[1]  = {0, 5, 1};   // row 0 col 5
    g.fillers[2]  = {2, 4, 1};   // row 2 col 4
    g.fillers[3]  = {2, 12, 1};  // row 2 col 12
    g.fillers[4]  = {3, 0, 1};   // row 3 col 0
    g.fillers[5]  = {3, 12, 1};  // row 3 col 12
    if (nora_row9) {
        g.fillers[6]  = {8, 4, 1};   // row 8 col 4
        g.fillers[7]  = {8, 5, 1};   // row 8 col 5
        g.fillers[8]  = {8, 12, 1};  // row 8 col 12
        g.fillers[9]  = {10, 0, 1};  // row 10 col 0
        g.fillers[10] = {11, 9, 1};  // row 11 col 9
        g.fillers[11] = {11, 12, 1}; // row 11 col 12
        g.fillers[12] = {12, 7, 1};  // row 12 col 7
        g.filler_count = 13;
    } else {
        g.fillers[6]  = {8, 5, 1};   // row 8 col 5
        g.fillers[7]  = {8, 12, 1};  // row 8 col 12
        g.fillers[8]  = {10, 0, 1};  // row 10 col 0
        g.fillers[9]  = {11, 9, 1};  // row 11 col 9
        g.fillers[10] = {11, 12, 1}; // row 11 col 12
        g.fillers[11] = {12, 7, 1};  // row 12 col 7
        g.filler_count = 12;
    }

    return g;
}

constexpr Grid EMORY_GRID = make_grid(EMORY_LETTERS, /*nora_row9=*/false);

// Build Nora's layout by starting with Emory's letters, overwriting the shared-row fillers
// with Nora's acrostic chars, then overwriting row 8 entirely.
constexpr char make_nora_letter(int idx) {
    // Acrostic "NORAMAR192025" for 13 filler positions (row-major).
    // Only the filler cells change between Emory and Nora; word cells are identical.
    switch (idx) {
        case  0 * 13 +  2: return 'N'; // (0,2)
        case  0 * 13 +  5: return 'O'; // (0,5)
        case  2 * 13 +  4: return 'R'; // (2,4)
        case  2 * 13 + 12: return 'A'; // (2,12)
        case  3 * 13 +  0: return 'M'; // (3,0)
        case  3 * 13 + 12: return 'A'; // (3,12)
        case 10 * 13 +  0: return '2'; // (10,0)
        case 11 * 13 +  9: return '0'; // (11,9)
        case 11 * 13 + 12: return '2'; // (11,12)
        case 12 * 13 +  7: return '5'; // (12,7)
        default: return EMORY_LETTERS[idx];
    }
}

constexpr Grid make_nora_grid() {
    Grid g = make_grid(EMORY_LETTERS, /*nora_row9=*/true);
    // Overwrite shared-row fillers with Nora's acrostic chars (row 8 already overwritten).
    for (int i = 0; i < 13 * 13; ++i) g.letters[i] = make_nora_letter(i);
    // Re-apply row 8 Nora values (make_nora_letter defaulted to Emory for row 8).
    for (int i = 0; i < 13; ++i) g.letters[8 * 13 + i] = NORA_ROW_9[i];
    return g;
}

constexpr Grid NORA_GRID = make_nora_grid();

} // namespace

const Grid& get(ClockTarget target) {
    return target == ClockTarget::EMORY ? EMORY_GRID : NORA_GRID;
}

} // namespace wc
```

- [ ] **Step 7: Run tests until green**

```bash
cd firmware
pio test -e native 2>&1 | tail -25
```

Expected: all 7 tests pass. If any fail, read the assertion and fix `grid.cpp`.

Common failure modes:
- `test_every_word_has_a_span` fails → missing `set(WordId::X, …)` call in `make_grid`. Check which WordId (print its index and fix.
- `test_nora_filler_acrostic` fails → `make_nora_letter` is wrong for one position. Dump the grid with a debug print (`printf("%c", g.letters[i])`) and compare visually.
- `constexpr` function errors in newer toolchains → remove `constexpr` from `make_grid` and `make_nora_grid` (make them `static` regular functions, call once at init).

- [ ] **Step 8: Commit**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git add firmware/lib/core/include/word_id.h firmware/lib/core/include/grid.h \
        firmware/lib/core/src/grid.cpp firmware/test/test_grid/ \
        firmware/test/test_placeholder
git commit -m "feat(firmware): grid module with emory and nora layouts"
```

(The deleted `test_placeholder` directory is staged as a deletion.)

---

## Task 2: `time_to_words` module

**Files:**
- Create: `firmware/lib/core/include/time_to_words.h`
- Create: `firmware/lib/core/src/time_to_words.cpp`
- Create: `firmware/test/test_time_to_words/test_time_to_words.cpp`

- [ ] **Step 1: Create the header**

```cpp
// firmware/lib/core/include/time_to_words.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc {

struct WordSet {
    WordId words[8];
    uint8_t count;
};

// Computes the words to light for a given 24-hour clock time.
// hour24: 0-23. minute: 0-59. Minute is internally floored to the 5-minute block.
WordSet time_to_words(uint8_t hour24, uint8_t minute);

} // namespace wc
```

- [ ] **Step 2: Create the failing test**

```cpp
// firmware/test/test_time_to_words/test_time_to_words.cpp
#include <unity.h>
#include "time_to_words.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static bool contains(const WordSet& ws, WordId id) {
    for (uint8_t i = 0; i < ws.count; ++i) if (ws.words[i] == id) return true;
    return false;
}

// 09:00 → "IT IS NINE OCLOCK IN THE MORNING"
void test_nine_oclock_morning(void) {
    WordSet ws = time_to_words(9, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::NINE));
    TEST_ASSERT_TRUE(contains(ws, WordId::OCLOCK));
    TEST_ASSERT_TRUE(contains(ws, WordId::IN));
    TEST_ASSERT_TRUE(contains(ws, WordId::THE));
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
}

// 12:00 → "IT IS TWELVE NOON" (no OCLOCK, no AT/IN/THE)
void test_twelve_noon(void) {
    WordSet ws = time_to_words(12, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::NOON));
    TEST_ASSERT_FALSE(contains(ws, WordId::OCLOCK));
    TEST_ASSERT_FALSE(contains(ws, WordId::AT));
    TEST_ASSERT_FALSE(contains(ws, WordId::IN));
    TEST_ASSERT_FALSE(contains(ws, WordId::THE));
}

// 00:00 → "IT IS TWELVE AT NIGHT" (midnight, no OCLOCK)
void test_midnight(void) {
    WordSet ws = time_to_words(0, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_FALSE(contains(ws, WordId::OCLOCK));
}

// 14:30 → "IT IS HALF PAST TWO IN THE AFTERNOON"
void test_half_past_two_afternoon(void) {
    WordSet ws = time_to_words(14, 30);
    TEST_ASSERT_TRUE(contains(ws, WordId::HALF));
    TEST_ASSERT_TRUE(contains(ws, WordId::PAST));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWO));
    TEST_ASSERT_TRUE(contains(ws, WordId::IN));
    TEST_ASSERT_TRUE(contains(ws, WordId::THE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AFTERNOON));
}

// 23:45 → "IT IS QUARTER TO TWELVE AT NIGHT" (next hour wraps from 11→12)
void test_quarter_to_midnight(void) {
    WordSet ws = time_to_words(23, 45);
    TEST_ASSERT_TRUE(contains(ws, WordId::QUARTER));
    TEST_ASSERT_TRUE(contains(ws, WordId::TO));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
}

// 06:25 → "IT IS TWENTY FIVE MINUTES PAST SIX IN THE MORNING"
void test_twenty_five_past_six_morning(void) {
    WordSet ws = time_to_words(6, 25);
    TEST_ASSERT_TRUE(contains(ws, WordId::TWENTY));
    TEST_ASSERT_TRUE(contains(ws, WordId::FIVE_MIN));
    TEST_ASSERT_TRUE(contains(ws, WordId::MINUTES));
    TEST_ASSERT_TRUE(contains(ws, WordId::PAST));
    TEST_ASSERT_TRUE(contains(ws, WordId::SIX));
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
}

// Minute flooring: 14:02 behaves identically to 14:00
void test_minute_flooring(void) {
    WordSet a = time_to_words(14, 0);
    WordSet b = time_to_words(14, 2);
    WordSet c = time_to_words(14, 4);
    TEST_ASSERT_EQUAL(a.count, b.count);
    TEST_ASSERT_EQUAL(a.count, c.count);
    for (uint8_t i = 0; i < a.count; ++i) {
        TEST_ASSERT_EQUAL(a.words[i], b.words[i]);
        TEST_ASSERT_EQUAL(a.words[i], c.words[i]);
    }
}

// 17:00 boundary — EVENING, not AFTERNOON
void test_five_pm_is_evening(void) {
    WordSet ws = time_to_words(17, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::EVENING));
    TEST_ASSERT_FALSE(contains(ws, WordId::AFTERNOON));
}

// 21:00 boundary — NIGHT, not EVENING
void test_nine_pm_is_night(void) {
    WordSet ws = time_to_words(21, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_FALSE(contains(ws, WordId::EVENING));
}

// 05:00 boundary — MORNING starts here
void test_five_am_is_morning(void) {
    WordSet ws = time_to_words(5, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
    TEST_ASSERT_FALSE(contains(ws, WordId::NIGHT));
}

// 04:59 is still NIGHT (floors to 04:55)
void test_late_night_edge(void) {
    WordSet ws = time_to_words(4, 59);
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_FALSE(contains(ws, WordId::MORNING));
}

// count never exceeds 8
void test_wordset_bounds(void) {
    for (uint8_t h = 0; h < 24; ++h) {
        for (uint8_t m = 0; m < 60; ++m) {
            WordSet ws = time_to_words(h, m);
            TEST_ASSERT_LESS_OR_EQUAL(8, ws.count);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nine_oclock_morning);
    RUN_TEST(test_twelve_noon);
    RUN_TEST(test_midnight);
    RUN_TEST(test_half_past_two_afternoon);
    RUN_TEST(test_quarter_to_midnight);
    RUN_TEST(test_twenty_five_past_six_morning);
    RUN_TEST(test_minute_flooring);
    RUN_TEST(test_five_pm_is_evening);
    RUN_TEST(test_nine_pm_is_night);
    RUN_TEST(test_five_am_is_morning);
    RUN_TEST(test_late_night_edge);
    RUN_TEST(test_wordset_bounds);
    return UNITY_END();
}
```

- [ ] **Step 3: Run, verify fail**

```bash
pio test -e native --filter 'test_time_to_words/*' 2>&1 | tail -10
```

Expected: link error (no `time_to_words.cpp` yet).

- [ ] **Step 4: Implement**

```cpp
// firmware/lib/core/src/time_to_words.cpp
#include "time_to_words.h"

namespace wc {

namespace {

WordId hour_word(uint8_t h12) {
    switch (h12) {
        case 1:  return WordId::ONE;
        case 2:  return WordId::TWO;
        case 3:  return WordId::THREE;
        case 4:  return WordId::FOUR;
        case 5:  return WordId::FIVE_HR;
        case 6:  return WordId::SIX;
        case 7:  return WordId::SEVEN;
        case 8:  return WordId::EIGHT;
        case 9:  return WordId::NINE;
        case 10: return WordId::TEN_HR;
        case 11: return WordId::ELEVEN;
        case 12:
        default: return WordId::TWELVE;
    }
}

uint8_t to_12h(uint8_t h24) {
    uint8_t h = h24 % 12;
    return h == 0 ? 12 : h;
}

enum class TimeOfDay { MORNING, AFTERNOON, EVENING, NIGHT, NOON_EXACT, MIDNIGHT_EXACT };

TimeOfDay classify(uint8_t h24, uint8_t m5) {
    if (h24 == 12 && m5 == 0) return TimeOfDay::NOON_EXACT;
    if (h24 == 0  && m5 == 0) return TimeOfDay::MIDNIGHT_EXACT;
    if (h24 >= 5  && h24 < 12) return TimeOfDay::MORNING;
    if (h24 >= 12 && h24 < 17) return TimeOfDay::AFTERNOON;
    if (h24 >= 17 && h24 < 21) return TimeOfDay::EVENING;
    return TimeOfDay::NIGHT; // 21-23 and 0-4
}

void push(WordSet& ws, WordId w) {
    ws.words[ws.count++] = w;
}

void push_suffix(WordSet& ws, TimeOfDay tod) {
    switch (tod) {
        case TimeOfDay::MORNING:
            push(ws, WordId::IN); push(ws, WordId::THE); push(ws, WordId::MORNING); break;
        case TimeOfDay::AFTERNOON:
            push(ws, WordId::IN); push(ws, WordId::THE); push(ws, WordId::AFTERNOON); break;
        case TimeOfDay::EVENING:
            push(ws, WordId::IN); push(ws, WordId::THE); push(ws, WordId::EVENING); break;
        case TimeOfDay::NIGHT:
            push(ws, WordId::AT); push(ws, WordId::NIGHT); break;
        case TimeOfDay::NOON_EXACT:
        case TimeOfDay::MIDNIGHT_EXACT:
            break; // handled by caller
    }
}

} // namespace

WordSet time_to_words(uint8_t hour24, uint8_t minute) {
    WordSet ws{};
    uint8_t m5 = minute - (minute % 5); // floor to 5-min block
    uint8_t block = m5 / 5;              // 0..11

    push(ws, WordId::IT);
    push(ws, WordId::IS);

    TimeOfDay tod = classify(hour24, m5);

    // Special cases: exactly noon / exactly midnight — no OCLOCK, custom suffix.
    if (tod == TimeOfDay::NOON_EXACT) {
        push(ws, WordId::TWELVE);
        push(ws, WordId::NOON);
        return ws;
    }
    if (tod == TimeOfDay::MIDNIGHT_EXACT) {
        push(ws, WordId::TWELVE);
        push(ws, WordId::AT);
        push(ws, WordId::NIGHT);
        return ws;
    }

    uint8_t h12 = to_12h(hour24);
    uint8_t next12 = to_12h(hour24 + 1);

    // For "TO" phrases (blocks 7-11), the announced hour is the NEXT hour.
    // Also the time-of-day classification must be for the NEXT hour's
    // context when crossing a TOD boundary (e.g., 11:45 PM → "quarter to
    // twelve AT NIGHT" because the next hour is midnight).
    if (block >= 7) {
        // classify at the rolled-over time
        uint8_t next_h = (hour24 + 1) % 24;
        tod = classify(next_h, 0);
    }

    switch (block) {
        case 0: // :00 — [hour] OCLOCK
            push(ws, hour_word(h12)); push(ws, WordId::OCLOCK); break;
        case 1: // :05 — FIVE MINUTES PAST [hour]
            push(ws, WordId::FIVE_MIN); push(ws, WordId::MINUTES); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 2: // :10 — TEN MINUTES PAST [hour]
            push(ws, WordId::TEN_MIN); push(ws, WordId::MINUTES); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 3: // :15 — QUARTER PAST [hour]
            push(ws, WordId::QUARTER); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 4: // :20 — TWENTY MINUTES PAST [hour]
            push(ws, WordId::TWENTY); push(ws, WordId::MINUTES); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 5: // :25 — TWENTY FIVE MINUTES PAST [hour]
            push(ws, WordId::TWENTY); push(ws, WordId::FIVE_MIN); push(ws, WordId::MINUTES); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 6: // :30 — HALF PAST [hour]
            push(ws, WordId::HALF); push(ws, WordId::PAST); push(ws, hour_word(h12)); break;
        case 7: // :35 — TWENTY FIVE MINUTES TO [next]
            push(ws, WordId::TWENTY); push(ws, WordId::FIVE_MIN); push(ws, WordId::MINUTES); push(ws, WordId::TO); push(ws, hour_word(next12)); break;
        case 8: // :40 — TWENTY MINUTES TO [next]
            push(ws, WordId::TWENTY); push(ws, WordId::MINUTES); push(ws, WordId::TO); push(ws, hour_word(next12)); break;
        case 9: // :45 — QUARTER TO [next]
            push(ws, WordId::QUARTER); push(ws, WordId::TO); push(ws, hour_word(next12)); break;
        case 10: // :50 — TEN MINUTES TO [next]
            push(ws, WordId::TEN_MIN); push(ws, WordId::MINUTES); push(ws, WordId::TO); push(ws, hour_word(next12)); break;
        case 11: // :55 — FIVE MINUTES TO [next]
            push(ws, WordId::FIVE_MIN); push(ws, WordId::MINUTES); push(ws, WordId::TO); push(ws, hour_word(next12)); break;
    }

    // TOD suffix — NOON and MIDNIGHT already returned above.
    // But if the TO-phrase points at exactly noon/midnight (e.g., 11:35 AM→"twenty five to twelve")
    // we treat the suffix as NOON/MIDNIGHT → collapse to just "AT NIGHT" or… hmm, spec doesn't
    // spell this out. Keep it simple: use the classify() result (MORNING/AFTERNOON/EVENING/NIGHT)
    // which `classify(next_h, 0)` returns MORNING at 12 PM? No — at hour==12 min==0 it's NOON_EXACT.
    // Handle that by falling through to AFTERNOON when the NEXT hour is exactly noon
    // and the block is 7-11 (TO phrase). Easier: remap NOON_EXACT→AFTERNOON, MIDNIGHT_EXACT→NIGHT
    // in this context.
    if (tod == TimeOfDay::NOON_EXACT) tod = TimeOfDay::AFTERNOON;
    if (tod == TimeOfDay::MIDNIGHT_EXACT) tod = TimeOfDay::NIGHT;
    push_suffix(ws, tod);

    return ws;
}

} // namespace wc
```

- [ ] **Step 5: Run tests green**

```bash
pio test -e native --filter 'test_time_to_words/*' 2>&1 | tail -20
```

Expected: all 12 tests pass. If `test_wordset_bounds` fails, one of the switch cases is over-pushing — count the pushes for the failing (h, m) and trim.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/core/include/time_to_words.h firmware/lib/core/src/time_to_words.cpp firmware/test/test_time_to_words/
git commit -m "feat(firmware): time_to_words with morning/afternoon/evening/night/noon"
```

---

## Task 3: Golden-file round-trip test for `time_to_words`

**Files:**
- Create: `firmware/test/test_time_to_words_golden/test_time_to_words_golden.cpp`
- Create: `firmware/test/test_time_to_words_golden/emory_golden.txt`
- Create: `firmware/test/test_time_to_words_golden/nora_golden.txt`

**Context:** A regression net. Any change to `time_to_words` that alters output for any of 1440 minutes must be reviewed as an intentional change to the golden file. For Phase 1, Emory and Nora produce identical output (grid words don't change between kids for time-telling) — the two golden files are therefore identical. They're duplicated anyway so Phase 2 can diverge them if needed (e.g., kid-specific color sequences hypothetically piping through here).

- [ ] **Step 1: Write the test scaffolding**

```cpp
// firmware/test/test_time_to_words_golden/test_time_to_words_golden.cpp
#include <unity.h>
#include <cstdio>
#include <cstring>
#include "time_to_words.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static const char* word_name(WordId w) {
    switch (w) {
        case WordId::IT: return "IT";
        case WordId::IS: return "IS";
        case WordId::TEN_MIN: return "TEN";
        case WordId::HALF: return "HALF";
        case WordId::QUARTER: return "QUARTER";
        case WordId::TWENTY: return "TWENTY";
        case WordId::FIVE_MIN: return "FIVE_MIN";
        case WordId::MINUTES: return "MINUTES";
        case WordId::PAST: return "PAST";
        case WordId::TO: return "TO";
        case WordId::ONE: return "ONE";
        case WordId::TWO: return "TWO";
        case WordId::THREE: return "THREE";
        case WordId::FOUR: return "FOUR";
        case WordId::FIVE_HR: return "FIVE_HR";
        case WordId::SIX: return "SIX";
        case WordId::SEVEN: return "SEVEN";
        case WordId::EIGHT: return "EIGHT";
        case WordId::NINE: return "NINE";
        case WordId::TEN_HR: return "TEN_HR";
        case WordId::ELEVEN: return "ELEVEN";
        case WordId::TWELVE: return "TWELVE";
        case WordId::OCLOCK: return "OCLOCK";
        case WordId::NOON: return "NOON";
        case WordId::IN: return "IN";
        case WordId::THE: return "THE";
        case WordId::AT: return "AT";
        case WordId::MORNING: return "MORNING";
        case WordId::AFTERNOON: return "AFTERNOON";
        case WordId::EVENING: return "EVENING";
        case WordId::NIGHT: return "NIGHT";
        case WordId::HAPPY: return "HAPPY";
        case WordId::BIRTH: return "BIRTH";
        case WordId::DAY: return "DAY";
        case WordId::NAME: return "NAME";
        case WordId::COUNT: return "?";
    }
    return "?";
}

// Writes a one-line representation of the WordSet at (h, m) to buf.
// Format: "HH:MM IT IS WORD WORD ..."
static void format_line(uint8_t h, uint8_t m, const WordSet& ws, char* buf, size_t buflen) {
    size_t n = snprintf(buf, buflen, "%02u:%02u", h, m);
    for (uint8_t i = 0; i < ws.count; ++i) {
        n += snprintf(buf + n, buflen - n, " %s", word_name(ws.words[i]));
        if (n >= buflen - 1) break;
    }
}

// Reads the golden file and compares line-by-line against generated output.
// If a line mismatches, fail with helpful diagnostic.
// Golden file path is built relative to __FILE__ so it works under PlatformIO's runner.
static void run_golden_check(const char* golden_rel) {
    // __FILE__ is .../firmware/test/test_time_to_words_golden/test_time_to_words_golden.cpp
    char path[1024];
    const char* slash = strrchr(__FILE__, '/');
    size_t dir_len = slash ? (size_t)(slash - __FILE__ + 1) : 0;
    snprintf(path, sizeof(path), "%.*s%s", (int)dir_len, __FILE__, golden_rel);

    FILE* f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    char line[512];
    for (int mins = 0; mins < 1440; ++mins) {
        uint8_t h = mins / 60;
        uint8_t m = mins % 60;
        WordSet ws = time_to_words(h, m);
        char actual[512];
        format_line(h, m, ws, actual, sizeof(actual));

        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            char msg[128];
            snprintf(msg, sizeof(msg), "golden file truncated at line %d", mins + 1);
            TEST_FAIL_MESSAGE(msg);
        }
        // Strip trailing newline.
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;

        if (strcmp(actual, line) != 0) {
            fclose(f);
            char msg[1200];
            snprintf(msg, sizeof(msg), "mismatch at line %d:\n  expected: %s\n  actual:   %s",
                     mins + 1, line, actual);
            TEST_FAIL_MESSAGE(msg);
        }
    }
    fclose(f);
}

void test_emory_golden(void) { run_golden_check("emory_golden.txt"); }
void test_nora_golden(void)  { run_golden_check("nora_golden.txt"); }

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_emory_golden);
    RUN_TEST(test_nora_golden);
    return UNITY_END();
}
```

- [ ] **Step 2: Generate the initial golden files using the current implementation**

Run from the repo root:

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
cat > /tmp/gen_golden.cpp <<'EOF'
#include <cstdio>
#include "time_to_words.h"
using namespace wc;
static const char* word_name(WordId w); // forward — define below if you want to compile standalone
int main() {
    // Copy word_name implementation here when running standalone; OR
    // link against the test binary. Easier: link the test binary and add a --generate-golden flag.
    return 0;
}
EOF
```

Actually, simpler: use a throwaway Python script that re-implements the word names in Python and calls the C++ binary indirectly — or just run the test once with `EXPECTED_TO_FAIL` mode and capture `actual:` lines from the failure messages.

**Easiest path:** Add a `--gen` flag to the test's `main` that writes the golden files instead of reading them, then run it manually one time.

Replace the `main` block in the test file temporarily:

```cpp
// Temporary generator — revert after producing golden files.
int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--gen") == 0) {
        char path[1024];
        const char* slash = strrchr(__FILE__, '/');
        size_t dir_len = slash ? (size_t)(slash - __FILE__ + 1) : 0;
        snprintf(path, sizeof(path), "%.*semory_golden.txt", (int)dir_len, __FILE__);
        FILE* f = fopen(path, "w");
        for (int mins = 0; mins < 1440; ++mins) {
            uint8_t h = mins / 60, m = mins % 60;
            WordSet ws = time_to_words(h, m);
            char buf[512];
            format_line(h, m, ws, buf, sizeof(buf));
            fprintf(f, "%s\n", buf);
        }
        fclose(f);
        // Nora is identical for now.
        snprintf(path, sizeof(path), "%.*snora_golden.txt", (int)dir_len, __FILE__);
        f = fopen(path, "w");
        for (int mins = 0; mins < 1440; ++mins) {
            uint8_t h = mins / 60, m = mins % 60;
            WordSet ws = time_to_words(h, m);
            char buf[512];
            format_line(h, m, ws, buf, sizeof(buf));
            fprintf(f, "%s\n", buf);
        }
        fclose(f);
        return 0;
    }
    UNITY_BEGIN();
    RUN_TEST(test_emory_golden);
    RUN_TEST(test_nora_golden);
    return UNITY_END();
}
```

Build + run in generator mode:

```bash
cd firmware
pio test -e native --filter 'test_time_to_words_golden/*' --without-testing
# Find the built binary:
BIN=$(find .pio/build/native -name 'program' -type f | head -1)
"$BIN" --gen
ls test/test_time_to_words_golden/
```

Expected: `emory_golden.txt` and `nora_golden.txt` created, each 1440 lines. Inspect a few lines by eye (00:00, 12:00, 23:55) to confirm they look correct.

If `--without-testing` is not a valid pio flag, use `pio test -e native --filter '...' --verbose` and find the program binary under `.pio/build/native/test/...` — any built test binary will have the `--gen` handler.

Then revert the test's `main` back to the regular test runner (remove the `--gen` block). The goldens are now a locked snapshot.

- [ ] **Step 3: Run golden test and verify pass**

```bash
cd firmware
pio test -e native --filter 'test_time_to_words_golden/*' 2>&1 | tail -10
```

Expected: 2 tests pass.

- [ ] **Step 4: Sanity-check specific lines**

```bash
head -1 firmware/test/test_time_to_words_golden/emory_golden.txt
# Expected: "00:00 IT IS TWELVE AT NIGHT"
sed -n '721p' firmware/test/test_time_to_words_golden/emory_golden.txt
# Line 721 = minute 720 = 12:00. Expected: "12:00 IT IS TWELVE NOON"
sed -n '1081p' firmware/test/test_time_to_words_golden/emory_golden.txt
# Line 1081 = minute 1080 = 18:00. Expected: "18:00 IT IS SIX OCLOCK IN THE EVENING"
```

- [ ] **Step 5: Commit**

```bash
git add firmware/test/test_time_to_words_golden/
git commit -m "test(firmware): 1440-minute golden round-trip for time_to_words"
```

---

## Task 4: `holidays` module

**Files:**
- Create: `firmware/lib/core/include/holidays.h`
- Create: `firmware/lib/core/src/holidays.cpp`
- Create: `firmware/test/test_holidays/test_holidays.cpp`

- [ ] **Step 1: Create the header**

```cpp
// firmware/lib/core/include/holidays.h
#pragma once

#include <cstdint>

namespace wc {

enum class Palette : uint8_t {
    WARM_WHITE,
    MLK_PURPLE,
    VALENTINES,
    WOMEN_PURPLE,
    EARTH_DAY,
    EASTER_PASTEL,
    JUNETEENTH,
    INDIGENOUS,
    HALLOWEEN,
    CHRISTMAS,
};

// Returns the palette for a given date. Defaults to WARM_WHITE.
// year: full year (e.g., 2030). month: 1-12. day: 1-31.
Palette palette_for_date(uint16_t year, uint8_t month, uint8_t day);

} // namespace wc
```

- [ ] **Step 2: Write the failing tests**

```cpp
// firmware/test/test_holidays/test_holidays.cpp
#include <unity.h>
#include "holidays.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

void test_default_is_warm_white(void) {
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 4, 15));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 7, 4));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 11, 1));
}

void test_fixed_dates(void) {
    TEST_ASSERT_EQUAL(Palette::VALENTINES,  palette_for_date(2030, 2, 14));
    TEST_ASSERT_EQUAL(Palette::WOMEN_PURPLE, palette_for_date(2030, 3, 8));
    TEST_ASSERT_EQUAL(Palette::EARTH_DAY,   palette_for_date(2030, 4, 22));
    TEST_ASSERT_EQUAL(Palette::JUNETEENTH,  palette_for_date(2030, 6, 19));
    TEST_ASSERT_EQUAL(Palette::HALLOWEEN,   palette_for_date(2030, 10, 31));
    TEST_ASSERT_EQUAL(Palette::CHRISTMAS,   palette_for_date(2030, 12, 25));
}

void test_mlk_day_third_monday_january(void) {
    // Known: 2030-01-21 = 3rd Monday. 2031-01-20. 2032-01-19. 2033-01-17. 2034-01-16.
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2030, 1, 21));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2031, 1, 20));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2032, 1, 19));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 1, 14)); // 2nd Mon
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 1, 28)); // 4th Mon
}

void test_indigenous_peoples_day_second_monday_october(void) {
    // 2030-10-14, 2031-10-13, 2032-10-11.
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2030, 10, 14));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2031, 10, 13));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 10, 7));  // 1st Mon
}

void test_easter_computed(void) {
    // Western Easter dates (Gregorian): 2030-04-21, 2031-04-13, 2032-03-28, 2033-04-17,
    // 2034-04-09, 2035-03-25. Source: US Naval Observatory.
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2030, 4, 21));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2031, 4, 13));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2032, 3, 28));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2033, 4, 17));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2034, 4, 9));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2035, 3, 25));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 4, 20));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 4, 22)); // Earth Day wins priority?
    // Earth Day (Apr 22) and Easter (never on Apr 22 in this century) don't collide.
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_warm_white);
    RUN_TEST(test_fixed_dates);
    RUN_TEST(test_mlk_day_third_monday_january);
    RUN_TEST(test_indigenous_peoples_day_second_monday_october);
    RUN_TEST(test_easter_computed);
    return UNITY_END();
}
```

- [ ] **Step 3: Run — expect link failure**

```bash
pio test -e native --filter 'test_holidays/*' 2>&1 | tail -5
```

- [ ] **Step 4: Implement `holidays.cpp`**

```cpp
// firmware/lib/core/src/holidays.cpp
#include "holidays.h"

namespace wc {

namespace {

// Zeller's congruence for Gregorian calendar. Returns day of week where 0=Sunday..6=Saturday.
int day_of_week(uint16_t y, uint8_t m, uint8_t d) {
    int month = m, year = y;
    if (month < 3) { month += 12; year -= 1; }
    int K = year % 100;
    int J = year / 100;
    int h = (d + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    // Zeller: 0=Saturday..6=Friday. Remap to 0=Sunday..6=Saturday.
    return (h + 6) % 7;
}

// Nth occurrence of dow in (year, month). dow: 0=Sun..6=Sat. n: 1-based.
// Returns day-of-month (1..31) for nth match.
uint8_t nth_weekday_of_month(uint16_t year, uint8_t month, int dow, int n) {
    int first_dow = day_of_week(year, month, 1);
    int offset = (dow - first_dow + 7) % 7;
    return (uint8_t)(1 + offset + (n - 1) * 7);
}

// Meeus/Jones/Butcher algorithm — computes Gregorian Easter.
// Returns month (3 or 4) and day via out params.
void gregorian_easter(uint16_t year, uint8_t& month, uint8_t& day) {
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int L = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * L) / 451;
    int month_i = (h + L - 7 * m + 114) / 31;
    int day_i = ((h + L - 7 * m + 114) % 31) + 1;
    month = (uint8_t)month_i;
    day = (uint8_t)day_i;
}

} // namespace

Palette palette_for_date(uint16_t year, uint8_t month, uint8_t day) {
    // Fixed-date holidays first (highest priority among fixed).
    if (month == 2  && day == 14) return Palette::VALENTINES;
    if (month == 3  && day == 8)  return Palette::WOMEN_PURPLE;
    if (month == 4  && day == 22) return Palette::EARTH_DAY;
    if (month == 6  && day == 19) return Palette::JUNETEENTH;
    if (month == 10 && day == 31) return Palette::HALLOWEEN;
    if (month == 12 && day == 25) return Palette::CHRISTMAS;

    // MLK Day: 3rd Monday of January.
    if (month == 1) {
        uint8_t mlk = nth_weekday_of_month(year, 1, 1 /*Mon*/, 3);
        if (day == mlk) return Palette::MLK_PURPLE;
    }

    // Indigenous Peoples' Day: 2nd Monday of October.
    if (month == 10) {
        uint8_t ipd = nth_weekday_of_month(year, 10, 1 /*Mon*/, 2);
        if (day == ipd) return Palette::INDIGENOUS;
    }

    // Easter: computed.
    uint8_t em, ed;
    gregorian_easter(year, em, ed);
    if (month == em && day == ed) return Palette::EASTER_PASTEL;

    return Palette::WARM_WHITE;
}

} // namespace wc
```

- [ ] **Step 5: Run tests, iterate until green**

```bash
pio test -e native --filter 'test_holidays/*' 2>&1 | tail -15
```

Expected: 5 tests pass. If `test_mlk_day_third_monday_january` fails, check `day_of_week` output for Jan 1 of each test year manually (`cal -j 2030 01` in terminal) and compare.

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/core/include/holidays.h firmware/lib/core/src/holidays.cpp firmware/test/test_holidays/
git commit -m "feat(firmware): holiday palette lookup with easter + nth-weekday computation"
```

---

## Task 5: `birthday` module

**Files:**
- Create: `firmware/lib/core/include/birthday.h`
- Create: `firmware/lib/core/src/birthday.cpp`
- Create: `firmware/test/test_birthday/test_birthday.cpp`

- [ ] **Step 1: Header**

```cpp
// firmware/lib/core/include/birthday.h
#pragma once

#include <cstdint>

namespace wc {

struct BirthdayConfig {
    uint8_t month;   // 1-12
    uint8_t day;     // 1-31
    uint8_t hour;    // 0-23 (birth minute)
    uint8_t minute;  // 0-59
};

struct BirthdayState {
    bool is_birthday;       // today is the kid's birthday (any year, same month+day)
    bool is_birth_minute;   // ALSO currently at the exact birth-minute
};

// now_month/day/hour/minute describe current local time.
BirthdayState birthday_state(uint8_t now_month, uint8_t now_day,
                              uint8_t now_hour, uint8_t now_minute,
                              const BirthdayConfig& cfg);

} // namespace wc
```

- [ ] **Step 2: Test**

```cpp
// firmware/test/test_birthday/test_birthday.cpp
#include <unity.h>
#include "birthday.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static constexpr BirthdayConfig EMORY = {10, 6, 18, 10};  // Oct 6, 6:10 PM
static constexpr BirthdayConfig NORA  = {3, 19, 9, 17};   // Mar 19, 9:17 AM

void test_not_birthday(void) {
    BirthdayState s = birthday_state(4, 15, 12, 0, EMORY);
    TEST_ASSERT_FALSE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

void test_is_birthday_all_day(void) {
    BirthdayState s1 = birthday_state(10, 6, 0,  0,  EMORY);
    BirthdayState s2 = birthday_state(10, 6, 12, 0,  EMORY);
    BirthdayState s3 = birthday_state(10, 6, 23, 59, EMORY);
    TEST_ASSERT_TRUE(s1.is_birthday);
    TEST_ASSERT_TRUE(s2.is_birthday);
    TEST_ASSERT_TRUE(s3.is_birthday);
}

void test_birth_minute_exact(void) {
    BirthdayState s = birthday_state(10, 6, 18, 10, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_TRUE(s.is_birth_minute);
}

void test_birth_minute_wrong_minute(void) {
    BirthdayState s = birthday_state(10, 6, 18, 11, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

void test_birth_minute_wrong_hour(void) {
    BirthdayState s = birthday_state(10, 6, 19, 10, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

void test_nora_separate(void) {
    BirthdayState s = birthday_state(3, 19, 9, 17, NORA);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_TRUE(s.is_birth_minute);

    s = birthday_state(3, 19, 9, 17, EMORY);
    TEST_ASSERT_FALSE(s.is_birthday); // Emory's birthday is not Mar 19
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_not_birthday);
    RUN_TEST(test_is_birthday_all_day);
    RUN_TEST(test_birth_minute_exact);
    RUN_TEST(test_birth_minute_wrong_minute);
    RUN_TEST(test_birth_minute_wrong_hour);
    RUN_TEST(test_nora_separate);
    return UNITY_END();
}
```

- [ ] **Step 3: Run — expect fail**

```bash
pio test -e native --filter 'test_birthday/*' 2>&1 | tail -5
```

- [ ] **Step 4: Implement**

```cpp
// firmware/lib/core/src/birthday.cpp
#include "birthday.h"

namespace wc {

BirthdayState birthday_state(uint8_t now_month, uint8_t now_day,
                              uint8_t now_hour, uint8_t now_minute,
                              const BirthdayConfig& cfg) {
    BirthdayState s{};
    s.is_birthday = (now_month == cfg.month) && (now_day == cfg.day);
    s.is_birth_minute = s.is_birthday
                     && (now_hour == cfg.hour)
                     && (now_minute == cfg.minute);
    return s;
}

} // namespace wc
```

- [ ] **Step 5: Run tests green**

```bash
pio test -e native --filter 'test_birthday/*' 2>&1 | tail -10
```

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/core/include/birthday.h firmware/lib/core/src/birthday.cpp firmware/test/test_birthday/
git commit -m "feat(firmware): birthday and birth-minute detection"
```

---

## Task 6: `dim_schedule` module

**Files:**
- Create: `firmware/lib/core/include/dim_schedule.h`
- Create: `firmware/lib/core/src/dim_schedule.cpp`
- Create: `firmware/test/test_dim_schedule/test_dim_schedule.cpp`

- [ ] **Step 1: Header**

```cpp
// firmware/lib/core/include/dim_schedule.h
#pragma once

#include <cstdint>

namespace wc {

// Returns brightness multiplier [0.0, 1.0] for the given 24-hour local time.
// Spec: between 19:00 (7 PM) and 08:00 (8 AM) the clock runs at 10%. Full bright otherwise.
// Boundaries: 19:00:00 is dim; 07:59:59 is dim; 08:00:00 is full.
float brightness(uint8_t hour24, uint8_t minute);

} // namespace wc
```

- [ ] **Step 2: Test**

```cpp
// firmware/test/test_dim_schedule/test_dim_schedule.cpp
#include <unity.h>
#include "dim_schedule.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

void test_full_bright_midday(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(12, 0));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(8, 0));   // 8 AM is full
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(18, 59)); // 6:59 PM is full
}

void test_dim_at_night(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(19, 0));  // 7 PM — first dim minute
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(22, 30));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(0,  0));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(7, 59));  // 7:59 AM — last dim minute
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_bright_midday);
    RUN_TEST(test_dim_at_night);
    return UNITY_END();
}
```

- [ ] **Step 3: Run — expect fail**

```bash
pio test -e native --filter 'test_dim_schedule/*' 2>&1 | tail -5
```

- [ ] **Step 4: Implement**

```cpp
// firmware/lib/core/src/dim_schedule.cpp
#include "dim_schedule.h"

namespace wc {

float brightness(uint8_t hour24, uint8_t /*minute*/) {
    // Dim window: [19:00, 08:00). I.e., hour >= 19 OR hour < 8.
    if (hour24 >= 19 || hour24 < 8) return 0.1f;
    return 1.0f;
}

} // namespace wc
```

- [ ] **Step 5: Run green**

```bash
pio test -e native --filter 'test_dim_schedule/*' 2>&1 | tail -10
```

- [ ] **Step 6: Commit**

```bash
git add firmware/lib/core/include/dim_schedule.h firmware/lib/core/src/dim_schedule.cpp firmware/test/test_dim_schedule/
git commit -m "feat(firmware): bedtime dim schedule 19:00-08:00"
```

---

## Task 7: Phase 1 wrap-up — full suite, binary-size check, ESP32 env still builds

- [ ] **Step 1: Run the full native suite**

```bash
cd firmware
pio test -e native 2>&1 | tail -15
```

Expected: all tests pass (7 + 12 + 2 + 5 + 6 + 2 = 34 tests). Total duration should be under 10 seconds per the phase exit criteria.

- [ ] **Step 2: Confirm ESP32 envs still compile with the new `lib/core/` files**

The ESP32 envs don't include the test files, but they do compile `lib/core/src/*.cpp`. If any symbol leaked a native-only dependency (stdio, etc.), the Arduino build will complain.

```bash
pio run -e emory 2>&1 | tail -6
pio run -e nora 2>&1 | tail -6
```

Expected: both succeed. If compilation breaks with an unresolved symbol from `<cstdio>` or similar, trace it back — `lib/core/` must be dependency-free beyond the C++ standard library.

- [ ] **Step 3: Print binary-size deltas**

```bash
pio run -e emory 2>&1 | grep -E 'RAM|Flash'
pio run -e nora 2>&1 | grep -E 'RAM|Flash'
```

Record the current sizes in the commit message or a scratch note — useful later when we add Phase 2 hardware drivers and want to know how much the logic added.

- [ ] **Step 4: Tag Phase 1 complete**

```bash
cd /Users/samueldacanay/dev/personal/word-clock-for-my-daughters
git tag -a phase-1-complete -m "Phase 1: native-testable firmware logic complete"
git push origin phase-1-complete
```

---

## Self-Review

**Spec coverage (from `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md`):**

- ✓ **§Letter Grid — 13×13 layout, Emory + Nora variants, hidden acrostic** → Task 1 (grid module).
- ✓ **§Word inventory (24 clock + 4 birthday + fillers)** → Task 1 (`WordId` enum + spans).
- ✓ **§Time phrasing "IT IS [minutes] PAST/TO [hour] IN THE MORNING/AFTERNOON/EVENING / AT NIGHT"** → Task 2 (`time_to_words`).
- ✓ **§Noon + midnight special cases ("IT IS TWELVE NOON" / "IT IS TWELVE AT NIGHT")** → Task 2.
- ✓ **§5-minute floor rounding** → Task 2 (`m5 = minute - minute % 5`).
- ✓ **§Holiday modes (9 holidays, some computed)** → Task 4 (`holidays`).
- ✓ **§Birthday mode trigger + birth-minute trigger** → Task 5 (`birthday`).
- ✓ **§Bedtime dim (19:00-08:00, ~10%)** → Task 6 (`dim_schedule`).
- ✓ **§Per-kid configuration via compile-time flag** → config is passed as function arguments (tests cover both); firmware's Phase-2 glue layer will build the config struct from `CLOCK_BIRTH_*` macros.
- ✗ **§Captive portal WiFi provisioning, audio I2S, FastLED, RTC, buttons** — all Arduino-dependent, **intentionally out of scope** for Phase 1 (see plan's `Out of scope` note).
- ✗ **§Display colors (RGB values for each palette)** — color values live in the Phase-2 display module; `holidays` here returns a `Palette` enum only. Intentional.

**Placeholder scan:** No "TODO", "TBD", or "implement later". All test code is concrete. All implementation code is present. Comments in `grid.cpp` that explain the filler-cell derivation are acceptable (they document a tricky spec inference, not a deferred task).

**Type consistency:** `WordId`, `CellSpan`, `WordSet`, `Grid`, `ClockTarget`, `Palette`, `BirthdayConfig`, `BirthdayState` — all used identically across their creation task and any later reference. `uint8_t` widths are consistent. `hour24` / `minute` parameter names consistent across `time_to_words` and `dim_schedule`.

**One fix I caught during self-review:** Task 2's `time_to_words` suffix logic needed to handle the edge where a TO-phrase lands at exactly noon or midnight (e.g., 11:35 AM's next hour is 12:00 PM). I addressed this with the `if (tod == NOON_EXACT) tod = AFTERNOON;` remap inside `time_to_words`. Golden file in Task 3 exercises these (11:35-11:55 AM and 11:35-11:55 PM).

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-14-phase-1-firmware-logic.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using `executing-plans`, batch execution with checkpoints.

Which approach?
