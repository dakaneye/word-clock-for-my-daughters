#include <unity.h>
#include "buttons/debouncer.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Raw press held for 25ms must commit on the 25ms tick.
void test_press_commits_after_25ms_stable(void) {
    Debouncer d;
    // Before 25ms the press isn't stable yet — no edge.
    TEST_ASSERT_FALSE(d.step(/* pressed = */ true, /* now_ms = */ 0));
    TEST_ASSERT_FALSE(d.step(true, 10));
    TEST_ASSERT_FALSE(d.step(true, 24));
    // At 25ms of continuous press the edge fires.
    TEST_ASSERT_TRUE(d.step(true, 25));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_press_commits_after_25ms_stable);
    return UNITY_END();
}
