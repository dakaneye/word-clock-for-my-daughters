// firmware/test/test_display_renderer_golden/test_renderer_golden.cpp
#include <unity.h>
#include "display/led_map.h"
#include "display/renderer.h"
#include "time_to_words.h"
#include "word_id.h"

using namespace wc;
using namespace wc::display;

namespace {

bool word_in(WordId w, const WordSet& s) {
    for (uint8_t i = 0; i < s.count; ++i) if (s.words[i] == w) return true;
    return false;
}

bool is_decor(WordId w) {
    return w == WordId::HAPPY || w == WordId::BIRTH ||
           w == WordId::DAY   || w == WordId::NAME;
}

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

// Fixed input: Emory, 2030-10-06 14:23, fresh sync, now_ms=0.
// Every LED is classified by role; the full frame is asserted.
void test_golden_birthday_non_holiday_bright(void) {
    RenderInput in{};
    in.year   = 2030;
    in.month  = 10;
    in.day    = 6;
    in.hour   = 14;
    in.minute = 23;
    in.now_ms = 0;
    in.seconds_since_sync = 0;
    in.birthday = BirthdayConfig{10, 6, 18, 10};

    const Frame f = render(in);
    const WordSet lit = time_to_words(14, 23);

    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const Rgb c = f[index_of(w)];
        if (word_in(w, lit)) {
            // Time words: warm white exactly.
            TEST_ASSERT_EQUAL_UINT8(255, c.r);
            TEST_ASSERT_EQUAL_UINT8(170, c.g);
            TEST_ASSERT_EQUAL_UINT8(100, c.b);
        } else if (is_decor(w)) {
            // Decor words: rainbow family (some phase of 4 decor).
            TEST_ASSERT_TRUE_MESSAGE(is_rainbow_family(c),
                                     "decor word not rainbow-family");
        } else {
            // All other LEDs: black.
            TEST_ASSERT_EQUAL_UINT8(0, c.r);
            TEST_ASSERT_EQUAL_UINT8(0, c.g);
            TEST_ASSERT_EQUAL_UINT8(0, c.b);
        }
    }
}

// Three signals change simultaneously (palette boundary + birthday
// start + dim window entry). Verify the rendered frame is
// internally consistent with the priority table — no LED
// "half-transitioned" between two states. Regression guard against
// adding stateful caching.
void test_multi_signal_transition_is_atomic(void) {
    RenderInput in{};
    in.year   = 2030;
    in.month  = 11;        // Nov 1 — non-Halloween, non-holiday
    in.day    = 1;
    in.hour   = 0;         // dim window active
    in.minute = 0;
    in.now_ms = 0;
    in.seconds_since_sync = 0;
    in.birthday = BirthdayConfig{11, 1, 0, 0};  // synthetic Nov 1 birthday

    const Frame f = render(in);
    const WordSet lit = time_to_words(0, 0);

    for (uint8_t i = 0; i < static_cast<uint8_t>(WordId::COUNT); ++i) {
        const WordId w = static_cast<WordId>(i);
        const Rgb c = f[index_of(w)];
        if (word_in(w, lit)) {
            // Time words: warm white × dim = {26, 17, 10}.
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(26, c.r,
                "time word R should be warm-white × dim");
            TEST_ASSERT_EQUAL_UINT8(17, c.g);
            TEST_ASSERT_EQUAL_UINT8(10, c.b);
        } else if (is_decor(w)) {
            // Decor words: rainbow value at now_ms=0 × dim.
            // Rainbow produces saturated colors; dim scales by 26/255,
            // which keeps at-least-one channel non-zero. Assert not-black.
            const bool not_black = (c.r != 0) || (c.g != 0) || (c.b != 0);
            TEST_ASSERT_TRUE_MESSAGE(not_black,
                "decor word should be lit in dim × rainbow");
        } else {
            TEST_ASSERT_EQUAL_UINT8(0, c.r);
            TEST_ASSERT_EQUAL_UINT8(0, c.g);
            TEST_ASSERT_EQUAL_UINT8(0, c.b);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_golden_birthday_non_holiday_bright);
    RUN_TEST(test_multi_signal_transition_is_atomic);
    return UNITY_END();
}
