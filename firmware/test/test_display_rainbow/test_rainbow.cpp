#include <unity.h>
#include <cstdint>
#include <cstdlib>
#include "display/rainbow.h"

using namespace wc;
using namespace wc::display;

void setUp(void) {}
void tearDown(void) {}

// HAPPY at t=0 has 0° phase offset → red.
void test_rainbow_happy_at_t0_is_red(void) {
    const Rgb c = rainbow(WordId::HAPPY, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.r);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
}

// BIRTH at t=0 has 90° phase offset → yellow-green
// (sector 1 midpoint; G=255, B≈0, R near-half).
void test_rainbow_birth_at_t0_is_yellow_green(void) {
    const Rgb c = rainbow(WordId::BIRTH, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.g);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.b);
    TEST_ASSERT_TRUE_MESSAGE(c.r >= 120 && c.r <= 135,
                             "R should be near the 127.5 midpoint");
}

// DAY at t=0 has 180° phase offset → cyan.
void test_rainbow_day_at_t0_is_cyan(void) {
    const Rgb c = rainbow(WordId::DAY, 0);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.r);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.g);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.b);
}

// NAME at t=0 has 270° phase offset → purple
// (sector 4 midpoint; B=255, G≈0, R near-half).
void test_rainbow_name_at_t0_is_purple(void) {
    const Rgb c = rainbow(WordId::NAME, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(250, c.b);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, c.g);
    TEST_ASSERT_TRUE_MESSAGE(c.r >= 120 && c.r <= 135,
                             "R should be near the 127.5 midpoint");
}

// A full 60 s cycle returns to the same color.
void test_rainbow_period_is_60s(void) {
    const Rgb a = rainbow(WordId::HAPPY, 0);
    const Rgb b = rainbow(WordId::HAPPY, 60'000);
    // Exact equality; period wrap is modular-exact when now_ms hits
    // a whole multiple of RAINBOW_PERIOD_MS.
    TEST_ASSERT_EQUAL_UINT8(a.r, b.r);
    TEST_ASSERT_EQUAL_UINT8(a.g, b.g);
    TEST_ASSERT_EQUAL_UINT8(a.b, b.b);
}

// Across the uint32_t millis rollover boundary (~49 days of uptime),
// rainbow still produces saturated, valid rainbow-family colors — not
// corrupt bytes from a broken modular operation. UINT32_MAX % 60'000
// = 42'295, so a uint32_t-wrapped now_ms lands mid-cycle. Both samples
// should be saturated (one channel >= 250, one channel <= 5) — the
// defining property of a point on the rainbow wheel at full S, V.
void test_rainbow_wraps_across_uint32_max(void) {
    const Rgb before_wrap = rainbow(WordId::HAPPY, UINT32_MAX - 1'000);
    const Rgb after_wrap  = rainbow(WordId::HAPPY, 1'000);
    const auto saturated = [](Rgb c) -> bool {
        const uint8_t mx = c.r > c.g ? (c.r > c.b ? c.r : c.b)
                                     : (c.g > c.b ? c.g : c.b);
        const uint8_t mn = c.r < c.g ? (c.r < c.b ? c.r : c.b)
                                     : (c.g < c.b ? c.g : c.b);
        return mx >= 250 && mn <= 5;
    };
    TEST_ASSERT_TRUE_MESSAGE(saturated(before_wrap),
        "pre-wrap color should be on the rainbow wheel (saturated)");
    TEST_ASSERT_TRUE_MESSAGE(saturated(after_wrap),
        "post-wrap color should be on the rainbow wheel (saturated)");
    // Also sanity-check they're not the same color — they're 2 s of
    // phase apart after accounting for UINT32_MAX % 60'000.
    const bool differs = (before_wrap.r != after_wrap.r) ||
                         (before_wrap.g != after_wrap.g) ||
                         (before_wrap.b != after_wrap.b);
    TEST_ASSERT_TRUE(differs);
}

// At the boundary where a naive `now_ms * 360` overflows uint32_t
// (UINT32_MAX / 360 ≈ 11'930'464), the correctly-implemented formula
// produces a stable hue with adjacent-input stability.
void test_rainbow_no_overflow_near_uint32_div_360(void) {
    const Rgb lo  = rainbow(WordId::HAPPY, 11'930'463);
    const Rgb mid = rainbow(WordId::HAPPY, 11'930'464);
    const Rgb hi  = rainbow(WordId::HAPPY, 11'930'465);
    const auto abs8 = [](uint8_t a, uint8_t b) -> uint8_t {
        return a > b ? static_cast<uint8_t>(a - b)
                     : static_cast<uint8_t>(b - a);
    };
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.r,  mid.r));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.g,  mid.g));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(lo.b,  mid.b));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.r, hi.r));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.g, hi.g));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(2, abs8(mid.b, hi.b));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rainbow_happy_at_t0_is_red);
    RUN_TEST(test_rainbow_birth_at_t0_is_yellow_green);
    RUN_TEST(test_rainbow_day_at_t0_is_cyan);
    RUN_TEST(test_rainbow_name_at_t0_is_purple);
    RUN_TEST(test_rainbow_period_is_60s);
    RUN_TEST(test_rainbow_wraps_across_uint32_max);
    RUN_TEST(test_rainbow_no_overflow_near_uint32_div_360);
    return UNITY_END();
}
