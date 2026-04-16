// firmware/lib/core/src/grid.cpp
#include "grid.h"

namespace wc {

namespace {

// Emory's 13×13 letter layout, row-major. From spec §"Letter Grid":
// Row 0:  I T E I S M T E N H A L F
// Row 1:  Q U A R T E R T W E N T Y
// Row 2:  F I V E O M I N U T E S R
// Row 3:  Y H A P P Y P A S T T O 1
// Row 4:  O N E B I R T H T H R E E
// Row 5:  E L E V E N F O U R D A Y
// Row 6:  T W O E I G H T S E V E N
// Row 7:  N I N E S I X T W E L V E
// Row 8:  E M O R Y 0 A T F I V E 6
// Row 9:  T E N O C L O C K N O O N
// Row 10: 2 I N T H E M O R N I N G
// Row 11: A F T E R N O O N 0 A T 2
// Row 12: E V E N I N G 3 N I G H T
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

// Nora's row 8: N O R A R 1 A T F I V E 9
constexpr char NORA_ROW_8[13] = {
    'N','O','R','A','R','1','A','T','F','I','V','E','9',
};

// Set all word-cell spans (same positions for both grids; only NAME length differs).
inline void set_spans_common(Grid& g) {
    auto set = [&g](WordId id, uint8_t row, uint8_t col, uint8_t length) {
        g.spans[static_cast<uint8_t>(id)] = {row, col, length};
    };
    // Row 0
    set(WordId::IT, 0, 0, 2);
    set(WordId::IS, 0, 3, 2);
    set(WordId::TEN_MIN, 0, 6, 3);
    set(WordId::HALF, 0, 9, 4);
    // Row 1
    set(WordId::QUARTER, 1, 0, 7);
    set(WordId::TWENTY, 1, 7, 6);
    // Row 2
    set(WordId::FIVE_MIN, 2, 0, 4);
    set(WordId::MINUTES, 2, 5, 7);
    // Row 3
    set(WordId::HAPPY, 3, 1, 5);
    set(WordId::PAST, 3, 6, 4);
    set(WordId::TO, 3, 10, 2);
    // Row 4
    set(WordId::ONE, 4, 0, 3);
    set(WordId::BIRTH, 4, 3, 5);
    set(WordId::THREE, 4, 8, 5);
    // Row 5
    set(WordId::ELEVEN, 5, 0, 6);
    set(WordId::FOUR, 5, 6, 4);
    set(WordId::DAY, 5, 10, 3);
    // Row 6
    set(WordId::TWO, 6, 0, 3);
    set(WordId::EIGHT, 6, 3, 5);
    set(WordId::SEVEN, 6, 8, 5);
    // Row 7
    set(WordId::NINE, 7, 0, 4);
    set(WordId::SIX, 7, 4, 3);
    set(WordId::TWELVE, 7, 7, 6);
    // Row 8: AT and FIVE_HR same position for both kids; NAME length set per-kid below.
    // AT lives on row 11 (cols 10-11 of "A F T E R N O O N 0 A T 2"), not row 8.
    // Row-8 "AT" letters (cols 6-7) are now decorative-only. This placement keeps
    // the "AT NIGHT" phrase in reading order for times like "ELEVEN OCLOCK AT NIGHT" —
    // AT below OCLOCK (row 9) in reading order.
    set(WordId::AT, 11, 10, 2);
    set(WordId::FIVE_HR, 8, 8, 4);
    // Row 9
    set(WordId::TEN_HR, 9, 0, 3);
    set(WordId::OCLOCK, 9, 3, 6);
    set(WordId::NOON, 9, 9, 4);
    // Row 10
    set(WordId::IN, 10, 1, 2);
    set(WordId::THE, 10, 3, 3);
    set(WordId::MORNING, 10, 6, 7);
    // Row 11
    set(WordId::AFTERNOON, 11, 0, 9);
    // Row 12
    set(WordId::EVENING, 12, 0, 7);
    set(WordId::NIGHT, 12, 8, 5);
}

Grid build_emory() {
    Grid g{};
    for (int i = 0; i < 13 * 13; ++i) g.letters[i] = EMORY_LETTERS[i];
    set_spans_common(g);
    g.spans[static_cast<uint8_t>(WordId::NAME)] = {8, 0, 5}; // EMORY
    // Fillers in row-major order. Emory's: E M O R Y 1 0 6 2 0 2 3 (12 chars).
    g.fillers[0]  = {0, 2, 1};   // E
    g.fillers[1]  = {0, 5, 1};   // M
    g.fillers[2]  = {2, 4, 1};   // O
    g.fillers[3]  = {2, 12, 1};  // R
    g.fillers[4]  = {3, 0, 1};   // Y
    g.fillers[5]  = {3, 12, 1};  // 1
    g.fillers[6]  = {8, 5, 1};   // 0
    g.fillers[7]  = {8, 12, 1};  // 6
    g.fillers[8]  = {10, 0, 1};  // 2
    g.fillers[9]  = {11, 9, 1};  // 0
    g.fillers[10] = {11, 12, 1}; // 2
    g.fillers[11] = {12, 7, 1};  // 3
    g.filler_count = 12;
    return g;
}

Grid build_nora() {
    Grid g{};
    // Start from Emory's letters, then overwrite: row 8 entirely, plus the shared-row filler
    // cells that differ because the acrostics differ.
    // Nora acrostic (13 chars): N O R A M A R 1 9 2 0 2 5
    // Filler cells in row-major order: (0,2)=N (0,5)=O (2,4)=R (2,12)=A (3,0)=M (3,12)=A
    //                                  (8,4)=R (8,5)=1 (8,12)=9 (10,0)=2 (11,9)=0 (11,12)=2 (12,7)=5
    for (int i = 0; i < 13 * 13; ++i) g.letters[i] = EMORY_LETTERS[i];
    for (int i = 0; i < 13; ++i) g.letters[8 * 13 + i] = NORA_ROW_8[i];
    g.letters[0  * 13 +  2] = 'N';
    g.letters[0  * 13 +  5] = 'O';
    g.letters[2  * 13 +  4] = 'R';
    g.letters[2  * 13 + 12] = 'A';
    g.letters[3  * 13 +  0] = 'M';
    g.letters[3  * 13 + 12] = 'A';
    g.letters[10 * 13 +  0] = '2';
    g.letters[11 * 13 +  9] = '0';
    g.letters[11 * 13 + 12] = '2';
    g.letters[12 * 13 +  7] = '5';

    set_spans_common(g);
    g.spans[static_cast<uint8_t>(WordId::NAME)] = {8, 0, 4}; // NORA (4 chars)

    g.fillers[0]  = {0, 2, 1};   // N
    g.fillers[1]  = {0, 5, 1};   // O
    g.fillers[2]  = {2, 4, 1};   // R
    g.fillers[3]  = {2, 12, 1};  // A
    g.fillers[4]  = {3, 0, 1};   // M
    g.fillers[5]  = {3, 12, 1};  // A
    g.fillers[6]  = {8, 4, 1};   // R
    g.fillers[7]  = {8, 5, 1};   // 1
    g.fillers[8]  = {8, 12, 1};  // 9
    g.fillers[9]  = {10, 0, 1};  // 2
    g.fillers[10] = {11, 9, 1};  // 0
    g.fillers[11] = {11, 12, 1}; // 2
    g.fillers[12] = {12, 7, 1};  // 5
    g.filler_count = 13;
    return g;
}

const Grid& emory_grid() {
    static const Grid g = build_emory();
    return g;
}

const Grid& nora_grid() {
    static const Grid g = build_nora();
    return g;
}

} // namespace

const Grid& get(ClockTarget target) {
    return target == ClockTarget::EMORY ? emory_grid() : nora_grid();
}

} // namespace wc
