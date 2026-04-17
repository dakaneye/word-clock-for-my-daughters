#include <unity.h>
#include "display/led_map.h"
#include "display/renderer.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

namespace {

RenderInput make_input() {
    RenderInput in{};
    in.year   = 2027;    // non-holiday, non-birthday year for defaults
    in.month  = 5;
    in.day    = 15;
    in.hour   = 14;      // full-bright window
    in.minute = 0;
    in.now_ms = 0;
    in.seconds_since_sync = 0;  // fresh sync
    in.birthday = BirthdayConfig{10, 6, 18, 10};  // Emory birthday
    return in;
}

// Returns true if `c` is a saturated rainbow-family color: one
// channel ≥ 250, at least one channel ≤ 5.
bool is_rainbow_family(Rgb c) {
    const uint8_t mx = c.r > c.g ? (c.r > c.b ? c.r : c.b)
                                 : (c.g > c.b ? c.g : c.b);
    const uint8_t mn = c.r < c.g ? (c.r < c.b ? c.r : c.b)
                                 : (c.g < c.b ? c.g : c.b);
    return mx >= 250 && mn <= 5;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_unlit_word_is_black(void) {
    const Frame f = render(make_input());
    const Rgb noon = f[index_of(WordId::NOON)];
    TEST_ASSERT_EQUAL_UINT8(0, noon.r);
    TEST_ASSERT_EQUAL_UINT8(0, noon.g);
    TEST_ASSERT_EQUAL_UINT8(0, noon.b);
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        const Rgb c = f[index_of(w)];
        TEST_ASSERT_EQUAL_UINT8(0, c.r);
        TEST_ASSERT_EQUAL_UINT8(0, c.g);
        TEST_ASSERT_EQUAL_UINT8(0, c.b);
    }
}

void test_warm_white_default(void) {
    const Frame f = render(make_input());
    for (WordId w : {WordId::IT, WordId::IS}) {
        const Rgb c = f[index_of(w)];
        TEST_ASSERT_EQUAL_UINT8(255, c.r);
        TEST_ASSERT_EQUAL_UINT8(170, c.g);
        TEST_ASSERT_EQUAL_UINT8(100, c.b);
    }
}

void test_holiday_palette_replaces_warm_white(void) {
    RenderInput in = make_input();
    in.month = 10;   // Halloween
    in.day   = 31;
    in.hour  = 14;
    const Frame f = render(in);
    // IT is enum 0 — Halloween palette[0] = orange {255, 100, 0}
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
    // IS is enum 1 — Halloween palette[1] = purple {140, 0, 180}
    const Rgb is = f[index_of(WordId::IS)];
    TEST_ASSERT_EQUAL_UINT8(140, is.r);
    TEST_ASSERT_EQUAL_UINT8(  0, is.g);
    TEST_ASSERT_EQUAL_UINT8(180, is.b);
}

void test_stale_sync_replaces_warm_white_on_time_words(void) {
    RenderInput in = make_input();
    in.seconds_since_sync = 90'000;   // > 86'400
    const Frame f = render(in);
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(120, it.g);
    TEST_ASSERT_EQUAL_UINT8( 30, it.b);
}

void test_stale_sync_boundary(void) {
    RenderInput in = make_input();
    in.seconds_since_sync = 86'400;    // NOT stale (strict >)
    Frame f = render(in);
    Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(170, it.g);
    TEST_ASSERT_EQUAL_UINT8(100, it.b);

    in.seconds_since_sync = 86'401;    // IS stale
    f = render(in);
    it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(120, it.g);
    TEST_ASSERT_EQUAL_UINT8( 30, it.b);
}

void test_stale_sync_does_not_override_holiday(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 31;
    in.seconds_since_sync = 90'000;  // stale
    const Frame f = render(in);
    // IT still shows Halloween orange, not amber.
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
}

void test_birthday_rainbow_on_decor_only(void) {
    RenderInput in = make_input();
    in.month = 10;                // Emory birthday
    in.day   = 6;
    in.now_ms = 0;
    const Frame f = render(in);
    // Decor words show rainbow family.
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        TEST_ASSERT_TRUE_MESSAGE(is_rainbow_family(f[index_of(w)]),
                                 "decor word should be rainbow-family");
    }
    // Time words show warm white (Oct 6 is not a holiday).
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);
    TEST_ASSERT_EQUAL_UINT8(170, it.g);
    TEST_ASSERT_EQUAL_UINT8(100, it.b);
}

void test_birthday_rainbow_shifts_with_now_ms(void) {
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 6;

    in.now_ms = 0;
    const Frame f0 = render(in);

    in.now_ms = 15'000;   // 1/4 of rainbow period — HAPPY shifts 90°
    const Frame f15 = render(in);

    // HAPPY decor shifts.
    const Rgb happy0  = f0[index_of(WordId::HAPPY)];
    const Rgb happy15 = f15[index_of(WordId::HAPPY)];
    const bool shifted = (happy0.r != happy15.r) ||
                         (happy0.g != happy15.g) ||
                         (happy0.b != happy15.b);
    TEST_ASSERT_TRUE(shifted);

    // Time words (warm white) are identical across the two frames.
    const Rgb it0  = f0[index_of(WordId::IT)];
    const Rgb it15 = f15[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(it0.r, it15.r);
    TEST_ASSERT_EQUAL_UINT8(it0.g, it15.g);
    TEST_ASSERT_EQUAL_UINT8(it0.b, it15.b);
}

void test_birthday_rainbow_wins_over_holiday_palette(void) {
    // Synthetic BirthdayConfig coinciding with Halloween — neither
    // daughter's real birthday, but exercises precedence.
    RenderInput in = make_input();
    in.month = 10;
    in.day   = 31;
    in.birthday = BirthdayConfig{10, 31, 12, 0};
    const Frame f = render(in);
    // Decor still rainbow despite Halloween palette being active.
    for (WordId w : {WordId::HAPPY, WordId::BIRTH, WordId::DAY, WordId::NAME}) {
        TEST_ASSERT_TRUE(is_rainbow_family(f[index_of(w)]));
    }
    // Time words use Halloween palette, not warm white, not amber.
    const Rgb it = f[index_of(WordId::IT)];
    TEST_ASSERT_EQUAL_UINT8(255, it.r);   // Halloween orange
    TEST_ASSERT_EQUAL_UINT8(100, it.g);
    TEST_ASSERT_EQUAL_UINT8(  0, it.b);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unlit_word_is_black);
    RUN_TEST(test_warm_white_default);
    RUN_TEST(test_holiday_palette_replaces_warm_white);
    RUN_TEST(test_stale_sync_replaces_warm_white_on_time_words);
    RUN_TEST(test_stale_sync_boundary);
    RUN_TEST(test_stale_sync_does_not_override_holiday);
    RUN_TEST(test_birthday_rainbow_on_decor_only);
    RUN_TEST(test_birthday_rainbow_shifts_with_now_ms);
    RUN_TEST(test_birthday_rainbow_wins_over_holiday_palette);
    return UNITY_END();
}
