// firmware/lib/display/src/led_map.cpp
#include "display/led_map.h"

#include <array>

namespace wc::display {

namespace {

// WordId enum value → LED chain index.
//
// Derived from the actual data-line routing in
// `hardware/word-clock.kicad_pcb` by walking the chain:
// LED_DATA_OUT (from level shifter through 300Ω) feeds D1's DIN; each
// LED's DOUT feeds the next LED's DIN. The auto-router did NOT lay the
// chain out in D-numerical order — it minimized trace length, so e.g.
// D5 → D10 → D6 → D7 → D8 → D9 → D12 → D13 → D14 → D11 → D15...
//
// Earlier versions of this map assumed "D-number minus 1" which matched
// the breadboard wiring (manually patched in numerical order) but does
// not match the production PCB. That mismatch caused the wrong words to
// light up at runtime — e.g. firmware lighting "PAST" (idx 8) lit the
// physical LED at chain pos 8 = D8 = HAPPY.
constexpr std::array<uint8_t, 35> kMap = {
    /* IT        */  0,  // D1
    /* IS        */  1,  // D2
    /* TEN_MIN   */  2,  // D3
    /* HALF      */  3,  // D4
    /* QUARTER   */  4,  // D5
    /* TWENTY    */  5,  // D10 (chain skips D6-D9, returns later)
    /* FIVE_MIN  */  6,  // D6
    /* MINUTES   */  7,  // D7
    /* PAST      */  9,  // D9
    /* TO        */ 10,  // D12 (chain skips D11, picked up later)
    /* ONE       */ 11,  // D13
    /* TWO       */ 17,  // D18
    /* THREE     */ 13,  // D11
    /* FOUR      */ 15,  // D16
    /* FIVE_HR   */ 24,  // D25
    /* SIX       */ 21,  // D22
    /* SEVEN     */ 19,  // D20
    /* EIGHT     */ 18,  // D19
    /* NINE      */ 20,  // D21
    /* TEN_HR    */ 25,  // D26
    /* ELEVEN    */ 14,  // D15
    /* TWELVE    */ 22,  // D23
    /* OCLOCK    */ 26,  // D27
    /* NOON      */ 27,  // D28
    /* IN        */ 28,  // D29
    /* THE       */ 29,  // D30
    /* AT        */ 32,  // D35
    /* MORNING   */ 30,  // D34
    /* AFTERNOON */ 31,  // D31
    /* EVENING   */ 33,  // D32
    /* NIGHT     */ 34,  // D33
    /* HAPPY     */  8,  // D8
    /* BIRTH     */ 12,  // D14
    /* DAY       */ 16,  // D17
    /* NAME      */ 23,  // D24
};

} // namespace

uint8_t index_of(WordId w) {
    return kMap[static_cast<uint8_t>(w)];
}

} // namespace wc::display
