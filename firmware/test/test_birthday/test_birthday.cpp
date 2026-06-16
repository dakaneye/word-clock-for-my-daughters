#include <unity.h>
#include "birthday.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static constexpr BirthdayConfig EMORY = {10, 6, 18, 10};  // Oct 6, 6:10 PM
static constexpr BirthdayConfig NORA  = {3, 19, 9, 17};   // Mar 19, 9:17 AM
static constexpr BirthdayConfig NEWYEAR = {1, 1, 0, 0};   // Jan 1, midnight (month-boundary)

void test_not_birthday(void) {
    BirthdayState s = birthday_state(4, 15, 12, 0, EMORY);
    TEST_ASSERT_FALSE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

void test_is_birthday_all_day(void) {
    BirthdayState s1 = birthday_state(10, 6, 0,  0,  EMORY);
    BirthdayState s2 = birthday_state(10, 6, 12, 0,  EMORY);
    BirthdayState s3 = birthday_state(10, 6, 23, 59, EMORY);
    TEST_ASSERT_TRUE(s1.is_birthday);
    TEST_ASSERT_TRUE(s2.is_birthday);
    TEST_ASSERT_TRUE(s3.is_birthday);
}

void test_birth_minute_exact(void) {
    BirthdayState s = birthday_state(10, 6, 18, 10, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_TRUE(s.is_birth_minute);
}

void test_birth_minute_wrong_minute(void) {
    BirthdayState s = birthday_state(10, 6, 18, 11, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

void test_birth_minute_wrong_hour(void) {
    BirthdayState s = birthday_state(10, 6, 19, 10, EMORY);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

// Decor path: the day BEFORE the birthday must not light the birthday decor.
void test_day_before_not_birthday(void) {
    BirthdayState s = birthday_state(10, 5, 18, 10, EMORY);  // Oct 5, even at birth time
    TEST_ASSERT_FALSE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

// Decor path: the day AFTER the birthday must not light the birthday decor.
void test_day_after_not_birthday(void) {
    BirthdayState s = birthday_state(10, 7, 18, 10, EMORY);  // Oct 7, even at birth time
    TEST_ASSERT_FALSE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

// One minute BEFORE the configured birth minute is not the birth minute.
void test_birth_minute_one_before(void) {
    BirthdayState s = birthday_state(10, 6, 18, 9, EMORY);  // 18:09
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

// Month-boundary birthday: day before crosses into the prior month.
void test_month_boundary_day_before(void) {
    BirthdayState s = birthday_state(12, 31, 0, 0, NEWYEAR);  // Dec 31, prior month
    TEST_ASSERT_FALSE(s.is_birthday);
    TEST_ASSERT_FALSE(s.is_birth_minute);
}

// Month-boundary birthday: exact day and birth minute still resolve.
void test_month_boundary_exact(void) {
    BirthdayState s = birthday_state(1, 1, 0, 0, NEWYEAR);  // Jan 1, midnight
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_TRUE(s.is_birth_minute);

    // Day after (Jan 2) is back to no decor.
    BirthdayState after = birthday_state(1, 2, 0, 0, NEWYEAR);
    TEST_ASSERT_FALSE(after.is_birthday);
    TEST_ASSERT_FALSE(after.is_birth_minute);
}

void test_nora_separate(void) {
    BirthdayState s = birthday_state(3, 19, 9, 17, NORA);
    TEST_ASSERT_TRUE(s.is_birthday);
    TEST_ASSERT_TRUE(s.is_birth_minute);

    s = birthday_state(3, 19, 9, 17, EMORY);
    TEST_ASSERT_FALSE(s.is_birthday);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_not_birthday);
    RUN_TEST(test_is_birthday_all_day);
    RUN_TEST(test_birth_minute_exact);
    RUN_TEST(test_birth_minute_wrong_minute);
    RUN_TEST(test_birth_minute_wrong_hour);
    RUN_TEST(test_day_before_not_birthday);
    RUN_TEST(test_day_after_not_birthday);
    RUN_TEST(test_birth_minute_one_before);
    RUN_TEST(test_month_boundary_day_before);
    RUN_TEST(test_month_boundary_exact);
    RUN_TEST(test_nora_separate);
    return UNITY_END();
}
