#include <unity.h>
#include <set>
#include "display/led_map.h"
#include "display/rgb.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_index_of_first_word_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, index_of(WordId::IT));
}

void test_index_of_name_is_23(void) {
    // NAME (enum 34) lands at LED chain index 23 — the key "PCB is
    // not enum order" assertion. See led-map table in the spec.
    TEST_ASSERT_EQUAL_UINT8(23, index_of(WordId::NAME));
}

void test_index_of_at_is_34(void) {
    // AT (enum 26) is D35, the last LED in the chain.
    TEST_ASSERT_EQUAL_UINT8(34, index_of(WordId::AT));
}

void test_all_word_ids_map_to_unique_led_indices(void) {
    std::set<uint8_t> seen;
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const uint8_t idx = index_of(static_cast<WordId>(i));
        TEST_ASSERT_LESS_THAN_UINT8(LED_COUNT, idx);
        TEST_ASSERT_TRUE_MESSAGE(seen.insert(idx).second,
                                 "index_of returned a duplicate LED index");
    }
    TEST_ASSERT_EQUAL_UINT32(LED_COUNT, seen.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_index_of_first_word_is_zero);
    RUN_TEST(test_index_of_name_is_23);
    RUN_TEST(test_index_of_at_is_34);
    RUN_TEST(test_all_word_ids_map_to_unique_led_indices);
    return UNITY_END();
}
