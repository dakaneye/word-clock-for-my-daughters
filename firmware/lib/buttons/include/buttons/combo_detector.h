#pragma once

#include <cstdint>

namespace wc::buttons {

// Detects Hour + Audio held simultaneously for 10s.
// Fires exactly once per continuous hold.
class ComboDetector {
public:
    // Feed the debounced states of Hour and Audio + current time.
    // Returns true on the tick where the 10s threshold is crossed.
    bool step(bool hour_pressed, bool audio_pressed, uint32_t now_ms);

    // True iff BOTH Hour and Audio were debounced-pressed on the most
    // recent step() call. Transitions to false immediately when either
    // button releases, regardless of whether the 10s threshold already
    // fired during the hold. Adapter uses this to suppress the
    // individual HourTick / AudioPressed events for the duration of
    // the both-held window.
    bool in_combo() const;

private:
    static constexpr uint32_t COMBO_MS = 10000;
    bool armed_ = false;           // both pressed at last step?
    bool fired_ = false;           // already fired this hold?
    uint32_t armed_since_ = 0;
};

} // namespace wc::buttons
