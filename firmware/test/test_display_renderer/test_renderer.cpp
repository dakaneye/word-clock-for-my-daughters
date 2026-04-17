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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unlit_word_is_black);
    RUN_TEST(test_warm_white_default);
    return UNITY_END();
}
