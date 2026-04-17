#include <unity.h>
#include "display/palette.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

void test_warm_white_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, warm_white().r);
    TEST_ASSERT_EQUAL_UINT8(170, warm_white().g);
    TEST_ASSERT_EQUAL_UINT8(100, warm_white().b);
}

void test_amber_rgb(void) {
    TEST_ASSERT_EQUAL_UINT8(255, amber().r);
    TEST_ASSERT_EQUAL_UINT8(120, amber().g);
    TEST_ASSERT_EQUAL_UINT8(30,  amber().b);
}

void test_palette_warm_white_is_warm_white_for_all_words(void) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const Rgb c = color_for(Palette::WARM_WHITE, static_cast<WordId>(i));
        TEST_ASSERT_EQUAL_UINT8(255, c.r);
        TEST_ASSERT_EQUAL_UINT8(170, c.g);
        TEST_ASSERT_EQUAL_UINT8(100, c.b);
    }
}

void test_palette_halloween_alternates_orange_purple_by_enum_idx(void) {
    // IT=0 → orange; IS=1 → purple. (enum idx parity decides.)
    const Rgb orange = color_for(Palette::HALLOWEEN, WordId::IT);
    const Rgb purple = color_for(Palette::HALLOWEEN, WordId::IS);
    TEST_ASSERT_EQUAL_UINT8(255, orange.r);
    TEST_ASSERT_EQUAL_UINT8(100, orange.g);
    TEST_ASSERT_EQUAL_UINT8(  0, orange.b);
    TEST_ASSERT_EQUAL_UINT8(140, purple.r);
    TEST_ASSERT_EQUAL_UINT8(  0, purple.g);
    TEST_ASSERT_EQUAL_UINT8(180, purple.b);
}

void test_palette_christmas_alternates_red_green(void) {
    const Rgb red   = color_for(Palette::CHRISTMAS, WordId::IT);  // idx 0
    const Rgb green = color_for(Palette::CHRISTMAS, WordId::IS);  // idx 1
    TEST_ASSERT_EQUAL_UINT8(220, red.r);
    TEST_ASSERT_EQUAL_UINT8( 30, red.g);
    TEST_ASSERT_EQUAL_UINT8( 30, red.b);
    TEST_ASSERT_EQUAL_UINT8( 30, green.r);
    TEST_ASSERT_EQUAL_UINT8(160, green.g);
    TEST_ASSERT_EQUAL_UINT8( 60, green.b);
}

void test_palette_valentines_alternates_red_pink(void) {
    const Rgb red  = color_for(Palette::VALENTINES, WordId::IT);
    const Rgb pink = color_for(Palette::VALENTINES, WordId::IS);
    TEST_ASSERT_EQUAL_UINT8(255, red.r);
    TEST_ASSERT_EQUAL_UINT8( 30, red.g);
    TEST_ASSERT_EQUAL_UINT8( 60, red.b);
    TEST_ASSERT_EQUAL_UINT8(255, pink.r);
    TEST_ASSERT_EQUAL_UINT8(120, pink.g);
    TEST_ASSERT_EQUAL_UINT8(160, pink.b);
}

void test_palette_juneteenth_cycles_three_colors(void) {
    const Rgb r0 = color_for(Palette::JUNETEENTH, static_cast<WordId>(0));
    const Rgb r1 = color_for(Palette::JUNETEENTH, static_cast<WordId>(1));
    const Rgb r2 = color_for(Palette::JUNETEENTH, static_cast<WordId>(2));
    const Rgb r3 = color_for(Palette::JUNETEENTH, static_cast<WordId>(3));
    TEST_ASSERT_EQUAL_UINT8(200, r0.r);   // red
    TEST_ASSERT_EQUAL_UINT8( 10, r1.r);   // black
    TEST_ASSERT_EQUAL_UINT8( 30, r2.r);   // green
    TEST_ASSERT_EQUAL_UINT8(200, r3.r);   // wrap → red
}

void test_palette_easter_cycles_four_colors(void) {
    const Rgb r0 = color_for(Palette::EASTER_PASTEL, static_cast<WordId>(0));
    const Rgb r4 = color_for(Palette::EASTER_PASTEL, static_cast<WordId>(4));
    TEST_ASSERT_EQUAL_UINT8(255, r0.r);
    TEST_ASSERT_EQUAL_UINT8(180, r0.g);
    TEST_ASSERT_EQUAL_UINT8(200, r0.b);
    TEST_ASSERT_EQUAL_UINT8(255, r4.r);
    TEST_ASSERT_EQUAL_UINT8(180, r4.g);
    TEST_ASSERT_EQUAL_UINT8(200, r4.b);
}

void test_palette_power_budget(void) {
    const Palette all_palettes[] = {
        Palette::WARM_WHITE,
        Palette::MLK_PURPLE,
        Palette::VALENTINES,
        Palette::WOMEN_PURPLE,
        Palette::EARTH_DAY,
        Palette::EASTER_PASTEL,
        Palette::JUNETEENTH,
        Palette::INDIGENOUS,
        Palette::HALLOWEEN,
        Palette::CHRISTMAS,
    };
    for (Palette p : all_palettes) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
            const WordId w = static_cast<WordId>(i);
            const Rgb c = color_for(p, w);
            const uint16_t sum = static_cast<uint16_t>(c.r) +
                                 static_cast<uint16_t>(c.g) +
                                 static_cast<uint16_t>(c.b);
            TEST_ASSERT_LESS_OR_EQUAL_UINT16(PALETTE_MAX_RGB_SUM, sum);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_warm_white_rgb);
    RUN_TEST(test_amber_rgb);
    RUN_TEST(test_palette_warm_white_is_warm_white_for_all_words);
    RUN_TEST(test_palette_halloween_alternates_orange_purple_by_enum_idx);
    RUN_TEST(test_palette_christmas_alternates_red_green);
    RUN_TEST(test_palette_valentines_alternates_red_pink);
    RUN_TEST(test_palette_juneteenth_cycles_three_colors);
    RUN_TEST(test_palette_easter_cycles_four_colors);
    RUN_TEST(test_palette_power_budget);
    return UNITY_END();
}
