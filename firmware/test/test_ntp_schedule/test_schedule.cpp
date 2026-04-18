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

// -- next_deadline_after_success ----------------------------------

// jitter_sample = 0 -> jitter offset = -30 min.
void test_deadline_jitter_zero_minus_30min(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint64_t THIRTY_MIN_MS = 1'800'000ULL;
    TEST_ASSERT_EQUAL_UINT64(T0 + TWENTY_FOUR_HOURS_MS - THIRTY_MIN_MS,
        next_deadline_after_success(T0, 0u));
}

// jitter_sample = 1'800'000 -> jitter offset = 0 (24h on the dot).
void test_deadline_jitter_mid_no_offset(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    TEST_ASSERT_EQUAL_UINT64(T0 + TWENTY_FOUR_HOURS_MS,
        next_deadline_after_success(T0, 1'800'000u));
}

// jitter_sample = 3'599'999 -> jitter offset = +30 min - 1 ms.
void test_deadline_jitter_max_plus_30min_minus_1ms(void) {
    constexpr uint64_t T0 = 1'000'000ULL;
    constexpr uint64_t TWENTY_FOUR_HOURS_MS = 86'400'000ULL;
    constexpr uint64_t THIRTY_MIN_MS = 1'800'000ULL;
    constexpr uint64_t ONE_MS = 1ULL;
    TEST_ASSERT_EQUAL_UINT64(
        T0 + TWENTY_FOUR_HOURS_MS + THIRTY_MIN_MS - ONE_MS,
        next_deadline_after_success(T0, 3'599'999u));
}

int main(int, char**) {
    UNITY_BEGIN();
    // next_backoff_ms
    RUN_TEST(test_backoff_n1_thirty_seconds);
    RUN_TEST(test_backoff_n5_eight_minutes);
    RUN_TEST(test_backoff_n6_at_cap);
    RUN_TEST(test_backoff_n100_still_at_cap);
    // next_deadline_after_success
    RUN_TEST(test_deadline_jitter_zero_minus_30min);
    RUN_TEST(test_deadline_jitter_mid_no_offset);
    RUN_TEST(test_deadline_jitter_max_plus_30min_minus_1ms);
    return UNITY_END();
}
