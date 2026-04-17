// firmware/lib/buttons/include/buttons/event.h
#pragma once

namespace wc::buttons {

enum class Event {
    HourTick,
    MinuteTick,
    AudioPressed,
    ResetCombo,
};

} // namespace wc::buttons
