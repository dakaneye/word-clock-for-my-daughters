// firmware/lib/audio/include/audio/playback.h
//
// Pure-logic playback state machine. No Arduino, no SD/I²S/NVS reads —
// the adapter calls next_transition() with the current state + an event,
// and executes the returned action. Compiled into both the ESP32 adapter
// and the native test binary.
//
// Spec: docs/superpowers/specs/2026-05-02-audio-playlist-design.md
#pragma once

#include <cstdint>

namespace wc::audio {

enum class State : uint8_t { Idle, Playing };
enum class Track : uint8_t { None, LullabyOne, LullabyTwo, Birth };

struct PlaybackEvent {
    enum class Kind : uint8_t {
        PlayLullabyRequested,
        StopRequested,
        FileEnded,
        BirthdayFired
    } kind;
};

struct PlaybackTransition {
    enum class Action : uint8_t {
        None,         // no I/O; state and track are unchanged (noop)
        OpenFile,     // open `path`, start I²S
        CloseFile,    // stop I²S, close current file
        SwitchFile    // close current, open `path`
    } action;
    const char* path;     // valid when action is OpenFile or SwitchFile; nullptr otherwise
    State next_state;
    Track next_track;
};

// SD card paths. Static const string literals — safe to return as
// const char* from next_transition() without lifetime concerns.
inline constexpr const char* kLullabyOnePath = "/lullaby1.wav";
inline constexpr const char* kLullabyTwoPath = "/lullaby2.wav";
inline constexpr const char* kBirthPath      = "/birth.wav";

// Pure. Returns the transition to execute for the given event in the
// given state. The caller (ESP32 adapter or test) is responsible for
// performing the I/O and updating its state to the returned values.
PlaybackTransition next_transition(State state, Track track, PlaybackEvent event);

} // namespace wc::audio
