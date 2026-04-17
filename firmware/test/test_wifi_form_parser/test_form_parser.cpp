#include <unity.h>
#include "wifi_provision/form_parser.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_parses_simple_body(void) {
    FormBody body = parse_form_body("ssid=HomeWiFi&pw=secret&tz=PST8PDT");
    TEST_ASSERT_EQUAL_STRING("HomeWiFi", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("PST8PDT", body.tz.c_str());
}

void test_decodes_plus_as_space(void) {
    FormBody body = parse_form_body("ssid=Home+WiFi+Network&pw=x&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("Home WiFi Network", body.ssid.c_str());
}

void test_decodes_percent_escapes(void) {
    // %21 = '!', %26 = '&' (inside a field, escaped)
    FormBody body = parse_form_body("ssid=a%21b&pw=p%26q&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("a!b", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("p&q", body.pw.c_str());
}

void test_missing_field_is_empty(void) {
    FormBody body = parse_form_body("ssid=X&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("X", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
}

void test_empty_body_yields_empty_fields(void) {
    FormBody body = parse_form_body("");
    TEST_ASSERT_EQUAL_STRING("", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.tz.c_str());
}

void test_extracts_csrf_token(void) {
    FormBody body = parse_form_body("csrf=abc123&ssid=X&pw=Y&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("abc123", body.csrf.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_simple_body);
    RUN_TEST(test_decodes_plus_as_space);
    RUN_TEST(test_decodes_percent_escapes);
    RUN_TEST(test_missing_field_is_empty);
    RUN_TEST(test_empty_body_yields_empty_fields);
    RUN_TEST(test_extracts_csrf_token);
    return UNITY_END();
}
