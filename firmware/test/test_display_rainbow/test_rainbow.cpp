#include <unity.h>
#include "display/rainbow.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

// HAPPY at t=0 has 0° phase offset → should be red-dominant.
void test_rainbow_happy_at_t0_is_red(void) {
    const Rgb c = rainbow(WordId::HAPPY, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.r);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rainbow_happy_at_t0_is_red);
    return UNITY_END();
}
