#include <unity.h>
#include "ntp/validation.h"

using namespace wc::ntp;

void setUp(void)    {}
void tearDown(void) {}

// 0 is the unix epoch (1970-01-01); below the 2026-01-01 floor.
void test_plausible_zero_rejected(void) {
    TEST_ASSERT_FALSE(is_plausible_epoch(0));
}

// One second below the floor must reject — boundary condition.
void test_plausible_just_below_floor_rejected(void) {
    TEST_ASSERT_FALSE(is_plausible_epoch(MIN_PLAUSIBLE_EPOCH - 1));
}

// Exact floor accepted — boundary condition.
void test_plausible_floor_accepted(void) {
    TEST_ASSERT_TRUE(is_plausible_epoch(MIN_PLAUSIBLE_EPOCH));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plausible_zero_rejected);
    RUN_TEST(test_plausible_just_below_floor_rejected);
    RUN_TEST(test_plausible_floor_accepted);
    return UNITY_END();
}
