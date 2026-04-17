#pragma once

#include <cstdint>

namespace wc::buttons {

// Debounces a single active-low button. Feed raw state (true = pressed).
// Reports a press-down edge once per press (fires on the first stable
// transition from released to pressed).
class Debouncer {
public:
    // Reset to released state. now_ms is used as the initial stability timestamp.
    explicit Debouncer(uint32_t now_ms = 0);

    // Feed current raw reading and timestamp. Returns true if this call
    // corresponds to a just-debounced press-down edge (fires exactly once
    // per press).
    bool step(bool raw_pressed, uint32_t now_ms);

    // Query current debounced state (does not trigger any edge event).
    bool is_pressed() const { return stable_; }

private:
    static constexpr uint32_t STABLE_MS = 25;
    bool stable_ = false;          // last-committed debounced state
    bool candidate_ = false;       // reading currently being measured for stability
    uint32_t candidate_since_ = 0; // millis when candidate first observed
};

} // namespace wc::buttons
