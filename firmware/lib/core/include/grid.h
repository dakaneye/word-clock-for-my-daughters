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
    CellSpan spans[static_cast<uint8_t>(WordId::COUNT)];
    // Filler cells (those not covered by any word), in reading order.
    // Max 16 fillers (Nora has 13, Emory 12).
    CellSpan fillers[16];
    uint8_t filler_count;
};

// Returns a const reference to the grid for the given target.
const Grid& get(ClockTarget target);

} // namespace wc
