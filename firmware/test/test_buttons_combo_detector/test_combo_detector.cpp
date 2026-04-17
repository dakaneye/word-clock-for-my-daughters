#include <unity.h>
#include "buttons/combo_detector.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Both held for exactly 10000ms fires the combo on that tick.
void test_fires_after_10s_of_both_held(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(true, true, 0));
    TEST_ASSERT_FALSE(c.step(true, true, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 9999));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
}

// Fires only once per continuous hold, even if held past 10s.
void test_does_not_refire_while_held(void) {
    ComboDetector c;
    c.step(true, true, 0);
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
    TEST_ASSERT_FALSE(c.step(true, true, 11000));
    TEST_ASSERT_FALSE(c.step(true, true, 15000));
    TEST_ASSERT_FALSE(c.step(true, true, 30000));
}

// Hour released just before 10s cancels the combo.
void test_hour_release_cancels_combo(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 9999);
    // Hour releases at t=9999 — no fire.
    TEST_ASSERT_FALSE(c.step(false, true, 9999));
    // Both re-pressed at t=10000 — timer starts fresh.
    TEST_ASSERT_FALSE(c.step(true, true, 10000));
    // Needs another 10s from the re-press moment.
    TEST_ASSERT_FALSE(c.step(true, true, 19999));
    TEST_ASSERT_TRUE(c.step(true, true, 20000));
}

// Audio released mid-hold cancels the combo (symmetric to Hour).
void test_audio_release_cancels_combo(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 5000);
    TEST_ASSERT_FALSE(c.step(true, false, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 5001));
    TEST_ASSERT_FALSE(c.step(true, true, 15000));
    TEST_ASSERT_TRUE(c.step(true, true, 15001));
}

// Minute held alongside does nothing — combo_detector only sees Hour+Audio.
void test_minute_not_part_of_combo(void) {
    // ComboDetector's API only takes hour & audio state; Minute state
    // never reaches it. This test documents the API contract: the
    // caller (buttons.cpp adapter) must not pass Minute state here.
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(true, true, 0));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
    // (Calling with only hour or only audio would represent a single
    // button — tested in the hour/audio release-cancel cases above.)
}

// in_combo() is true only while both buttons are actively debounced-pressed.
void test_in_combo_tracks_both_pressed(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.in_combo());  // initial
    c.step(true, false, 0);
    TEST_ASSERT_FALSE(c.in_combo());  // only hour
    c.step(true, true, 100);
    TEST_ASSERT_TRUE(c.in_combo());   // both pressed
    c.step(true, true, 5000);
    TEST_ASSERT_TRUE(c.in_combo());   // still both
    c.step(true, false, 5100);
    TEST_ASSERT_FALSE(c.in_combo());  // audio released — immediately false
}

// in_combo() reports false after fire + release.
void test_in_combo_false_after_fire_and_release(void) {
    ComboDetector c;
    c.step(true, true, 0);
    c.step(true, true, 10000);        // fires
    TEST_ASSERT_TRUE(c.in_combo());   // still both held
    c.step(false, true, 10100);
    TEST_ASSERT_FALSE(c.in_combo());  // released — false immediately
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fires_after_10s_of_both_held);
    RUN_TEST(test_does_not_refire_while_held);
    RUN_TEST(test_hour_release_cancels_combo);
    RUN_TEST(test_audio_release_cancels_combo);
    RUN_TEST(test_minute_not_part_of_combo);
    RUN_TEST(test_in_combo_tracks_both_pressed);
    RUN_TEST(test_in_combo_false_after_fire_and_release);
    return UNITY_END();
}
