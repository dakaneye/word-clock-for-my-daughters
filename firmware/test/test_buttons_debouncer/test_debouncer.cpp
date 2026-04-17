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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_press_commits_after_25ms_stable);
    RUN_TEST(test_bounce_within_window_suppresses_press);
    RUN_TEST(test_press_edge_fires_only_once_per_press);
    RUN_TEST(test_release_does_not_fire_edge);
    RUN_TEST(test_second_press_after_release_fires);
    RUN_TEST(test_is_pressed_reflects_debounced_state);
    return UNITY_END();
}
