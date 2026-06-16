#include <unity.h>
#include "buttons/debouncer.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Raw press held for 25ms must commit on the 25ms tick.
void test_press_commits_after_25ms_stable(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_FALSE(d.step(true, 10));
    TEST_ASSERT_FALSE(d.step(true, 24));
    TEST_ASSERT_TRUE(d.step(true, 25));
}

// A bounce (state flip) within the 25ms window does NOT commit a press.
void test_bounce_within_window_suppresses_press(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));    // raw goes pressed
    TEST_ASSERT_FALSE(d.step(false, 10));  // bounces back to released — timer resets
    TEST_ASSERT_FALSE(d.step(true, 20));   // bounces pressed again — timer resets
    TEST_ASSERT_FALSE(d.step(true, 30));   // only 10ms stable — still not committed
    // Commits at 20+25=45 (25ms after the last raw-state change to true).
    TEST_ASSERT_TRUE(d.step(true, 45));
}

// Press-down edge fires exactly once per press.
void test_press_edge_fires_only_once_per_press(void) {
    Debouncer d;
    // Commit the press edge.
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));
    // Still held — no further edges.
    TEST_ASSERT_FALSE(d.step(true, 100));
    TEST_ASSERT_FALSE(d.step(true, 500));
    TEST_ASSERT_FALSE(d.step(true, 1000));
}

// Release does NOT fire a press edge (press edges only, no release edges).
void test_release_does_not_fire_edge(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));     // press committed
    TEST_ASSERT_FALSE(d.step(false, 100));  // raw released
    TEST_ASSERT_FALSE(d.step(false, 125));  // release committed, still no edge
    TEST_ASSERT_FALSE(d.step(false, 200));
}

// After release, a fresh press fires a new edge.
void test_second_press_after_release_fires(void) {
    Debouncer d;
    TEST_ASSERT_FALSE(d.step(true, 0));
    TEST_ASSERT_TRUE(d.step(true, 25));     // press 1
    TEST_ASSERT_FALSE(d.step(false, 100));
    TEST_ASSERT_FALSE(d.step(false, 125));  // released
    TEST_ASSERT_FALSE(d.step(true, 200));
    TEST_ASSERT_TRUE(d.step(true, 225));    // press 2
}

// is_pressed() reports the current debounced state, not the raw state.
void test_is_pressed_reflects_debounced_state(void) {
    Debouncer d;
    d.step(true, 0);
    TEST_ASSERT_FALSE(d.is_pressed()); // raw true but not yet stable
    d.step(true, 25);
    TEST_ASSERT_TRUE(d.is_pressed());  // now stable pressed
    d.step(false, 100);
    TEST_ASSERT_TRUE(d.is_pressed());  // raw released but not yet stable
    d.step(false, 125);
    TEST_ASSERT_FALSE(d.is_pressed()); // now stable released
}

// A press whose stability window straddles the millis() UINT32_MAX rollover
// still commits at exactly 25ms elapsed. candidate_since_ sits at 2^32-6, so
// the post-wrap timestamps compute elapsed via unsigned subtraction.
void test_press_commits_across_millis_rollover(void) {
    Debouncer d(0xFFFFFFFF - 5);          // candidate_since_ = 2^32-6, released
    TEST_ASSERT_FALSE(d.step(true, 0xFFFFFFFF - 5)); // raw goes pressed pre-wrap
    TEST_ASSERT_FALSE(d.step(true, 9));   // wrapped; elapsed 15 < 25
    TEST_ASSERT_FALSE(d.step(true, 18));  // wrapped; elapsed 24 < 25
    TEST_ASSERT_TRUE(d.step(true, 19));   // wrapped; elapsed 25 — commits
}

// A bounce that resets the timer just before the rollover boundary restarts
// the 25ms window at the wrapped-side timestamp, not the absolute one.
void test_bounce_across_rollover_resets_window(void) {
    Debouncer d(0xFFFFFFFF - 30);              // candidate_since_ = 2^32-31, released
    TEST_ASSERT_FALSE(d.step(true, 0xFFFFFFFF - 30));  // pressed — window starts
    TEST_ASSERT_FALSE(d.step(false, 0xFFFFFFFF - 20)); // bounce released — resets
    TEST_ASSERT_FALSE(d.step(true, 0xFFFFFFFF - 10));  // pressed again — resets to 2^32-11
    TEST_ASSERT_FALSE(d.step(true, 13));      // wrapped; elapsed 24 < 25
    TEST_ASSERT_TRUE(d.step(true, 14));       // wrapped; elapsed 25 — commits
}

// is_pressed() settles correctly when both the press and the subsequent
// release windows straddle the rollover boundary.
void test_is_pressed_settles_across_rollover(void) {
    Debouncer d(0xFFFFFFFF - 5);
    d.step(true, 0xFFFFFFFF - 5);
    TEST_ASSERT_FALSE(d.is_pressed());  // pressed but elapsed 0
    d.step(true, 19);                   // wrapped; elapsed 25
    TEST_ASSERT_TRUE(d.is_pressed());   // now stable pressed
    d.step(false, 30);                  // raw released — window starts at 30
    TEST_ASSERT_TRUE(d.is_pressed());   // not yet stable
    d.step(false, 55);                  // elapsed 25 — release commits
    TEST_ASSERT_FALSE(d.is_pressed());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_press_commits_after_25ms_stable);
    RUN_TEST(test_bounce_within_window_suppresses_press);
    RUN_TEST(test_press_edge_fires_only_once_per_press);
    RUN_TEST(test_release_does_not_fire_edge);
    RUN_TEST(test_second_press_after_release_fires);
    RUN_TEST(test_is_pressed_reflects_debounced_state);
    RUN_TEST(test_press_commits_across_millis_rollover);
    RUN_TEST(test_bounce_across_rollover_resets_window);
    RUN_TEST(test_is_pressed_settles_across_rollover);
    return UNITY_END();
}
