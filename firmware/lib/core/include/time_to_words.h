// firmware/lib/core/include/time_to_words.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc {

struct WordSet {
    WordId words[12];
    uint8_t count;
};

// Computes the words to light for a given 24-hour clock time.
// hour24: 0-23. minute: 0-59. Minute is internally floored to the 5-minute block.
WordSet time_to_words(uint8_t hour24, uint8_t minute);

} // namespace wc
