#include <unity.h>
#include "holidays.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

void test_default_is_warm_white(void) {
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 4, 15));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 7, 4));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 11, 1));
}

void test_fixed_dates(void) {
    TEST_ASSERT_EQUAL(Palette::VALENTINES,   palette_for_date(2030, 2, 14));
    TEST_ASSERT_EQUAL(Palette::WOMEN_PURPLE, palette_for_date(2030, 3, 8));
    TEST_ASSERT_EQUAL(Palette::EARTH_DAY,    palette_for_date(2030, 4, 22));
    TEST_ASSERT_EQUAL(Palette::JUNETEENTH,   palette_for_date(2030, 6, 19));
    TEST_ASSERT_EQUAL(Palette::HALLOWEEN,    palette_for_date(2030, 10, 31));
    TEST_ASSERT_EQUAL(Palette::CHRISTMAS,    palette_for_date(2030, 12, 25));
}

void test_mlk_day_third_monday_january(void) {
    // MLK Day is 3rd Monday of January.
    // 2030-01-21, 2031-01-20, 2032-01-19.
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2030, 1, 21));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2031, 1, 20));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2032, 1, 19));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 1, 14)); // 2nd Mon
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 1, 28)); // 4th Mon
}

void test_indigenous_peoples_day_second_monday_october(void) {
    // 2030-10-14, 2031-10-13.
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2030, 10, 14));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2031, 10, 13));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 10, 7));  // 1st Mon
}

void test_easter_computed(void) {
    // Western Easter dates (Gregorian): verified via Meeus/Jones/Butcher algorithm.
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2030, 4, 21));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2031, 4, 13));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2032, 3, 28));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2033, 4, 17));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2034, 4, 9));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2035, 3, 25));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2030, 4, 20));
}

void test_mlk_day_across_horizon(void) {
    // 3rd Monday of January across the 40-year design horizon.
    // Dates derived from the weekday rule and cross-checked against a real calendar.
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2030, 1, 21));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2040, 1, 16));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2050, 1, 17));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2060, 1, 19));
    TEST_ASSERT_EQUAL(Palette::MLK_PURPLE, palette_for_date(2070, 1, 20));
    // Day before each is not MLK Day.
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2040, 1, 15));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2070, 1, 19));
}

void test_indigenous_peoples_day_across_horizon(void) {
    // 2nd Monday of October across the 40-year design horizon.
    // Dates derived from the weekday rule and cross-checked against a real calendar.
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2030, 10, 14));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2040, 10, 8));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2050, 10, 10));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2060, 10, 11));
    TEST_ASSERT_EQUAL(Palette::INDIGENOUS, palette_for_date(2070, 10, 13));
    // Week earlier (1st Monday) is not IPD.
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2040, 10, 1));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2070, 10, 6));
}

void test_easter_across_horizon(void) {
    // Western (Gregorian) Easter across the 40-year design horizon, computed via
    // Meeus/Jones/Butcher and cross-checked against a real calendar.
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2030, 4, 21));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2040, 4, 1));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2050, 4, 10));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2060, 4, 18));
    TEST_ASSERT_EQUAL(Palette::EASTER_PASTEL, palette_for_date(2070, 3, 30));
    // Day after each Easter is back to default.
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2040, 4, 2));
    TEST_ASSERT_EQUAL(Palette::WARM_WHITE, palette_for_date(2070, 3, 31));
}

void test_easter_earth_day_collision_earth_day_wins(void) {
    // In 2057 and 2068 the computed Gregorian Easter lands on April 22, which is
    // also the fixed Earth Day. Fixed-date holidays are checked before Easter in
    // palette_for_date, so Earth Day must win the priority tie.
    TEST_ASSERT_EQUAL(Palette::EARTH_DAY, palette_for_date(2057, 4, 22));
    TEST_ASSERT_EQUAL(Palette::EARTH_DAY, palette_for_date(2068, 4, 22));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_warm_white);
    RUN_TEST(test_fixed_dates);
    RUN_TEST(test_mlk_day_third_monday_january);
    RUN_TEST(test_indigenous_peoples_day_second_monday_october);
    RUN_TEST(test_easter_computed);
    RUN_TEST(test_mlk_day_across_horizon);
    RUN_TEST(test_indigenous_peoples_day_across_horizon);
    RUN_TEST(test_easter_across_horizon);
    RUN_TEST(test_easter_earth_day_collision_earth_day_wins);
    return UNITY_END();
}
