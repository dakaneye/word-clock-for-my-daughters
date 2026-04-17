// firmware/lib/display/src/led_map.cpp
#include "display/led_map.h"

#include <array>

namespace wc::display {

namespace {

// WordId enum value → LED chain index. Row i is for WordId(i).
// Derived from hardware/word-clock-all-pos.csv D-number minus 1.
constexpr std::array<uint8_t, 35> kMap = {
    /* IT        */  0,
    /* IS        */  1,
    /* TEN_MIN   */  2,
    /* HALF      */  3,
    /* QUARTER   */  4,
    /* TWENTY    */  9,
    /* FIVE_MIN  */  5,
    /* MINUTES   */  6,
    /* PAST      */  8,
    /* TO        */ 11,
    /* ONE       */ 12,
    /* TWO       */ 17,
    /* THREE     */ 10,
    /* FOUR      */ 15,
    /* FIVE_HR   */ 24,
    /* SIX       */ 21,
    /* SEVEN     */ 19,
    /* EIGHT     */ 18,
    /* NINE      */ 20,
    /* TEN_HR    */ 25,
    /* ELEVEN    */ 14,
    /* TWELVE    */ 22,
    /* OCLOCK    */ 26,
    /* NOON      */ 27,
    /* IN        */ 28,
    /* THE       */ 29,
    /* AT        */ 34,
    /* MORNING   */ 33,
    /* AFTERNOON */ 30,
    /* EVENING   */ 31,
    /* NIGHT     */ 32,
    /* HAPPY     */  7,
    /* BIRTH     */ 13,
    /* DAY       */ 16,
    /* NAME      */ 23,
};

} // namespace

uint8_t index_of(WordId w) {
    return kMap[static_cast<uint8_t>(w)];
}

} // namespace wc::display
