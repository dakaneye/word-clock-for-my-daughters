#include <unity.h>
#include "wifi_provision/tz_options.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_has_eight_options(void) {
    auto opts = tz_options();
    TEST_ASSERT_EQUAL(8, opts.size());
}

void test_pacific_is_first(void) {
    auto opts = tz_options();
    TEST_ASSERT_EQUAL_STRING("PST8PDT,M3.2.0,M11.1.0", opts[0].posix.c_str());
}

void test_arizona_has_no_dst(void) {
    auto opts = tz_options();
    for (const auto& opt : opts) {
        if (opt.label.find("Arizona") != std::string::npos) {
            TEST_ASSERT_EQUAL_STRING("MST7", opt.posix.c_str());
            return;
        }
    }
    TEST_FAIL_MESSAGE("Arizona not found in options");
}

void test_is_known_posix_tz_recognizes_all(void) {
    for (const auto& opt : tz_options()) {
        TEST_ASSERT_TRUE_MESSAGE(is_known_posix_tz(opt.posix), opt.posix.c_str());
    }
}

void test_is_known_posix_tz_rejects_garbage(void) {
    TEST_ASSERT_FALSE(is_known_posix_tz("not_a_tz"));
    TEST_ASSERT_FALSE(is_known_posix_tz(""));
    TEST_ASSERT_FALSE(is_known_posix_tz("America/Los_Angeles"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_has_eight_options);
    RUN_TEST(test_pacific_is_first);
    RUN_TEST(test_arizona_has_no_dst);
    RUN_TEST(test_is_known_posix_tz_recognizes_all);
    RUN_TEST(test_is_known_posix_tz_rejects_garbage);
    return UNITY_END();
}
