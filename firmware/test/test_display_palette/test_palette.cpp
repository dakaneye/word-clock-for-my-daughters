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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_warm_white_rgb);
    RUN_TEST(test_amber_rgb);
    return UNITY_END();
}
