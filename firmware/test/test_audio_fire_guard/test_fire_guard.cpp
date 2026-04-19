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
    return UNITY_END();
}
