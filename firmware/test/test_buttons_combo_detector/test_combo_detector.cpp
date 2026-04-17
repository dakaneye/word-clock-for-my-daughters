#include <unity.h>
#include "buttons/combo_detector.h"

using namespace wc::buttons;

void setUp(void) {}
void tearDown(void) {}

// Both held for exactly 10000ms fires the combo on that tick.
void test_fires_after_10s_of_both_held(void) {
    ComboDetector c;
    TEST_ASSERT_FALSE(c.step(/* hour = */ true, /* audio = */ true, /* now = */ 0));
    TEST_ASSERT_FALSE(c.step(true, true, 5000));
    TEST_ASSERT_FALSE(c.step(true, true, 9999));
    TEST_ASSERT_TRUE(c.step(true, true, 10000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fires_after_10s_of_both_held);
    return UNITY_END();
}
