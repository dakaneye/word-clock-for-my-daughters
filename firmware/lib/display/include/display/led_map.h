// firmware/lib/display/include/display/led_map.h
#pragma once

#include <cstdint>
#include "display/rgb.h"
#include "word_id.h"

namespace wc::display {

// Returns the LED chain index (0..34) for a given WordId.
// Source of truth: hardware/word-clock-all-pos.csv. The PCB wires
// LEDs in spatial reading order (left→right, top→bottom), not in
// WordId enum order — this is an explicit permutation, encoded as a
// table so a future PCB LED reordering is caught by the permutation
// unit test rather than silently miswiring the firmware.
uint8_t index_of(WordId w);

} // namespace wc::display
