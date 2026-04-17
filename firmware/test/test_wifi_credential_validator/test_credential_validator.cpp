#include <unity.h>
#include "wifi_provision/credential_validator.h"
#include "wifi_provision/form_parser.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

static FormBody body(const char* ssid, const char* pw, const char* tz) {
    FormBody b;
    b.ssid = ssid;
    b.pw = pw;
    b.tz = tz;
    return b;
}

void test_valid_credentials_pass(void) {
    auto result = validate(body("HomeWiFi", "goodpass", "PST8PDT,M3.2.0,M11.1.0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_empty_ssid_fails(void) {
    auto result = validate(body("", "goodpass", "PST8PDT,M3.2.0,M11.1.0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_empty_password_ok_for_open_network(void) {
    auto result = validate(body("OpenNet", "", "UTC0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_ssid_too_long_fails(void) {
    // WiFi SSID max is 32 chars (IEEE 802.11)
    std::string long_ssid(33, 'X');
    auto result = validate(body(long_ssid.c_str(), "p", "UTC0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_ssid_exactly_32_chars_ok(void) {
    std::string boundary(32, 'X');
    auto result = validate(body(boundary.c_str(), "p", "UTC0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_password_too_long_fails(void) {
    // WPA2 passphrase max is 63 chars; we allow 64 in NVS to account for edge cases
    std::string long_pw(65, 'p');
    auto result = validate(body("SSID", long_pw.c_str(), "UTC0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_password_exactly_64_chars_ok(void) {
    // Boundary: 64 is the documented max (spec comment notes WPA2 is 63 chars;
    // we allow 64 for edge cases). Mirror the SSID 32-char boundary test.
    std::string boundary(64, 'p');
    auto result = validate(body("SSID", boundary.c_str(), "UTC0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_unknown_tz_fails(void) {
    auto result = validate(body("SSID", "pw", "not_a_real_tz"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_empty_tz_fails(void) {
    // Empty TZ is distinct from "string provided but not in the known list".
    // Both must fail validation — tested at the validate() integration point
    // (vs. the tz_options layer which tests is_known_posix_tz("") separately).
    auto result = validate(body("SSID", "pw", ""));
    TEST_ASSERT_FALSE(result.ok);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_credentials_pass);
    RUN_TEST(test_empty_ssid_fails);
    RUN_TEST(test_empty_password_ok_for_open_network);
    RUN_TEST(test_ssid_too_long_fails);
    RUN_TEST(test_ssid_exactly_32_chars_ok);
    RUN_TEST(test_password_too_long_fails);
    RUN_TEST(test_password_exactly_64_chars_ok);
    RUN_TEST(test_unknown_tz_fails);
    RUN_TEST(test_empty_tz_fails);
    return UNITY_END();
}
