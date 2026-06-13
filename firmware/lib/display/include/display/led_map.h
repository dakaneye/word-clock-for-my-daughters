// firmware/lib/display/include/display/led_map.h
#pragma once

#include <cstdint>
#include "word_id.h"

namespace wc::display {

// A contiguous run of LEDs in the data chain that belong to one word.
// Every LED in the span is lit the same color so long words (which span
// several physical LEDs) illuminate evenly.
struct LedSpan {
    uint8_t start;  // first chain index, 0..LED_COUNT-1
    uint8_t count;  // number of LEDs in the word, >= 1
};

// Returns the LED chain span for a given WordId.
//
// Source of truth: hardware/word-clock.kicad_pcb. Each WS2812B footprint
// carries a Value field naming its word (LED_<WORD>, with _1/_2 suffixes
// for the 2nd/3rd LED of a multi-LED word); the data line wires the LEDs
// in physical reading order (left->right, top->bottom). Walking the
// DOUT->DIN chain gives each LED its chain index, and because a word's
// letters sit adjacent on the face, each word's LEDs form a contiguous
// span. The 63 LEDs tile the 35 words with no gaps or overlaps — the
// led_map unit test asserts this, so a future PCB re-route or re-letter
// is caught here rather than silently lighting the wrong words.
LedSpan span_of(WordId w);

// Convenience: the first chain index of a word's span
// (== span_of(w).start). The renderer lights the whole span the same
// color, so this index is a valid representative for that word's color.
uint8_t index_of(WordId w);

} // namespace wc::display
