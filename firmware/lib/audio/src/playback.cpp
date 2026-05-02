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
            return {A::OpenFile, LULLABY_ONE_PATH, State::Playing, Track::LullabyOne};
        }
        // Already playing — ignore; caller must stop first.
        return {A::None, nullptr, state, track};

    case K::StopRequested:
        if (state == State::Playing) {
            return {A::CloseFile, nullptr, State::Idle, Track::None};
        }
        // Already idle — no-op; main.cpp gates on is_playing() but handled defensively.
        return {A::None, nullptr, State::Idle, Track::None};

    case K::FileEnded:
        if (state == State::Playing && track == Track::LullabyOne) {
            return {A::SwitchFile, LULLABY_TWO_PATH, State::Playing, Track::LullabyTwo};
        }
        // LullabyTwo or Birth EOF — close and idle.
        // (Any other combination is unreachable by construction; defensive default.)
        return {A::CloseFile, nullptr, State::Idle, Track::None};

    case K::BirthdayFired:
        if (state == State::Idle) {
            return {A::OpenFile, BIRTH_PATH, State::Playing, Track::Birth};
        }
        // Playing/* — switch to birth, interrupting whatever was playing.
        // Includes the Playing/Birth case: the NVS year-stamp gate
        // prevents BirthdayFired from re-firing within the same year, so
        // this row is unreachable in practice; if it did fire, SwitchFile
        // would re-open birth.wav from the start, which is benign.
        return {A::SwitchFile, BIRTH_PATH, State::Playing, Track::Birth};
    }
    return {A::None, nullptr, state, track};
}

} // namespace wc::audio
