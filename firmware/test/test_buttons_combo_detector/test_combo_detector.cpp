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

// After a successful fire + full release, a fresh press should re-arm and
// fire again 10s later. Spec: "fires exactly once per continuous hold."
void test_fires_again_after_full_release_and_rearm(void) {
    ComboDetector c;
    // First combo: arm at t=0, fire at t=10000.
    c.step(true, true, 0);
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
    // Release both; in_combo() transitions to false, fired_ should reset.
    c.step(false, false, 10100);
    TEST_ASSERT_FALSE(c.in_combo());
    // Re-press both at t=15000. Expect a fresh 10s countdown.
    c.step(true, true, 15000);
    TEST_ASSERT_FALSE(c.step(true, true, 24999));
    TEST_ASSERT_TRUE(c.step(true, true, 25000));
}

// The armed timer must start when BOTH buttons are simultaneously pressed,
// not when the first of the two was pressed. A plausible implementation
// bug would treat any hold-since-first-press as the combo interval.
void test_armed_timer_starts_on_simultaneous_press_not_first_press(void) {
    ComboDetector c;
    // Hour alone for 9000ms.
    for (uint32_t t = 0; t <= 9000; t += 1000) {
        c.step(true, false, t);
    }
    // Audio joins at t=9000. Combo timer starts now, not at t=0.
    c.step(true, true, 9000);
    // 10s from t=9000 is t=19000, not t=10000.
    TEST_ASSERT_FALSE(c.step(true, true, 10000));
    TEST_ASSERT_FALSE(c.step(true, true, 18999));
    TEST_ASSERT_TRUE(c.step(true, true, 19000));
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
    RUN_TEST(test_fires_again_after_full_release_and_rearm);
    RUN_TEST(test_armed_timer_starts_on_simultaneous_press_not_first_press);
    return UNITY_END();
}
