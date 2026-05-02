// firmware/lib/audio/src/playback.cpp
//
// Pure-logic playback state machine. See audio/playback.h.
#include "audio/playback.h"

namespace wc::audio {

PlaybackTransition next_transition(State state, Track track, PlaybackEvent event) {
    using K = PlaybackEvent::Kind;
    using A = PlaybackTransition::Action;

    switch (event.kind) {
    case K::PlayLullabyRequested:
        if (state == State::Idle) {
            return {A::OpenFile, kLullabyOnePath, State::Playing, Track::LullabyOne};
        }
        return {A::None, nullptr, state, track};

    case K::StopRequested:
        // To be implemented in Task 3.
        return {A::None, nullptr, state, track};

    case K::FileEnded:
        // To be implemented in Task 2.
        return {A::None, nullptr, state, track};

    case K::BirthdayFired:
        // To be implemented in Task 4.
        return {A::None, nullptr, state, track};
    }
    return {A::None, nullptr, state, track};
}

} // namespace wc::audio
