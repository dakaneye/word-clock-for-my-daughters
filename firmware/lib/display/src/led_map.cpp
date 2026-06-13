// firmware/lib/display/src/led_map.cpp
#include "display/led_map.h"

#include <array>

namespace wc::display {

namespace {

// WordId enum order -> LED chain span {start, count}.
//
// Derived from hardware/word-clock.kicad_pcb. Each WS2812B footprint's
// Value field names its word (LED_<WORD>, with _1/_2 for the 2nd/3rd LED
// of a multi-LED word); walking the data line DOUT->DIN from the level
// shifter gives each LED its chain index. The router lays the chain in
// physical reading order (left->right, top->bottom), so each word's LEDs
// are a contiguous run. Long words get 2-3 LEDs so they light evenly.
//
// The 63 LEDs partition the 35 words exactly (no gaps, no overlaps) —
// enforced by test_display_led_map. Chain index ranges shown in comments.
constexpr std::array<LedSpan, 35> kSpans = {{
    /* IT        */ {0, 1},   // 0
    /* IS        */ {1, 1},   // 1
    /* TEN_MIN   */ {2, 1},   // 2
    /* HALF      */ {3, 2},   // 3-4
    /* QUARTER   */ {5, 3},   // 5-7
    /* TWENTY    */ {8, 2},   // 8-9
    /* FIVE_MIN  */ {10, 2},  // 10-11
    /* MINUTES   */ {12, 3},  // 12-14
    /* PAST      */ {17, 2},  // 17-18
    /* TO        */ {19, 1},  // 19
    /* ONE       */ {20, 1},  // 20
    /* TWO       */ {30, 1},  // 30
    /* THREE     */ {23, 2},  // 23-24
    /* FOUR      */ {27, 2},  // 27-28
    /* FIVE_HR   */ {42, 2},  // 42-43
    /* SIX       */ {37, 1},  // 37
    /* SEVEN     */ {33, 2},  // 33-34
    /* EIGHT     */ {31, 2},  // 31-32
    /* NINE      */ {35, 2},  // 35-36
    /* TEN_HR    */ {44, 1},  // 44
    /* ELEVEN    */ {25, 2},  // 25-26
    /* TWELVE    */ {38, 2},  // 38-39
    /* OCLOCK    */ {45, 2},  // 45-46
    /* NOON      */ {47, 2},  // 47-48
    /* IN        */ {49, 1},  // 49
    /* THE       */ {50, 1},  // 50
    /* AT        */ {57, 1},  // 57
    /* MORNING   */ {51, 3},  // 51-53
    /* AFTERNOON */ {54, 3},  // 54-56
    /* EVENING   */ {58, 3},  // 58-60
    /* NIGHT     */ {61, 2},  // 61-62
    /* HAPPY     */ {15, 2},  // 15-16
    /* BIRTH     */ {21, 2},  // 21-22
    /* DAY       */ {29, 1},  // 29
    /* NAME      */ {40, 2},  // 40-41
}};

} // namespace

LedSpan span_of(WordId w) {
    return kSpans[static_cast<uint8_t>(w)];
}

uint8_t index_of(WordId w) {
    return kSpans[static_cast<uint8_t>(w)].start;
}

} // namespace wc::display
