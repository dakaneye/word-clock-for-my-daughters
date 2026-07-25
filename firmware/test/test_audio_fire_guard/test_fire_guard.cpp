#include <unity.h>
#include "audio/fire_guard.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

// Fixture — Emory's config. 2030 = first birthday we'd auto-fire.
static constexpr BirthConfig EMORY = {
    /*month=*/10, /*day=*/6, /*hour=*/18, /*minute=*/10
};

static NowFields make_now(uint16_t y, uint8_t m, uint8_t d,
                          uint8_t h, uint8_t mm) {
    return NowFields{y, m, d, h, mm};
}

void test_fire_happy_path(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2029, /*time_known=*/true,
        /*already_playing=*/false));
}

void test_fire_wrong_month(void) {
    NowFields n = make_now(2030, 11, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_day(void) {
    NowFields n = make_now(2030, 10, 7, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_hour(void) {
    NowFields n = make_now(2030, 10, 6, 17, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_wrong_minute(void) {
    NowFields n = make_now(2030, 10, 6, 18, 11);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true, false));
}

void test_fire_already_fired_this_year(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2030, true, false));
}

void test_fire_unknown_time(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029,
        /*time_known=*/false, false));
}

void test_fire_already_playing(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY, 2029, true,
        /*already_playing=*/true));
}

// --- Multi-year re-fire (the keepsake promise: fires every birthday) ---

// Year Y, last_fired_year < Y, at the birth minute -> fires.
void test_fire_first_year_2030(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2029, true, false));
}

// Same year after firing (last_fired_year == Y) -> suppressed for rest of Y.
void test_fire_suppressed_rest_of_year(void) {
    NowFields n = make_now(2030, 10, 6, 18, 10);
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2030, true, false));
}

// Year Y+1 at the birth minute, last_fired_year still Y -> re-fires next year.
void test_fire_refires_next_year_2031(void) {
    NowFields n = make_now(2031, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2030, true, false));
}

// Far end of the 40-year horizon: year 2069 fires, suppressed once fired.
void test_fire_horizon_2069(void) {
    NowFields n = make_now(2069, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2068, true, false));
    TEST_ASSERT_FALSE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2069, true, false));
}

// And re-fires the following year (2070) with last_fired_year lagging at 2069.
void test_fire_horizon_refires_2070(void) {
    NowFields n = make_now(2070, 10, 6, 18, 10);
    TEST_ASSERT_TRUE(should_auto_fire(n, EMORY,
        /*last_fired_year=*/2069, true, false));
}

// is_birthday: the Audio-button day gate. True any time on the birthday
// (month+day), independent of the birth hour:minute.
void test_is_birthday_any_time_of_day(void) {
    TEST_ASSERT_TRUE(is_birthday(make_now(2030, 10, 6, 7, 0),  EMORY, true));
    TEST_ASSERT_TRUE(is_birthday(make_now(2030, 10, 6, 23, 59), EMORY, true));
    TEST_ASSERT_TRUE(is_birthday(make_now(2030, 10, 6, 18, 10), EMORY, true));
}

void test_is_birthday_wrong_date(void) {
    TEST_ASSERT_FALSE(is_birthday(make_now(2030, 10, 5, 12, 0), EMORY, true));
    TEST_ASSERT_FALSE(is_birthday(make_now(2030, 9, 6, 12, 0),  EMORY, true));
}

// An unsynced RTC could read any date; without known time the button
// must fall back to lullabies, never false-fire the birthday message.
void test_is_birthday_requires_known_time(void) {
    TEST_ASSERT_FALSE(is_birthday(make_now(2030, 10, 6, 12, 0), EMORY, false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fire_happy_path);
    RUN_TEST(test_fire_wrong_month);
    RUN_TEST(test_fire_wrong_day);
    RUN_TEST(test_fire_wrong_hour);
    RUN_TEST(test_fire_wrong_minute);
    RUN_TEST(test_fire_already_fired_this_year);
    RUN_TEST(test_fire_unknown_time);
    RUN_TEST(test_fire_already_playing);
    RUN_TEST(test_fire_first_year_2030);
    RUN_TEST(test_fire_suppressed_rest_of_year);
    RUN_TEST(test_fire_refires_next_year_2031);
    RUN_TEST(test_fire_horizon_2069);
    RUN_TEST(test_fire_horizon_refires_2070);
    RUN_TEST(test_is_birthday_any_time_of_day);
    RUN_TEST(test_is_birthday_wrong_date);
    RUN_TEST(test_is_birthday_requires_known_time);
    return UNITY_END();
}
