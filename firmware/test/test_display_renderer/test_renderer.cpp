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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unlit_word_is_black);
    RUN_TEST(test_warm_white_default);
    RUN_TEST(test_holiday_palette_replaces_warm_white);
    RUN_TEST(test_stale_sync_replaces_warm_white_on_time_words);
    RUN_TEST(test_stale_sync_boundary);
    RUN_TEST(test_stale_sync_does_not_override_holiday);
    return UNITY_END();
}
