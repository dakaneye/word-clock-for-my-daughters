#include <unity.h>
#include "rtc/advance.h"

using namespace wc::rtc;

void setUp(void) {}
void tearDown(void) {}

// Helper for concise equality assertions on a DateTime.
static void assert_dt_equal(const DateTime& expected,
                             const DateTime& actual) {
    TEST_ASSERT_EQUAL_UINT16(expected.year,   actual.year);
    TEST_ASSERT_EQUAL_UINT8 (expected.month,  actual.month);
    TEST_ASSERT_EQUAL_UINT8 (expected.day,    actual.day);
    TEST_ASSERT_EQUAL_UINT8 (expected.hour,   actual.hour);
    TEST_ASSERT_EQUAL_UINT8 (expected.minute, actual.minute);
    TEST_ASSERT_EQUAL_UINT8 (expected.second, actual.second);
}

// -- advance_hour_fields -------------------------------------------

void test_advance_hour_increments_by_one(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 45},
        advance_hour_fields({2030, 5, 15, 14, 23, 45}));
}

void test_advance_hour_wraps_23_to_0(void) {
    assert_dt_equal({2030, 5, 15, 0, 23, 45},
        advance_hour_fields({2030, 5, 15, 23, 23, 45}));
}

void test_advance_hour_no_day_carry_on_new_years_eve(void) {
    assert_dt_equal({2030, 12, 31, 0, 23, 45},
        advance_hour_fields({2030, 12, 31, 23, 23, 45}));
}

void test_advance_hour_preserves_second_at_zero(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 0},
        advance_hour_fields({2030, 5, 15, 14, 23, 0}));
}

void test_advance_hour_preserves_second_at_59(void) {
    assert_dt_equal({2030, 5, 15, 15, 23, 59},
        advance_hour_fields({2030, 5, 15, 14, 23, 59}));
}

// -- advance_minute_fields -----------------------------------------

void test_advance_minute_adds_5_from_aligned(void) {
    assert_dt_equal({2030, 5, 15, 14, 30, 0},
        advance_minute_fields({2030, 5, 15, 14, 25, 45}));
}

void test_advance_minute_adds_5_then_floors(void) {
    // 23 + 5 = 28 → floor(28/5)*5 = 25.
    assert_dt_equal({2030, 5, 15, 14, 25, 0},
        advance_minute_fields({2030, 5, 15, 14, 23, 45}));
}

void test_advance_minute_at_55_carries_to_hour(void) {
    assert_dt_equal({2030, 5, 15, 15, 0, 0},
        advance_minute_fields({2030, 5, 15, 14, 55, 45}));
}

void test_advance_minute_at_23_55_wraps_hour_and_stays_on_same_day(void) {
    assert_dt_equal({2030, 5, 15, 0, 0, 0},
        advance_minute_fields({2030, 5, 15, 23, 55, 45}));
}

void test_advance_minute_at_57_carries_past_60_and_floors(void) {
    // 57 + 5 = 62 → 62 - 60 = 2 → floor(2/5)*5 = 0. Hour carries.
    assert_dt_equal({2030, 5, 15, 15, 0, 0},
        advance_minute_fields({2030, 5, 15, 14, 57, 45}));
}

void test_advance_minute_preserves_year_month_day_at_calendar_edge(void) {
    assert_dt_equal({2030, 12, 31, 14, 30, 0},
        advance_minute_fields({2030, 12, 31, 14, 25, 45}));
}

int main(int, char**) {
    UNITY_BEGIN();
    // advance_hour_fields
    RUN_TEST(test_advance_hour_increments_by_one);
    RUN_TEST(test_advance_hour_wraps_23_to_0);
    RUN_TEST(test_advance_hour_no_day_carry_on_new_years_eve);
    RUN_TEST(test_advance_hour_preserves_second_at_zero);
    RUN_TEST(test_advance_hour_preserves_second_at_59);
    // advance_minute_fields
    RUN_TEST(test_advance_minute_adds_5_from_aligned);
    RUN_TEST(test_advance_minute_adds_5_then_floors);
    RUN_TEST(test_advance_minute_at_55_carries_to_hour);
    RUN_TEST(test_advance_minute_at_23_55_wraps_hour_and_stays_on_same_day);
    RUN_TEST(test_advance_minute_at_57_carries_past_60_and_floors);
    RUN_TEST(test_advance_minute_preserves_year_month_day_at_calendar_edge);
    return UNITY_END();
}
