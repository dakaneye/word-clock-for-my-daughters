#include <unity.h>
#include "dim_schedule.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

void test_full_bright_midday(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(12, 0));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(8, 0));   // 8 AM is full
    TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(18, 59)); // 6:59 PM is full
}

void test_dim_at_night(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(19, 0));  // 7 PM — first dim minute
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(22, 30));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(0,  0));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(7, 59));  // 7:59 AM — last dim minute
}

// brightness() is hour-granular: the minute argument must not change the result.
// Pin that independence so a future change can't silently make dimming
// minute-dependent within an hour.
void test_minute_independent_full_bright_hour(void) {
    for (uint8_t m = 0; m < 60; ++m) {
        TEST_ASSERT_EQUAL_FLOAT(1.0f, brightness(12, m)); // noon: full every minute
    }
}

void test_minute_independent_evening_dim_hour(void) {
    for (uint8_t m = 0; m < 60; ++m) {
        TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(20, m)); // 8 PM: dim every minute
    }
}

void test_minute_independent_predawn_dim_hour(void) {
    for (uint8_t m = 0; m < 60; ++m) {
        TEST_ASSERT_EQUAL_FLOAT(0.1f, brightness(3, m));  // 3 AM: dim every minute
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_full_bright_midday);
    RUN_TEST(test_dim_at_night);
    RUN_TEST(test_minute_independent_full_bright_hour);
    RUN_TEST(test_minute_independent_evening_dim_hour);
    RUN_TEST(test_minute_independent_predawn_dim_hour);
    return UNITY_END();
}
