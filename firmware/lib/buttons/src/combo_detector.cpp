#include "buttons/combo_detector.h"

namespace wc::buttons {

bool ComboDetector::step(bool hour_pressed, bool audio_pressed, uint32_t now_ms) {
    const bool both_pressed = hour_pressed && audio_pressed;

    if (!both_pressed) {
        // Either release cancels + resets.
        armed_ = false;
        fired_ = false;
        return false;
    }

    // Both pressed.
    if (!armed_) {
        armed_ = true;
        fired_ = false;
        armed_since_ = now_ms;
        return false;
    }

    // Still held; don't double-fire.
    if (fired_) {
        return false;
    }

    if (now_ms - armed_since_ >= COMBO_MS) {
        fired_ = true;
        return true;
    }
    return false;
}

bool ComboDetector::in_combo() const {
    return armed_;
}

} // namespace wc::buttons
