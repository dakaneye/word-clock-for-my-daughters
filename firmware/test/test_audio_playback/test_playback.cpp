// firmware/test/test_audio_playback/test_playback.cpp
#include <unity.h>
#include <cstring>
#include "audio/playback.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

static PlaybackEvent make_event(PlaybackEvent::Kind k) {
    return PlaybackEvent{k};
}

void test_idle_play_lullaby_opens_lullaby1(void) {
    PlaybackTransition t = next_transition(
        State::Idle, Track::None,
        make_event(PlaybackEvent::Kind::PlayLullabyRequested));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::OpenFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NOT_NULL(t.path);
    TEST_ASSERT_EQUAL_STRING("/lullaby1.wav", t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::LullabyOne), static_cast<int>(t.next_track));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_play_lullaby_opens_lullaby1);
    return UNITY_END();
}
