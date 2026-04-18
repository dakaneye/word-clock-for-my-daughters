#include <unity.h>
#include "ntp/schedule.h"

using namespace wc::ntp;

void setUp(void)    {}
void tearDown(void) {}

// -- next_backoff_ms ----------------------------------------------

// First retry: 30 s = 30'000 ms.
void test_backoff_n1_thirty_seconds(void) {
    TEST_ASSERT_EQUAL_UINT32(30'000u, next_backoff_ms(1));
}

// Last pre-cap step: 8 m = 480'000 ms.
void test_backoff_n5_eight_minutes(void) {
    TEST_ASSERT_EQUAL_UINT32(480'000u, next_backoff_ms(5));
}

// First step at the cap: 30 m = 1'800'000 ms.
void test_backoff_n6_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(6));
}

// Saturation: arbitrary large N still returns the cap.
void test_backoff_n100_still_at_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(1'800'000u, next_backoff_ms(100));
}

int main(int, char**) {
    UNITY_BEGIN();
    // next_backoff_ms
    RUN_TEST(test_backoff_n1_thirty_seconds);
    RUN_TEST(test_backoff_n5_eight_minutes);
    RUN_TEST(test_backoff_n6_at_cap);
    RUN_TEST(test_backoff_n100_still_at_cap);
    return UNITY_END();
}
