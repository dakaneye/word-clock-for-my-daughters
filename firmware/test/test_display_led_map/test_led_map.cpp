#include <unity.h>

#include <array>

#include "display/led_map.h"
#include "display/rgb.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_first_word_starts_at_zero(void) {
    const LedSpan s = span_of(WordId::IT);
    TEST_ASSERT_EQUAL_UINT8(0, s.start);
    TEST_ASSERT_EQUAL_UINT8(1, s.count);
    TEST_ASSERT_EQUAL_UINT8(0, index_of(WordId::IT));
}

void test_multi_led_word_spans(void) {
    // Long words span several adjacent LEDs so they light evenly.
    // Source of truth: LED_QUARTER/_1/_2, LED_MORNING/_1/_2 footprints
    // plus the data-line chain order in word-clock.kicad_pcb.
    const LedSpan q = span_of(WordId::QUARTER);
    TEST_ASSERT_EQUAL_UINT8(5, q.start);
    TEST_ASSERT_EQUAL_UINT8(3, q.count);

    const LedSpan m = span_of(WordId::MORNING);
    TEST_ASSERT_EQUAL_UINT8(51, m.start);
    TEST_ASSERT_EQUAL_UINT8(3, m.count);
}

void test_chain_tail_words(void) {
    // The PCB chain order is NOT WordId order; these pin the tail of the
    // 63-LED chain so a future PCB re-route is caught here, not at runtime.
    const LedSpan at = span_of(WordId::AT);
    TEST_ASSERT_EQUAL_UINT8(57, at.start);
    TEST_ASSERT_EQUAL_UINT8(1, at.count);

    const LedSpan name = span_of(WordId::NAME);
    TEST_ASSERT_EQUAL_UINT8(40, name.start);
    TEST_ASSERT_EQUAL_UINT8(2, name.count);
}

// The 35 word spans must tile [0, LED_COUNT) exactly: every LED in the
// chain belongs to exactly one word — no gaps, no overlaps.
void test_spans_partition_the_chain(void) {
    std::array<uint8_t, LED_COUNT> covered{};
    uint32_t total = 0;

    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const LedSpan s = span_of(static_cast<WordId>(i));
        TEST_ASSERT_GREATER_THAN_UINT8(0, s.count);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16(
            LED_COUNT, static_cast<uint16_t>(s.start) + s.count);
        for (uint8_t k = 0; k < s.count; ++k) {
            covered[s.start + k]++;
        }
        total += s.count;
    }

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(
            1, covered[i], "LED is unmapped (gap) or double-mapped (overlap)");
    }
    TEST_ASSERT_EQUAL_UINT32(LED_COUNT, total);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_word_starts_at_zero);
    RUN_TEST(test_multi_led_word_spans);
    RUN_TEST(test_chain_tail_words);
    RUN_TEST(test_spans_partition_the_chain);
    return UNITY_END();
}
