#include <unity.h>
#include "birthday.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static constexpr BirthdayConfig EMORY = {10, 6, 18, 10};  // Oct 6, 6:10 PM
static constexpr BirthdayConfig NORA  = {3, 19, 9, 17};   // Mar 19, 9:17 AM

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
    RUN_TEST(test_nora_separate);
    return UNITY_END();
}
