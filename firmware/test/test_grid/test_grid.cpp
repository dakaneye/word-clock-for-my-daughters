#include <initializer_list>
#include <unity.h>
#include "grid.h"

using namespace wc;

void setUp(void) {}
void tearDown(void) {}

void test_emory_and_nora_are_distinct(void) {
    const Grid& e = get(ClockTarget::EMORY);
    const Grid& n = get(ClockTarget::NORA);
    TEST_ASSERT_NOT_EQUAL(&e, &n);
}

void test_emory_row0_word_spans(void) {
    const Grid& g = get(ClockTarget::EMORY);
    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IT)].row);
    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IT)].col);
    TEST_ASSERT_EQUAL(2, g.spans[static_cast<uint8_t>(WordId::IT)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::IS)].row);
    TEST_ASSERT_EQUAL(3, g.spans[static_cast<uint8_t>(WordId::IS)].col);
    TEST_ASSERT_EQUAL(2, g.spans[static_cast<uint8_t>(WordId::IS)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].row);
    TEST_ASSERT_EQUAL(6, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].col);
    TEST_ASSERT_EQUAL(3, g.spans[static_cast<uint8_t>(WordId::TEN_MIN)].length);

    TEST_ASSERT_EQUAL(0, g.spans[static_cast<uint8_t>(WordId::HALF)].row);
    TEST_ASSERT_EQUAL(9, g.spans[static_cast<uint8_t>(WordId::HALF)].col);
    TEST_ASSERT_EQUAL(4, g.spans[static_cast<uint8_t>(WordId::HALF)].length);
}

void test_emory_word_letters_match_word(void) {
    const Grid& g = get(ClockTarget::EMORY);
    auto check = [&](WordId id, const char* expected) {
        const CellSpan& s = g.spans[static_cast<uint8_t>(id)];
        for (uint8_t i = 0; i < s.length; ++i) {
            TEST_ASSERT_EQUAL_CHAR(expected[i], g.letters[s.row * 13 + s.col + i]);
        }
    };
    check(WordId::IT, "IT");
    check(WordId::IS, "IS");
    check(WordId::TEN_MIN, "TEN");
    check(WordId::HALF, "HALF");
    check(WordId::QUARTER, "QUARTER");
    check(WordId::TWENTY, "TWENTY");
    check(WordId::FIVE_MIN, "FIVE");
    check(WordId::MINUTES, "MINUTES");
    check(WordId::HAPPY, "HAPPY");
    check(WordId::BIRTH, "BIRTH");
    check(WordId::DAY, "DAY");
    check(WordId::NAME, "EMORY");
    check(WordId::NOON, "NOON");
    check(WordId::AFTERNOON, "AFTERNOON");
    check(WordId::EVENING, "EVENING");
    check(WordId::NIGHT, "NIGHT");
    check(WordId::MORNING, "MORNING");
}

void test_nora_name_is_nora(void) {
    const Grid& g = get(ClockTarget::NORA);
    const CellSpan& s = g.spans[static_cast<uint8_t>(WordId::NAME)];
    TEST_ASSERT_EQUAL(4, s.length);
    for (uint8_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_CHAR("NORA"[i], g.letters[s.row * 13 + s.col + i]);
    }
}

void test_every_word_has_a_span(void) {
    for (auto target : {ClockTarget::EMORY, ClockTarget::NORA}) {
        const Grid& g = get(target);
        for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
            TEST_ASSERT_TRUE_MESSAGE(
                g.spans[i].length > 0,
                "every WordId must have a non-empty span");
        }
    }
}

void test_emory_filler_acrostic(void) {
    const Grid& g = get(ClockTarget::EMORY);
    TEST_ASSERT_EQUAL(12, g.filler_count);
    char spelled[13] = {0};
    for (uint8_t i = 0; i < g.filler_count; ++i) {
        const CellSpan& f = g.fillers[i];
        spelled[i] = g.letters[f.row * 13 + f.col];
    }
    TEST_ASSERT_EQUAL_STRING("EMORY1062023", spelled);
}

void test_nora_filler_acrostic(void) {
    const Grid& g = get(ClockTarget::NORA);
    TEST_ASSERT_EQUAL(13, g.filler_count);
    char spelled[14] = {0};
    for (uint8_t i = 0; i < g.filler_count; ++i) {
        const CellSpan& f = g.fillers[i];
        spelled[i] = g.letters[f.row * 13 + f.col];
    }
    TEST_ASSERT_EQUAL_STRING("NORAMAR192025", spelled);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_emory_and_nora_are_distinct);
    RUN_TEST(test_emory_row0_word_spans);
    RUN_TEST(test_emory_word_letters_match_word);
    RUN_TEST(test_nora_name_is_nora);
    RUN_TEST(test_every_word_has_a_span);
    RUN_TEST(test_emory_filler_acrostic);
    RUN_TEST(test_nora_filler_acrostic);
    return UNITY_END();
}
