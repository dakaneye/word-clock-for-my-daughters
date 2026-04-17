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

// Malformed %XX tolerance: %X at end-of-string, %<non-hex>, truncated %.
// Header contract says "Malformed %XX yields the literal chars."
void test_malformed_percent_truncated(void) {
    // Trailing %X with only one char after the %.
    FormBody body = parse_form_body("ssid=ab%4&tz=UTC0");
    // Expectation: the %4 is passed through as literal "%4" since there's
    // no full %XX pair. Exact output isn't the point; no crash + survives
    // to parse subsequent fields.
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
}

void test_malformed_percent_nonhex(void) {
    FormBody body = parse_form_body("ssid=a%GGb&tz=UTC0");
    // %GG is not a valid hex escape; pass through literal.
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
    // The ssid field decodes without crashing; behavior is "best effort
    // pass-through" per header contract.
}

void test_percent_at_very_end(void) {
    FormBody body = parse_form_body("ssid=abc%");
    // Trailing bare % — bounds check prevents OOB read.
    // We don't pin exact output; we assert no crash and the earlier chars
    // survived.
    TEST_ASSERT_TRUE(body.ssid.find("abc") != std::string::npos);
}

void test_trailing_ampersand(void) {
    FormBody body = parse_form_body("ssid=X&pw=Y&tz=UTC0&");
    TEST_ASSERT_EQUAL_STRING("X", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Y", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
}

void test_duplicate_key_last_wins(void) {
    FormBody body = parse_form_body("ssid=first&ssid=second&tz=UTC0");
    // Documents current behavior: later value overwrites earlier.
    TEST_ASSERT_EQUAL_STRING("second", body.ssid.c_str());
}

void test_unknown_key_ignored(void) {
    FormBody body = parse_form_body("foo=bar&ssid=X&baz=qux&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("X", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_simple_body);
    RUN_TEST(test_decodes_plus_as_space);
    RUN_TEST(test_decodes_percent_escapes);
    RUN_TEST(test_missing_field_is_empty);
    RUN_TEST(test_empty_body_yields_empty_fields);
    RUN_TEST(test_extracts_csrf_token);
    RUN_TEST(test_malformed_percent_truncated);
    RUN_TEST(test_malformed_percent_nonhex);
    RUN_TEST(test_percent_at_very_end);
    RUN_TEST(test_trailing_ampersand);
    RUN_TEST(test_duplicate_key_last_wins);
    RUN_TEST(test_unknown_key_ignored);
    return UNITY_END();
}
