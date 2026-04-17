#include <ctime>
#include <unity.h>
#include "rtc/epoch.h"

using namespace wc::rtc;

void setUp(void) {}
void tearDown(void) {}

// Unix epoch starts at 1970-01-01 00:00:00 UTC by definition.
void test_utc_epoch_from_fields_at_unix_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0u,
        utc_epoch_from_fields({1970, 1, 1, 0, 0, 0}));
}

// RTClib's SECONDS_FROM_1970_TO_2000 boundary. Cross-verifiable via
// `date -u -d '2000-01-01 00:00:00' +%s` on any Unix host.
void test_utc_epoch_from_fields_at_2000_01_01(void) {
    TEST_ASSERT_EQUAL_UINT32(946684800u,
        utc_epoch_from_fields({2000, 1, 1, 0, 0, 0}));
}

// 2028 is a leap year — Feb 29 exists. A one-day offset between
// 2028-02-29 00:00:00 and 2028-03-01 00:00:00 must equal 86400 seconds.
void test_utc_epoch_from_fields_across_leap_day(void) {
    uint32_t feb29 = utc_epoch_from_fields({2028, 2, 29, 0, 0, 0});
    uint32_t mar01 = utc_epoch_from_fields({2028, 3, 1, 0, 0, 0});
    TEST_ASSERT_EQUAL_UINT32(86400u, mar01 - feb29);
}

// Emory's birthday minute projected as UTC for arithmetic test.
// `date -j -u -f '%Y-%m-%d %H:%M:%S' '2030-10-06 18:10:00' +%s` = 1917540600.
void test_utc_epoch_from_fields_at_2030_10_06_18_10_00(void) {
    TEST_ASSERT_EQUAL_UINT32(1917540600u,
        utc_epoch_from_fields({2030, 10, 6, 18, 10, 0}));
}

// Cross-check against the host's libc timegm() at 10 arbitrary points
// spanning the daughters' 2026-2100 lifespan. Catches any arithmetic
// bug in days_from_civil against an independently-implemented reference.
void test_utc_epoch_from_fields_round_trips_through_gmtime(void) {
    const DateTime cases[] = {
        {2026,  1,  1,  0,  0,  0},
        {2026, 12, 31, 23, 59, 59},
        {2030,  6, 15, 12,  0,  0},
        {2032,  2, 29,  6, 30, 45},   // leap day
        {2040,  7,  4,  9, 15, 30},
        {2050, 11, 22, 18, 45, 12},
        {2060,  3,  3,  3,  3,  3},
        {2070,  8,  8,  8,  8,  8},
        {2080, 12, 25, 16, 20,  0},
        {2099,  6, 30,  0,  0,  1},
    };
    for (const auto& dt : cases) {
        struct tm t{};
        t.tm_year = dt.year - 1900;
        t.tm_mon  = dt.month - 1;
        t.tm_mday = dt.day;
        t.tm_hour = dt.hour;
        t.tm_min  = dt.minute;
        t.tm_sec  = dt.second;
        time_t expected = timegm(&t);
        TEST_ASSERT_EQUAL_UINT32(
            static_cast<uint32_t>(expected),
            utc_epoch_from_fields(dt));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_utc_epoch_from_fields_at_unix_zero);
    RUN_TEST(test_utc_epoch_from_fields_at_2000_01_01);
    RUN_TEST(test_utc_epoch_from_fields_across_leap_day);
    RUN_TEST(test_utc_epoch_from_fields_at_2030_10_06_18_10_00);
    RUN_TEST(test_utc_epoch_from_fields_round_trips_through_gmtime);
    return UNITY_END();
}
