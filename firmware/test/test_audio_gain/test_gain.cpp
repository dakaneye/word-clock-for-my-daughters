#include <unity.h>
#include <climits>
#include "audio/gain.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

void test_gain_unity_256_identity(void) {
    int16_t buf[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    int16_t expected[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    apply_gain_q8(buf, 5, 256);
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT16(expected[i], buf[i]);
    }
}

void test_gain_half_128_halves(void) {
    int16_t buf[1] = { 32000 };
    apply_gain_q8(buf, 1, 128);
    TEST_ASSERT_EQUAL_INT16(16000, buf[0]);
}

void test_gain_zero_mutes(void) {
    int16_t buf[2] = { 32000, -32000 };
    apply_gain_q8(buf, 2, 0);
    TEST_ASSERT_EQUAL_INT16(0, buf[0]);
    TEST_ASSERT_EQUAL_INT16(0, buf[1]);
}

void test_gain_no_clipping_at_unity(void) {
    // Prove the multiply-and-shift path does not wrap at int16 boundaries.
    int16_t buf[2] = { INT16_MAX, INT16_MIN };
    apply_gain_q8(buf, 2, 256);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, buf[0]);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, buf[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_gain_unity_256_identity);
    RUN_TEST(test_gain_half_128_halves);
    RUN_TEST(test_gain_zero_mutes);
    RUN_TEST(test_gain_no_clipping_at_unity);
    return UNITY_END();
}
