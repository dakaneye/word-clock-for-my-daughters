// firmware/test/test_audio_playback/test_playback.cpp
#include <unity.h>
#include <cstring>
#include <initializer_list>
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

void test_lullaby1_end_switches_to_lullaby2(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::LullabyOne,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::SwitchFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_EQUAL_STRING("/lullaby2.wav", t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Playing), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::LullabyTwo), static_cast<int>(t.next_track));
}

void test_lullaby2_end_closes_to_idle(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::LullabyTwo,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NULL(t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}

void test_birth_end_closes_to_idle(void) {
    PlaybackTransition t = next_transition(
        State::Playing, Track::Birth,
        make_event(PlaybackEvent::Kind::FileEnded));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                      static_cast<int>(t.action));
    TEST_ASSERT_NULL(t.path);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}

void test_playing_stop_closes_to_idle(void) {
    // Test for each Track value to confirm the transition is uniform.
    for (Track t_in : {Track::LullabyOne, Track::LullabyTwo, Track::Birth}) {
        PlaybackTransition t = next_transition(
            State::Playing, t_in,
            make_event(PlaybackEvent::Kind::StopRequested));
        TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::CloseFile),
                          static_cast<int>(t.action));
        TEST_ASSERT_NULL(t.path);
        TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
        TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
    }
}

void test_idle_stop_stays_idle(void) {
    PlaybackTransition t = next_transition(
        State::Idle, Track::None,
        make_event(PlaybackEvent::Kind::StopRequested));
    TEST_ASSERT_EQUAL(static_cast<int>(PlaybackTransition::Action::None),
                      static_cast<int>(t.action));
    TEST_ASSERT_EQUAL(static_cast<int>(State::Idle), static_cast<int>(t.next_state));
    TEST_ASSERT_EQUAL(static_cast<int>(Track::None), static_cast<int>(t.next_track));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_play_lullaby_opens_lullaby1);
    RUN_TEST(test_lullaby1_end_switches_to_lullaby2);
    RUN_TEST(test_lullaby2_end_closes_to_idle);
    RUN_TEST(test_birth_end_closes_to_idle);
    RUN_TEST(test_playing_stop_closes_to_idle);
    RUN_TEST(test_idle_stop_stays_idle);
    return UNITY_END();
}
