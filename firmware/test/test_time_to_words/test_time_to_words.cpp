#include <unity.h>
#include "time_to_words.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

static bool contains(const WordSet& ws, WordId id) {
    for (uint8_t i = 0; i < ws.count; ++i) if (ws.words[i] == id) return true;
    return false;
}

// 09:00 -> "IT IS NINE OCLOCK IN THE MORNING"
void test_nine_oclock_morning(void) {
    WordSet ws = time_to_words(9, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::NINE));
    TEST_ASSERT_TRUE(contains(ws, WordId::OCLOCK));
    TEST_ASSERT_TRUE(contains(ws, WordId::IN));
    TEST_ASSERT_TRUE(contains(ws, WordId::THE));
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
}

// 12:00 -> "IT IS TWELVE NOON" (no OCLOCK, no AT/IN/THE)
void test_twelve_noon(void) {
    WordSet ws = time_to_words(12, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::NOON));
    TEST_ASSERT_FALSE(contains(ws, WordId::OCLOCK));
    TEST_ASSERT_FALSE(contains(ws, WordId::AT));
    TEST_ASSERT_FALSE(contains(ws, WordId::IN));
    TEST_ASSERT_FALSE(contains(ws, WordId::THE));
}

// 00:00 -> "IT IS TWELVE AT NIGHT" (midnight, no OCLOCK)
void test_midnight(void) {
    WordSet ws = time_to_words(0, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::IT));
    TEST_ASSERT_TRUE(contains(ws, WordId::IS));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_FALSE(contains(ws, WordId::OCLOCK));
}

// 14:30 -> "IT IS HALF PAST TWO IN THE AFTERNOON"
void test_half_past_two_afternoon(void) {
    WordSet ws = time_to_words(14, 30);
    TEST_ASSERT_TRUE(contains(ws, WordId::HALF));
    TEST_ASSERT_TRUE(contains(ws, WordId::PAST));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWO));
    TEST_ASSERT_TRUE(contains(ws, WordId::IN));
    TEST_ASSERT_TRUE(contains(ws, WordId::THE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AFTERNOON));
}

// 23:45 -> "IT IS QUARTER TO TWELVE AT NIGHT" (next hour wraps from 11->12)
void test_quarter_to_midnight(void) {
    WordSet ws = time_to_words(23, 45);
    TEST_ASSERT_TRUE(contains(ws, WordId::QUARTER));
    TEST_ASSERT_TRUE(contains(ws, WordId::TO));
    TEST_ASSERT_TRUE(contains(ws, WordId::TWELVE));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
}

// 06:25 -> "IT IS TWENTY FIVE MINUTES PAST SIX IN THE MORNING"
void test_twenty_five_past_six_morning(void) {
    WordSet ws = time_to_words(6, 25);
    TEST_ASSERT_TRUE(contains(ws, WordId::TWENTY));
    TEST_ASSERT_TRUE(contains(ws, WordId::FIVE_MIN));
    TEST_ASSERT_TRUE(contains(ws, WordId::MINUTES));
    TEST_ASSERT_TRUE(contains(ws, WordId::PAST));
    TEST_ASSERT_TRUE(contains(ws, WordId::SIX));
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
}

void test_minute_flooring(void) {
    WordSet a = time_to_words(14, 0);
    WordSet b = time_to_words(14, 2);
    WordSet c = time_to_words(14, 4);
    TEST_ASSERT_EQUAL(a.count, b.count);
    TEST_ASSERT_EQUAL(a.count, c.count);
    for (uint8_t i = 0; i < a.count; ++i) {
        TEST_ASSERT_EQUAL(a.words[i], b.words[i]);
        TEST_ASSERT_EQUAL(a.words[i], c.words[i]);
    }
}

void test_five_pm_is_evening(void) {
    WordSet ws = time_to_words(17, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::EVENING));
    TEST_ASSERT_FALSE(contains(ws, WordId::AFTERNOON));
}

void test_nine_pm_is_night(void) {
    WordSet ws = time_to_words(21, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_TRUE(contains(ws, WordId::AT));
    TEST_ASSERT_FALSE(contains(ws, WordId::EVENING));
}

void test_five_am_is_morning(void) {
    WordSet ws = time_to_words(5, 0);
    TEST_ASSERT_TRUE(contains(ws, WordId::MORNING));
    TEST_ASSERT_FALSE(contains(ws, WordId::NIGHT));
}

void test_late_night_edge(void) {
    WordSet ws = time_to_words(4, 59);
    TEST_ASSERT_TRUE(contains(ws, WordId::NIGHT));
    TEST_ASSERT_FALSE(contains(ws, WordId::MORNING));
}

void test_wordset_bounds(void) {
    for (uint8_t h = 0; h < 24; ++h) {
        for (uint8_t m = 0; m < 60; ++m) {
            WordSet ws = time_to_words(h, m);
            TEST_ASSERT_LESS_OR_EQUAL(12, ws.count);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nine_oclock_morning);
    RUN_TEST(test_twelve_noon);
    RUN_TEST(test_midnight);
    RUN_TEST(test_half_past_two_afternoon);
    RUN_TEST(test_quarter_to_midnight);
    RUN_TEST(test_twenty_five_past_six_morning);
    RUN_TEST(test_minute_flooring);
    RUN_TEST(test_five_pm_is_evening);
    RUN_TEST(test_nine_pm_is_night);
    RUN_TEST(test_five_am_is_morning);
    RUN_TEST(test_late_night_edge);
    RUN_TEST(test_wordset_bounds);
    return UNITY_END();
}
