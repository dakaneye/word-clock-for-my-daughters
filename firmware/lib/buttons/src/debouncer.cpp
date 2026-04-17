#include "buttons/debouncer.h"

namespace wc::buttons {

Debouncer::Debouncer(uint32_t now_ms) {
    candidate_since_ = now_ms;
}

bool Debouncer::step(bool raw_pressed, uint32_t now_ms) {
    if (raw_pressed != candidate_) {
        // Raw state changed — restart the stability window.
        candidate_ = raw_pressed;
        candidate_since_ = now_ms;
        return false;
    }
    // Raw state matches candidate — check if it's been stable long enough.
    if (now_ms - candidate_since_ < STABLE_MS) {
        return false;
    }
    // Stable. Commit if it's a new state; otherwise no change.
    if (candidate_ == stable_) {
        return false;
    }
    const bool was_released = !stable_;
    stable_ = candidate_;
    // Press-down edge: just became pressed after being released.
    return stable_ && was_released;
}

} // namespace wc::buttons
