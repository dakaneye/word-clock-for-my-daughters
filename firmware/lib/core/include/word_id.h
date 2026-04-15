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
